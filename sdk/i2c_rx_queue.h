/**
 * -------------------------------------------------------------------------------
 *
 * Copyright (c) 2022, Daniel Gorbea
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * -------------------------------------------------------------------------------
 *
 *  i2c_rx_queue.h — Shared-byte-ring + descriptor-FIFO receive queue for
 *  complete I2C write transactions.
 *
 *  This module has NO dependency on PIO, IRQ, or any RP2040-specific hardware.
 *  It can be compiled and tested on a host machine.
 *
 * ── Architecture ──────────────────────────────────────────────────────────────
 *
 *  Two independent resources are managed:
 *
 *  1. Byte ring  (i2c_rx_queue_t::ring[])
 *     A single circular byte array shared by ALL in-flight and buffered
 *     transactions.  Bytes from different transactions are packed adjacently;
 *     no per-transaction MAX_LEN padding exists.
 *
 *       ring indices (all modulo I2C_MULTI_RX_BUFFER_SIZE):
 *         byte_write  — next byte the producer will write
 *         byte_read   — oldest byte not yet freed by the consumer
 *
 *       Bytes between [byte_read, byte_write) are "live" (either being
 *       written or waiting to be consumed).  Bytes between [byte_write,
 *       byte_read) are free.
 *
 *       A transaction that crosses the physical end of ring[] wraps around
 *       seamlessly because all index arithmetic is done modulo ring size.
 *
 *  2. Descriptor FIFO  (i2c_rx_queue_t::desc[])
 *     A ring array of I2C_MULTI_RX_QUEUE_DEPTH small descriptor structs.
 *     Each descriptor stores metadata for one complete transaction:
 *         start   — byte_ring offset where the transaction begins
 *         length  — number of data bytes
 *         address — 7-bit I2C address
 *
 *       desc_write  — next descriptor the producer will fill
 *       desc_read   — oldest descriptor not yet freed by the consumer
 *
 * ── Memory layout ─────────────────────────────────────────────────────────────
 *
 *  Default (I2C_MULTI_RX_BUFFER_SIZE=256, I2C_MULTI_RX_QUEUE_DEPTH=8):
 *
 *    ring[]     : 256 bytes
 *    desc[]     : 8 × (2 + 1 + 2 + pad) ≈ 8 × 6 = ~48 bytes
 *    Indices+flags: ~8 bytes
 *    Total      : ~312 bytes
 *
 *  Compare with the old per-slot design (DEPTH=8, MAX_LEN=64):
 *    8 × (64 + 1 + 1 + 1 + pad) ≈ 8 × 68 = ~544 bytes
 *
 * ── Overflow policy ───────────────────────────────────────────────────────────
 *
 *  "Drop newest" — if the ring has insufficient free bytes OR no free
 *  descriptor is available, the *new* transaction is dropped and a sticky
 *  overflow flag is set.  Existing buffered transactions are never touched.
 *
 *  Two distinct overflow causes can occur independently:
 *    - byte ring full  (not enough contiguous bytes available)
 *    - descriptor FIFO full (no free descriptor slots)
 *
 *  Both result in the current transaction being dropped and the overflow flag
 *  being set.  The IRQ never blocks waiting for space.
 *
 * ── Concurrency ───────────────────────────────────────────────────────────────
 *
 *  Single-producer (IRQ), single-consumer (main thread), same core.
 *
 *  On RP2040 Cortex-M0+:
 *    - The processor executes memory instructions in order.
 *    - IRQ preempts main thread; the reverse never happens within one core.
 *    - `volatile` prevents the *compiler* from reordering or eliding reads
 *      and writes to the marked fields.
 *    - The `volatile` qualifier alone does NOT provide the hardware memory
 *      barriers that would be required for multi-core safety.
 *    - For correct single-core SPSC behaviour the critical ordering
 *      requirements are:
 *        * Producer: byte writes complete BEFORE the descriptor is published
 *          (desc_write advance).
 *        * Consumer: data read completes BEFORE the byte ring head is freed
 *          (byte_read advance).
 *    - On Cortex-M0+ this ordering is guaranteed by the sequential execution
 *      model.  No explicit DSB/DMB is required for single-core use.
 *
 *  *** If the consumer is ever moved to a different core (RP2040 core 1),
 *      explicit memory barriers (DSB/DMB) must be added around the publish
 *      and release points. ***
 *
 * ── Overflow flag race ────────────────────────────────────────────────────────
 *
 *  i2c_rx_queue_overflow() performs a read-and-clear atomically from the
 *  consumer's perspective: because the IRQ only *sets* overflow_flag (never
 *  clears it), the consumer can safely read the flag and clear it without
 *  disabling interrupts.  If the IRQ fires between the consumer's read and
 *  clear, the IRQ will set the flag again and the consumer will see it on the
 *  next call — no event is lost.  The consumer never sets the flag, so the
 *  IRQ's write to overflow_flag is never clobbered by the clear.
 *
 * ── Compile-time checks ───────────────────────────────────────────────────────
 *
 *  I2C_MULTI_RX_BUFFER_SIZE must be a power of two (≤ 32768) so that modulo
 *  can be done with a bitmask.
 *
 *  The `length` field in a descriptor must be wide enough to hold
 *  I2C_MULTI_RX_BUFFER_SIZE.  uint16_t covers up to 65535 bytes.
 *
 * -------------------------------------------------------------------------------
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ── Compile-time tunables ─────────────────────────────────────────────────── */

