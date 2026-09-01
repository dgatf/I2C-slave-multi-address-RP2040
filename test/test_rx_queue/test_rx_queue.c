/**
 * test_rx_queue.c — Host-side unit tests for the i2c_rx_queue ring buffer.
 *
 * Build and run on a POSIX host:
 *   gcc -std=c11 -Wall -Wextra -I../../sdk -o test_rx_queue test_rx_queue.c && ./test_rx_queue
 *
 * The queue module (i2c_rx_queue.h) has no hardware dependencies; it compiles
 * and runs on any platform that provides <stdint.h>, <stdbool.h>, and <string.h>.
 *
 * Test coverage:
 *  1.  Single RX transaction.
 *  2.  Multiple consecutive transactions (3 back-to-back before first dequeue).
 *  3.  Back-to-back transactions buffered before consumer reads any.
 *  4.  Transactions of different lengths.
 *  5.  Slot reuse after dequeue (wrap-around sequence).
 *  6.  Ring buffer wrap-around past slot 0.
 *  7.  Queue full / overflow detection.
 *  8.  Repeated START: end() followed immediately by begin() on a new address.
 *  9.  Different I2C addresses stored per transaction.
 * 10.  Data does not change after enqueue (published data is immutable).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Adjust queue depth to a small value to exercise wrap-around and overflow
   without an unreasonably long test.  These override the defaults from the
   header when defined before the include. */
#define I2C_MULTI_RX_QUEUE_DEPTH 4
#define I2C_MULTI_RX_MAX_LEN     16

#include "i2c_rx_queue.h"

/* ── Helper: simulate IRQ sending one complete transaction ─────────────────── */

static void irq_send(i2c_rx_queue_t *q, uint8_t address, const uint8_t *data, uint8_t len) {
    i2c_rx_queue_begin(q, address);
    for (uint8_t i = 0; i < len; i++) {
        i2c_rx_queue_append(q, data[i]);
    }
    i2c_rx_queue_end(q);
}

