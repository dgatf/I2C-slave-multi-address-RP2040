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
 *  i2c_rx_queue.h — Lock-free single-producer / single-consumer ring buffer of
 *  complete I2C write transactions.
 *
 *  This module has NO dependency on PIO, IRQ, or any RP2040-specific hardware.
 *  It can be compiled and tested on a host machine.
 *
 *  Architecture
 *  ------------
 *  The queue holds I2C_MULTI_RX_QUEUE_DEPTH slots.  Each slot stores one
 *  complete received transaction (address + data bytes).
 *
 *  State machine per slot:
 *
 *    FREE ──begin()──▶ WRITING ──end()──▶ READY ──read()──▶ FREE
 *
 *  The producer (IRQ) is the only writer of FREE→WRITING and WRITING→READY.
 *  The consumer (main thread) is the only writer of READY→FREE.
 *  On RP2040 Cortex-M0+ this is safe without mutexes: the core executes
 *  memory operations in-order, and `volatile` prevents compiler reordering.
 *  The state field is the synchronisation point between producer and consumer.
 *
 *  write_head always points to the slot the producer will fill next.
 *  read_tail  always points to the oldest READY slot the consumer will read.
 *
 *  Overflow policy
 *  ---------------
 *  When a new transaction arrives and write_head slot is not FREE (queue
 *  full), the NEW transaction is dropped and the overflow flag is set.
 *  Rationale: the oldest data has already been buffered; discarding it
 *  would silently lose data the user was about to read.  The sticky
 *  overflow flag lets the application detect that data was lost without
 *  blocking the IRQ.
 *
 * -------------------------------------------------------------------------------
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ── Compile-time tunables ─────────────────────────────────────────────────── */

/** Number of transaction slots in the ring buffer. */
#ifndef I2C_MULTI_RX_QUEUE_DEPTH
#define I2C_MULTI_RX_QUEUE_DEPTH 8
#endif

/** Maximum data bytes stored per transaction (address byte not counted). */
#ifndef I2C_MULTI_RX_MAX_LEN
#define I2C_MULTI_RX_MAX_LEN 64
#endif

/* ── Public types ──────────────────────────────────────────────────────────── */

/**
 * A complete I2C write transaction delivered to the consumer.
 * Copying this struct out of the queue makes the slot immediately available
 * to the producer.
 */
typedef struct {
    uint8_t data[I2C_MULTI_RX_MAX_LEN]; /**< Received data bytes            */
    uint8_t length;                      /**< Number of valid bytes in data[] */
    uint8_t address;                     /**< 7-bit I2C address               */
} i2c_rx_transaction_t;

/* ── Internal types (not intended for direct use by application code) ──────── */

/** Life-cycle state of a single queue slot. */
typedef enum {
    I2C_RX_SLOT_FREE = 0, /**< Available for the producer to claim  */
    I2C_RX_SLOT_WRITING,  /**< Being filled by the IRQ producer     */
    I2C_RX_SLOT_READY     /**< Complete; waiting for the consumer   */
} i2c_rx_slot_state_t;

/** One entry in the ring buffer. */
typedef struct {
    uint8_t data[I2C_MULTI_RX_MAX_LEN];
    uint8_t length;
    uint8_t address;
    volatile i2c_rx_slot_state_t state;
} i2c_rx_slot_t;

/** The ring buffer. Declare one instance (static storage) in i2c_multi.c. */
typedef struct {
    i2c_rx_slot_t slots[I2C_MULTI_RX_QUEUE_DEPTH];
    volatile uint8_t write_head;  /**< Producer writes here next          */
    volatile uint8_t read_tail;   /**< Consumer reads from here next      */
    volatile bool in_overflow;    /**< Current incoming tx is being dropped */
    volatile bool overflow_flag;  /**< Sticky: at least one tx was dropped  */
} i2c_rx_queue_t;

/* ── Queue operations ──────────────────────────────────────────────────────── */

/**
 * Initialise (or re-initialise) all slots to FREE and reset all indices.
 * Call once during setup and again on a full reset.
 */
static inline void i2c_rx_queue_init(i2c_rx_queue_t *q) {
    for (int i = 0; i < I2C_MULTI_RX_QUEUE_DEPTH; i++) {
        q->slots[i].state = I2C_RX_SLOT_FREE;
        q->slots[i].length = 0;
        q->slots[i].address = 0;
    }
    q->write_head = 0;
    q->read_tail = 0;
    q->in_overflow = false;
    q->overflow_flag = false;
}

/**
 * Called from IRQ at the start of a master-write transaction (address byte).
 *
 * Claims write_head slot if FREE, stores the 7-bit address, and marks the slot
 * WRITING.  If the slot is not FREE the transaction is dropped and the overflow
 * flag is set.
 *
 * @param q        Queue instance.
 * @param address  7-bit I2C address (address byte >> 1).
 * @return true if a slot was claimed; false on overflow (transaction dropped).
 */