/**
 * Total size of the shared RX byte ring in bytes.
 * Must be a power of two, range [2, 32768].
 * Default: 256 bytes.
 */
#ifndef I2C_MULTI_RX_BUFFER_SIZE
#define I2C_MULTI_RX_BUFFER_SIZE 256u
#endif

/**
 * Maximum number of pending (buffered but not yet consumed) transactions.
 * Must be a power of two, range [2, 128].
 * Default: 8 transactions.
 */
#ifndef I2C_MULTI_RX_QUEUE_DEPTH
#define I2C_MULTI_RX_QUEUE_DEPTH 8u
#endif

/* ── Compile-time sanity checks ────────────────────────────────────────────── */

/* Power-of-two checks using classic bitmask trick. */
#if (I2C_MULTI_RX_BUFFER_SIZE == 0) || (I2C_MULTI_RX_BUFFER_SIZE & (I2C_MULTI_RX_BUFFER_SIZE - 1u))
#error "I2C_MULTI_RX_BUFFER_SIZE must be a power of two"
#endif
#if I2C_MULTI_RX_BUFFER_SIZE > 32768u
#error "I2C_MULTI_RX_BUFFER_SIZE must not exceed 32768"
#endif

#if (I2C_MULTI_RX_QUEUE_DEPTH == 0) || (I2C_MULTI_RX_QUEUE_DEPTH & (I2C_MULTI_RX_QUEUE_DEPTH - 1u))
#error "I2C_MULTI_RX_QUEUE_DEPTH must be a power of two"
#endif
#if I2C_MULTI_RX_QUEUE_DEPTH > 128u
#error "I2C_MULTI_RX_QUEUE_DEPTH must not exceed 128"
#endif

/* ── Index type ──────────────────────────────────────────────────────────────
 * Use uint16_t for byte-ring indices so they can represent values up to
 * I2C_MULTI_RX_BUFFER_SIZE (max 32768) without overflow.
 * Use uint8_t for descriptor indices (max depth 128).
 */
typedef uint16_t i2c_rx_bidx_t;  /* byte-ring index type */
typedef uint8_t  i2c_rx_didx_t;  /* descriptor index type */

/* Bitmasks for power-of-two modulo */
#define I2C_RX_RING_MASK  ((i2c_rx_bidx_t)(I2C_MULTI_RX_BUFFER_SIZE - 1u))
#define I2C_RX_DESC_MASK  ((i2c_rx_didx_t)(I2C_MULTI_RX_QUEUE_DEPTH - 1u))

/* ── Public types ──────────────────────────────────────────────────────────── */

/**
 * A complete I2C write transaction delivered to the consumer.
 *
 * `data` points into a caller-supplied buffer; `length` bytes are valid.
 * The struct itself is small — it carries no embedded byte storage.
 *
 * Lifetime: the pointed-to data is valid only as long as the caller-supplied
 * buffer remains in scope (typically a stack buffer provided to
 * i2c_multi_rx_read()).
 */