/* ── Minimal assertion wrapper ─────────────────────────────────────────────── */

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        tests_run++;                                                             \
        if (cond) {                                                              \
            tests_passed++;                                                      \
        } else {                                                                 \
            printf("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                        \
    } while (0)

/* ── Test 1: single transaction ───────────────────────────────────────────── */

static void test_1_single_transaction(void) {
    printf("Test 1: single transaction\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    irq_send(&q, 0x70, data, 4);

    CHECK(i2c_rx_queue_available(&q));

    i2c_rx_transaction_t rx;
    CHECK(i2c_rx_queue_read(&q, &rx));
    CHECK(rx.address == 0x70);
    CHECK(rx.length == 4);
    CHECK(rx.data[0] == 0x01);
    CHECK(rx.data[3] == 0x04);

    /* Queue is now empty */
    CHECK(!i2c_rx_queue_available(&q));
    CHECK(!i2c_rx_queue_read(&q, &rx));
}

/* ── Test 2: multiple consecutive transactions ────────────────────────────── */

static void test_2_multiple_consecutive(void) {
    printf("Test 2: multiple consecutive transactions\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    const uint8_t d1[] = {0xAA, 0xBB};
    const uint8_t d2[] = {0x11, 0x22, 0x33};

    irq_send(&q, 0x70, d1, 2);
    irq_send(&q, 0x71, d2, 3);

    CHECK(i2c_rx_queue_available(&q));

    i2c_rx_transaction_t rx;
    /* First transaction */
    CHECK(i2c_rx_queue_read(&q, &rx));
    CHECK(rx.address == 0x70 && rx.length == 2);
    CHECK(rx.data[0] == 0xAA && rx.data[1] == 0xBB);

    /* Second transaction */
    CHECK(i2c_rx_queue_available(&q));
    CHECK(i2c_rx_queue_read(&q, &rx));
    CHECK(rx.address == 0x71 && rx.length == 3);
    CHECK(rx.data[0] == 0x11 && rx.data[2] == 0x33);

    CHECK(!i2c_rx_queue_available(&q));
}

/* ── Test 3: 3 back-to-back transactions before consumer reads any ─────────── */

static void test_3_back_to_back(void) {
    printf("Test 3: 3 back-to-back transactions before consumer dequeues\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    /* With DEPTH=4 we can buffer 3 transactions comfortably */
    const uint8_t d1[] = {0x01};
    const uint8_t d2[] = {0x02, 0x03};
    const uint8_t d3[] = {0x04, 0x05, 0x06};

    irq_send(&q, 0x70, d1, 1);
    irq_send(&q, 0x70, d2, 2);
    irq_send(&q, 0x70, d3, 3);

    /* None consumed yet; all should be available */
    i2c_rx_transaction_t rx;

    CHECK(i2c_rx_queue_read(&q, &rx) && rx.length == 1 && rx.data[0] == 0x01);
    CHECK(i2c_rx_queue_read(&q, &rx) && rx.length == 2 && rx.data[0] == 0x02);
    CHECK(i2c_rx_queue_read(&q, &rx) && rx.length == 3 && rx.data[2] == 0x06);
    CHECK(!i2c_rx_queue_available(&q));
}

/* ── Test 4: transactions of different lengths ────────────────────────────── */

static void test_4_different_lengths(void) {
    printf("Test 4: transactions of different lengths\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    const uint8_t empty[] = {0};
    const uint8_t full[I2C_MULTI_RX_MAX_LEN];   /* zero-initialised */
    uint8_t max_data[I2C_MULTI_RX_MAX_LEN];
    for (int i = 0; i < I2C_MULTI_RX_MAX_LEN; i++) max_data[i] = (uint8_t)(i + 1);

    /* Zero-length transaction (address-only) */
    i2c_rx_queue_begin(&q, 0x70);
    i2c_rx_queue_end(&q);

    /* Max-length transaction */
    i2c_rx_queue_begin(&q, 0x71);
    for (int i = 0; i < I2C_MULTI_RX_MAX_LEN; i++) i2c_rx_queue_append(&q, max_data[i]);
    i2c_rx_queue_end(&q);

    (void)empty; (void)full;

    i2c_rx_transaction_t rx;
    CHECK(i2c_rx_queue_read(&q, &rx) && rx.length == 0 && rx.address == 0x70);
    CHECK(i2c_rx_queue_read(&q, &rx) && rx.length == I2C_MULTI_RX_MAX_LEN && rx.address == 0x71);
    CHECK(rx.data[0] == 1 && rx.data[I2C_MULTI_RX_MAX_LEN - 1] == I2C_MULTI_RX_MAX_LEN);
    CHECK(!i2c_rx_queue_available(&q));
}

/* ── Test 5: slot reuse after dequeue ─────────────────────────────────────── */

static void test_5_slot_reuse(void) {
    printf("Test 5: slot reuse after dequeue\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    i2c_rx_transaction_t rx;
    uint8_t data[1];

    /* Send DEPTH+2 transactions sequentially, dequeuing each one before
       sending the next: slots must be recycled correctly. */
    for (int round = 0; round < (int)I2C_MULTI_RX_QUEUE_DEPTH + 2; round++) {
        data[0] = (uint8_t)round;
        irq_send(&q, (uint8_t)(0x70 + (round % 2)), data, 1);
        CHECK(i2c_rx_queue_available(&q));
        CHECK(i2c_rx_queue_read(&q, &rx) && rx.data[0] == (uint8_t)round);
        CHECK(!i2c_rx_queue_available(&q));
        CHECK(!i2c_rx_queue_overflow(&q));
    }
}

/* ── Test 6: ring-buffer wrap-around ─────────────────────────────────────── */

static void test_6_wrap_around(void) {
    printf("Test 6: ring-buffer wrap-around\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    i2c_rx_transaction_t rx;

    /* Fill queue to DEPTH-1 slots (leave one free), drain, repeat twice so
       write_head and read_tail wrap past slot 0. */
    for (int pass = 0; pass < 3; pass++) {
        uint8_t d[2];
        for (int i = 0; i < (int)I2C_MULTI_RX_QUEUE_DEPTH - 1; i++) {
            d[0] = (uint8_t)(pass * 10 + i);
            d[1] = (uint8_t)(pass * 10 + i + 100);
            irq_send(&q, 0x70, d, 2);
        }
        for (int i = 0; i < (int)I2C_MULTI_RX_QUEUE_DEPTH - 1; i++) {
            CHECK(i2c_rx_queue_read(&q, &rx) && rx.length == 2);
            CHECK(rx.data[0] == (uint8_t)(pass * 10 + i));
        }
    }
    CHECK(!i2c_rx_queue_available(&q));
    CHECK(!i2c_rx_queue_overflow(&q));
}

/* ── Test 7: queue full / overflow ───────────────────────────────────────── */

static void test_7_overflow(void) {
    printf("Test 7: queue full / overflow\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    uint8_t data[1] = {0xAA};

    /* Fill all DEPTH slots */
    for (int i = 0; i < (int)I2C_MULTI_RX_QUEUE_DEPTH; i++) {
        irq_send(&q, 0x70, data, 1);
    }

    /* Queue is full; no overflow yet */
    CHECK(!i2c_rx_queue_overflow(&q));

    /* One more transaction should trigger overflow */
    data[0] = 0xFF;
    irq_send(&q, 0x71, data, 1);
    CHECK(i2c_rx_queue_overflow(&q));
    /* Flag cleared by the read above */
    CHECK(!i2c_rx_queue_overflow(&q));

    /* The DEPTH slots already in the queue are intact */
    i2c_rx_transaction_t rx;
    for (int i = 0; i < (int)I2C_MULTI_RX_QUEUE_DEPTH; i++) {
        CHECK(i2c_rx_queue_read(&q, &rx));
        CHECK(rx.address == 0x70 && rx.data[0] == 0xAA);
    }
    /* The overflowed transaction (0xFF) was never enqueued */
    CHECK(!i2c_rx_queue_available(&q));
}

/* ── Test 8: repeated START (end then begin) ──────────────────────────────── */

static void test_8_repeated_start(void) {
    printf("Test 8: repeated START\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    /* First transaction: 2 bytes */
    i2c_rx_queue_begin(&q, 0x70);
    i2c_rx_queue_append(&q, 0x01);
    i2c_rx_queue_append(&q, 0x02);
    /* Repeated START: close first, open second immediately */
    i2c_rx_queue_end(&q);
    i2c_rx_queue_begin(&q, 0x70);
    i2c_rx_queue_append(&q, 0x03);
    i2c_rx_queue_append(&q, 0x04);
    i2c_rx_queue_end(&q);

    i2c_rx_transaction_t rx;
    CHECK(i2c_rx_queue_read(&q, &rx));
    CHECK(rx.length == 2 && rx.data[0] == 0x01 && rx.data[1] == 0x02);

    CHECK(i2c_rx_queue_read(&q, &rx));
    CHECK(rx.length == 2 && rx.data[0] == 0x03 && rx.data[1] == 0x04);

    CHECK(!i2c_rx_queue_available(&q));
    CHECK(!i2c_rx_queue_overflow(&q));
}

/* ── Test 9: different I2C addresses ──────────────────────────────────────── */

static void test_9_different_addresses(void) {
    printf("Test 9: different I2C addresses\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    const uint8_t addresses[] = {0x10, 0x20, 0x30};
    uint8_t data[1];

    for (int i = 0; i < 3; i++) {
        data[0] = addresses[i];
        irq_send(&q, addresses[i], data, 1);
    }

    i2c_rx_transaction_t rx;
    for (int i = 0; i < 3; i++) {
        CHECK(i2c_rx_queue_read(&q, &rx));
        CHECK(rx.address == addresses[i]);
        CHECK(rx.data[0] == addresses[i]);
    }
    CHECK(!i2c_rx_queue_available(&q));
}

/* ── Test 10: data immutable after enqueue ────────────────────────────────── */

static void test_10_data_immutable_after_enqueue(void) {
    printf("Test 10: data immutable after enqueue\n");
    i2c_rx_queue_t q;
    i2c_rx_queue_init(&q);

    /* Enqueue one transaction */
    i2c_rx_queue_begin(&q, 0x70);
    i2c_rx_queue_append(&q, 0xAA);
    i2c_rx_queue_append(&q, 0xBB);
    i2c_rx_queue_end(&q);

    /* Simulate a second transaction being sent by the IRQ producer.
       This writes to write_head (slot 1), not the already-published slot 0. */
    i2c_rx_queue_begin(&q, 0x71);
    i2c_rx_queue_append(&q, 0xFF);
    i2c_rx_queue_end(&q);

    /* Now read the first transaction and verify bytes are unchanged */
    i2c_rx_transaction_t rx;
    CHECK(i2c_rx_queue_read(&q, &rx));
    CHECK(rx.address == 0x70);
    CHECK(rx.length == 2);
    CHECK(rx.data[0] == 0xAA);
    CHECK(rx.data[1] == 0xBB);

    /* Second transaction also correct */
    CHECK(i2c_rx_queue_read(&q, &rx));
    CHECK(rx.address == 0x71 && rx.data[0] == 0xFF);
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== i2c_rx_queue unit tests (DEPTH=%d, MAX_LEN=%d) ===\n\n",
           I2C_MULTI_RX_QUEUE_DEPTH, I2C_MULTI_RX_MAX_LEN);

    test_1_single_transaction();
    test_2_multiple_consecutive();
    test_3_back_to_back();
    test_4_different_lengths();
    test_5_slot_reuse();
    test_6_wrap_around();
    test_7_overflow();
    test_8_repeated_start();
    test_9_different_addresses();
    test_10_data_immutable_after_enqueue();

    printf("\n=== Results: %d / %d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