static inline bool i2c_rx_queue_begin(i2c_rx_queue_t *q, uint8_t address) {
    uint8_t head = q->write_head;
    if (q->slots[head].state != I2C_RX_SLOT_FREE) {
        q->overflow_flag = true;
        q->in_overflow = true;
        return false;
    }
    q->slots[head].address = address;
    q->slots[head].length = 0;
    q->slots[head].state = I2C_RX_SLOT_WRITING;
    q->in_overflow = false;
    return true;
}

/**
 * Called from IRQ for each data byte belonging to the current transaction.
 * A no-op when in overflow mode or when the slot storage is exhausted
 * (bytes beyond I2C_MULTI_RX_MAX_LEN are silently dropped; the slot itself
 * is still published with the first I2C_MULTI_RX_MAX_LEN bytes intact).
 *
 * @param q     Queue instance.
 * @param byte  Received data byte.
 */
static inline void i2c_rx_queue_append(i2c_rx_queue_t *q, uint8_t byte) {
    if (q->in_overflow) return;
    uint8_t head = q->write_head;
    if (q->slots[head].length < I2C_MULTI_RX_MAX_LEN) {
        q->slots[head].data[q->slots[head].length++] = byte;
    }
}

/**
 * Called from IRQ when the current transaction ends (STOP or repeated START).
 *
 * If not in overflow: marks the slot READY and advances write_head.
 * If in overflow: clears the flag and returns -1.
 *
 * The state transition WRITING→READY is the synchronisation point between
 * the producer and consumer.
 *
 * @param q  Queue instance.
 * @return   Number of data bytes in the completed transaction (0..255), or
 *           -1 if the transaction was dropped due to overflow.
 */
static inline int16_t i2c_rx_queue_end(i2c_rx_queue_t *q) {
    if (q->in_overflow) {
        q->in_overflow = false;
        return -1;
    }
    uint8_t head = q->write_head;
    if (q->slots[head].state != I2C_RX_SLOT_WRITING) {
        return -1; /* no transaction in progress (e.g. STOP after WRITE-to-master) */
    }
    int16_t len = (int16_t)q->slots[head].length;
    /* Publish: state write is the memory barrier for the consumer. */
    q->slots[head].state = I2C_RX_SLOT_READY;
    q->write_head = (uint8_t)((head + 1u) % I2C_MULTI_RX_QUEUE_DEPTH);
    return len;
}

/**
 * Consumer: returns true if at least one complete transaction is available.
 * Safe to call from main thread without disabling IRQs.
 */
static inline bool i2c_rx_queue_available(const i2c_rx_queue_t *q) {
    return q->slots[q->read_tail].state == I2C_RX_SLOT_READY;
}

/**
 * Consumer: copies the oldest READY transaction into *out and frees the slot.
 *
 * The state transition READY→FREE is the synchronisation point between the
 * consumer and producer.
 *
 * @param q    Queue instance.
 * @param out  Destination struct (must not be NULL).
 * @return     true on success; false if the queue was empty.
 */
static inline bool i2c_rx_queue_read(i2c_rx_queue_t *q, i2c_rx_transaction_t *out) {
    uint8_t tail = q->read_tail;
    if (q->slots[tail].state != I2C_RX_SLOT_READY) {
        return false;
    }
    out->length = q->slots[tail].length;
    out->address = q->slots[tail].address;
    memcpy(out->data, q->slots[tail].data, out->length);
    /* Release: state write must happen after data is copied. */
    q->slots[tail].state = I2C_RX_SLOT_FREE;
    q->read_tail = (uint8_t)((tail + 1u) % I2C_MULTI_RX_QUEUE_DEPTH);
    return true;
}

/**
 * Consumer: returns the current value of the overflow flag and clears it.
 * A true return means at least one transaction was silently dropped since the
 * last call to this function.
 */
static inline bool i2c_rx_queue_overflow(i2c_rx_queue_t *q) {
    bool f = q->overflow_flag;
    q->overflow_flag = false;
    return f;
}

/**
 * Abort the transaction currently being written (if any) without publishing it.
 * Call from i2c_multi_disable() / i2c_multi_restart() to ensure a clean state
 * after a forced reset.  Does NOT touch READY slots already in the queue.
 */
static inline void i2c_rx_queue_abort_current(i2c_rx_queue_t *q) {
    uint8_t head = q->write_head;
    if (q->slots[head].state == I2C_RX_SLOT_WRITING) {
        q->slots[head].state = I2C_RX_SLOT_FREE;
        q->slots[head].length = 0;
    }
    q->in_overflow = false;
}