typedef struct {
    const uint8_t *data;    /**< Pointer to received bytes (caller's buffer)  */
    uint16_t       length;  /**< Number of valid bytes                         */
    uint8_t        address; /**< 7-bit I2C address                             */
} i2c_rx_transaction_t;

/* ── Internal: transaction descriptor ─────────────────────────────────────── */

/**
 * Metadata for one complete transaction stored inside the queue.
 * No byte storage here — bytes live in the shared ring.
 */
typedef struct {
    i2c_rx_bidx_t start;   /**< Ring index of the first byte of this transaction */
    uint16_t      length;  /**< Number of data bytes (0 = address-only)          */
    uint8_t       address; /**< 7-bit I2C address                                 */
} i2c_rx_desc_t;

/* ── Queue structure ───────────────────────────────────────────────────────── */

typedef struct {
    /* Shared byte ring — all transaction bytes packed here. */
    uint8_t ring[I2C_MULTI_RX_BUFFER_SIZE];

    /* Descriptor FIFO — one entry per complete transaction. */
    i2c_rx_desc_t desc[I2C_MULTI_RX_QUEUE_DEPTH];

    /*
     * Byte-ring indices (producer / consumer).
     * byte_write: next position the producer will write into.
     * byte_read:  oldest byte still live (oldest transaction start).
     *
     * Free bytes  = I2C_MULTI_RX_BUFFER_SIZE - (byte_write - byte_read) mod SIZE
     * (using unsigned arithmetic; both indices advance monotonically and wrap).
     */
    volatile i2c_rx_bidx_t byte_write; /* producer-owned */
    volatile i2c_rx_bidx_t byte_read;  /* consumer-owned */

    /* Byte-ring position of the first byte of the transaction currently being
       assembled by the producer.  Valid only when in_transaction == true. */
    i2c_rx_bidx_t current_start;
    uint16_t      current_length; /* bytes appended so far in the current tx */

    /*
     * Descriptor FIFO indices.
     * desc_write: next descriptor slot the producer will fill.
     * desc_read:  oldest descriptor not yet consumed.
     */
    volatile i2c_rx_didx_t desc_write; /* producer-owned */
    volatile i2c_rx_didx_t desc_read;  /* consumer-owned */

    /* Per-transaction state flags (producer-only). */
    bool in_transaction; /**< A transaction is currently being assembled */
    bool in_overflow;    /**< Current tx is being dropped (overflow mode) */

    /**
     * Sticky overflow flag.
     * Set by the IRQ producer; read-and-cleared by the consumer via
     * i2c_rx_queue_overflow().
     *
     * Race safety: the IRQ only *sets* this flag; the consumer only *reads*
     * and then *clears* it.  On a single core the consumer's clear cannot
     * interleave with the IRQ's set in a harmful way (see header comments).
     */
    volatile bool overflow_flag;
} i2c_rx_queue_t;

/* ── Helper: available bytes in ring ──────────────────────────────────────── */

/**
 * Number of bytes currently free in the byte ring.
 * Inline so the compiler can optimise it in tight loops.
 */
static inline uint16_t i2c_rx_ring_free(const i2c_rx_queue_t *q) {
    /* Both indices are always in [0, SIZE).  Subtraction is done in the
       uint16_t domain; the cast handles the modulo arithmetic. */
    uint16_t used = (uint16_t)(q->byte_write - q->byte_read);
    return (uint16_t)(I2C_MULTI_RX_BUFFER_SIZE - used);
}

/** Number of free descriptor slots in the FIFO. */
static inline uint8_t i2c_rx_desc_free(const i2c_rx_queue_t *q) {
    uint8_t used = (uint8_t)(q->desc_write - q->desc_read);
    return (uint8_t)(I2C_MULTI_RX_QUEUE_DEPTH - used);
}

/* ── Queue operations ──────────────────────────────────────────────────────── */

/**
 * Initialise (or re-initialise) the queue.
 * Safe to call at any time from the same core (not IRQ-safe during normal
 * operation — call only when no IRQ can fire, e.g. during init or restart).
 */
static inline void i2c_rx_queue_init(i2c_rx_queue_t *q) {
    q->byte_write    = 0;
    q->byte_read     = 0;
    q->desc_write    = 0;
    q->desc_read     = 0;
    q->current_start  = 0;
    q->current_length = 0;
    q->in_transaction = false;
    q->in_overflow    = false;
    q->overflow_flag  = false;
}

/**
 * Called from IRQ at the start of a master-write transaction (address byte).
 *
 * Checks whether there is at least one free descriptor slot.  The byte ring
 * space is checked lazily per-byte in i2c_rx_queue_append().  If no
 * descriptor is available the transaction is dropped immediately and
 * overflow is flagged.
 *
 * @param q        Queue instance.
 * @param address  7-bit I2C address (raw address byte >> 1).
 */
static inline void i2c_rx_queue_begin(i2c_rx_queue_t *q, uint8_t address) {
    if (i2c_rx_desc_free(q) == 0) {
        /* No descriptor available — drop this transaction. */
        q->overflow_flag  = true;
        q->in_overflow    = true;
        q->in_transaction = false;
        return;
    }
    /* Record the starting ring position and open a new transaction. */
    q->current_start  = q->byte_write;
    q->current_length = 0;
    q->in_transaction = true;
    q->in_overflow    = false;
    /* Store address temporarily in the descriptor slot we will claim on end(). */
    q->desc[q->desc_write & I2C_RX_DESC_MASK].address = address;
    q->desc[q->desc_write & I2C_RX_DESC_MASK].start   = q->current_start;
}

/**
 * Called from IRQ for each data byte belonging to the current transaction.
 *
 * If the byte ring is full, the transaction is aborted (remaining bytes
 * discarded), the bytes already written for this transaction are reclaimed,
 * and overflow is flagged.  The in-progress transaction is not published.
 *
 * @param q     Queue instance.
 * @param byte  Received data byte.
 */
static inline void i2c_rx_queue_append(i2c_rx_queue_t *q, uint8_t byte) {
    if (q->in_overflow || !q->in_transaction) return;

    if (i2c_rx_ring_free(q) == 0) {
        /*
         * Ring is full.  Reclaim the bytes already written for this
         * transaction by rolling back byte_write to current_start.
         * This is safe because those bytes were not yet visible to the
         * consumer (no descriptor published for them).
         */
        q->byte_write    = q->current_start;
        q->overflow_flag = true;
        q->in_overflow   = true;
        q->in_transaction = false;
        return;
    }

    q->ring[q->byte_write & I2C_RX_RING_MASK] = byte;
    q->byte_write = (i2c_rx_bidx_t)(q->byte_write + 1u);
    q->current_length++;
}

/**
 * Called from IRQ when the current transaction ends (STOP or repeated START).
 *
 * If not in overflow: publishes a descriptor and advances desc_write.
 *                      byte_write is already at the correct position.
 * If in overflow: clears overflow state; byte_write was already rolled back.
 *
 * @param q  Queue instance.
 * @return   Number of data bytes in the published transaction (≥ 0), or
 *           -1 if the transaction was dropped due to overflow.
 */
static inline int32_t i2c_rx_queue_end(i2c_rx_queue_t *q) {
    if (q->in_overflow) {
        q->in_overflow    = false;
        q->in_transaction = false;
        return -1;
    }
    if (!q->in_transaction) {
        /* No transaction in progress (e.g. STOP after a master-read). */
        return -1;
    }
    /* Fill the descriptor (start and address were stored in begin()). */
    i2c_rx_didx_t didx = q->desc_write & I2C_RX_DESC_MASK;
    q->desc[didx].length = q->current_length;
    q->in_transaction = false;

    /*
     * Publish: advance desc_write AFTER all descriptor fields are written.
     * On single-core Cortex-M0+ the sequential execution model guarantees
     * the consumer sees a complete descriptor once it observes the updated
     * desc_write.
     */
    q->desc_write = (i2c_rx_didx_t)(q->desc_write + 1u);

    return (int32_t)q->current_length;
}

/**
 * Consumer: returns true if at least one complete transaction is available.
 */
static inline bool i2c_rx_queue_available(const i2c_rx_queue_t *q) {
    return (uint8_t)(q->desc_write - q->desc_read) > 0u;
}

/**
 * Consumer: copies the oldest complete transaction's bytes into @p buf and
 * fills @p out with a pointer, length, and address.
 *
 * @p buf must be at least as large as the transaction.  Call
 * i2c_rx_queue_peek_length() first if you need to know the size in advance.
 *
 * The byte-ring region used by the transaction is freed atomically once the
 * copy is complete (byte_read advances), making the space immediately
 * available to the producer.
 *
 * @param q    Queue instance.
 * @param out  Destination struct (must not be NULL).
 * @param buf  Caller-supplied buffer for the transaction bytes.
 * @param buf_size  Size of buf in bytes.  The function returns false without
 *                  consuming the descriptor if the transaction does not fit.
 * @return     true on success; false if queue is empty or buf is too small.
 */
static inline bool i2c_rx_queue_read(i2c_rx_queue_t *q, i2c_rx_transaction_t *out,
                                     uint8_t *buf, uint16_t buf_size) {
    if ((uint8_t)(q->desc_write - q->desc_read) == 0u) {
        return false; /* nothing available */
    }
    i2c_rx_didx_t didx = q->desc_read & I2C_RX_DESC_MASK;
    uint16_t      len  = q->desc[didx].length;

    if (len > buf_size) {
        /* Caller buffer too small — do not consume the descriptor. */
        return false;
    }

    /* Copy bytes out of the ring, handling wrap-around. */
    i2c_rx_bidx_t src = q->desc[didx].start;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = q->ring[(src + i) & I2C_RX_RING_MASK];
    }

    out->data    = buf;
    out->length  = len;
    out->address = q->desc[didx].address;

    /*
     * Release: advance byte_read to free the ring region, then advance
     * desc_read to free the descriptor slot.
     * Order matters: byte_read must be updated before (or simultaneously
     * with) desc_read from the producer's perspective — but since both
     * happen in the consumer and the producer only reads byte_read, either
     * order is safe here.
     */
    q->byte_read  = (i2c_rx_bidx_t)(q->byte_read + (i2c_rx_bidx_t)len);
    q->desc_read  = (i2c_rx_didx_t)(q->desc_read + 1u);

    return true;
}

/**
 * Consumer: returns the length of the next pending transaction without
 * consuming it.  Returns 0 if no transaction is available.
 */
static inline uint16_t i2c_rx_queue_peek_length(const i2c_rx_queue_t *q) {
    if ((uint8_t)(q->desc_write - q->desc_read) == 0u) {
        return 0u;
    }
    return q->desc[q->desc_read & I2C_RX_DESC_MASK].length;
}

/**
 * Consumer: reads and clears the overflow flag.
 *
 * Returns true if at least one transaction has been dropped since the last
 * call.  The flag is safe to read-and-clear without disabling interrupts on
 * a single core: the IRQ only ever *sets* the flag; the consumer only ever
 * *reads* then *clears* it.  Even if the IRQ fires between the read and the
 * clear, it will set the flag again and the event will be seen on the next
 * call — no information is lost.
 */
static inline bool i2c_rx_queue_overflow(i2c_rx_queue_t *q) {
    bool f = q->overflow_flag;
    if (f) q->overflow_flag = false;
    return f;
}

/**
 * Abort the transaction currently being assembled (if any) without publishing
 * it.  Rolls back byte_write to reclaim any bytes already written.
 * Does NOT touch completed (desc-published) transactions.
 *
 * Call from i2c_multi_disable() / i2c_multi_restart() to restore a clean
 * state after a forced reset.  Not IRQ-safe — call only when the IRQ is
 * disabled.
 */
static inline void i2c_rx_queue_abort_current(i2c_rx_queue_t *q) {
    if (q->in_transaction) {
        /* Roll back bytes written for the current (unpublished) transaction. */
        q->byte_write = q->current_start;
    }
    q->in_transaction = false;
    q->in_overflow    = false;
}
