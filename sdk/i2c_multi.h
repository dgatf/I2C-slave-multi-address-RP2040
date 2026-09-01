#ifndef I2C_MULTI
#define I2C_MULTI

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "hardware/pio.h"
#include "i2c_multi.pio.h"
#include "i2c_rx_queue.h"

typedef enum i2c_multi_status_t { I2C_IDLE, I2C_READ, I2C_WRITE } i2c_multi_status_t;

typedef void (*i2c_multi_request_handler_t)(uint8_t address);

/**
 * Called when a complete transaction ends.
 *
 * For master-write (slave-receive) transactions: called after the transaction
 * has been enqueued; @p length is the number of data bytes (≥ 0).  Use
 * i2c_multi_rx_read() inside or after this callback to dequeue it.
 *
 * For master-read (slave-transmit) transactions: called at end of the read;
 * @p length is the number of bytes sent.
 *
 * Not called when an RX transaction is dropped due to queue overflow; use
 * i2c_multi_rx_overflow() to detect that condition.
 */
typedef void (*i2c_multi_stop_handler_t)(uint8_t length);

typedef struct i2c_multi_t {
    PIO pio;
    uint offset_read, offset_write, sm_read, sm_write, offset_start, offset_stop, sm_start, sm_stop, pin;
    i2c_multi_status_t status;
    uint8_t *buffer, *buffer_start;
    uint8_t bytes_count;
    int16_t length;
    uint address[4];
} i2c_multi_t;

void i2c_multi_init(PIO pio, uint pin);
void i2c_multi_set_write_buffer(uint8_t *buffer);
void i2c_multi_set_request_handler(i2c_multi_request_handler_t handler);
void i2c_multi_set_stop_handler(i2c_multi_stop_handler_t handler);
void i2c_multi_enable_address(uint8_t address);
void i2c_multi_disable_address(uint8_t address);
void i2c_multi_enable_all_addresses(void);
void i2c_multi_disable_all_addresses(void);
bool i2c_multi_is_address_enabled(uint8_t address);
void i2c_multi_disable(void);
void i2c_multi_restart(void);
void i2c_multi_remove(void);
void i2c_multi_fixed_length(int16_t length);

/* ── RX queue API ────────────────────────────────────────────────────────── */

/**
 * Returns true if at least one complete master-write transaction is waiting
 * in the receive queue.  Safe to call from main thread without disabling IRQs.
 */
bool i2c_multi_rx_available(void);

/**
 * Dequeues the oldest complete transaction into @p out.
 *
 * @param out  Caller-provided struct to receive the transaction data.
 *             Must not be NULL.
 * @return     true on success; false if the queue was empty.
 */
bool i2c_multi_rx_read(i2c_rx_transaction_t *out);

/**
 * Returns true if at least one transaction has been dropped since the last
 * call, and clears the flag.  A dropped transaction means the queue was full
 * when the transaction arrived; the new transaction was discarded to preserve
 * the already-buffered (older) data.
 */
bool i2c_multi_rx_overflow(void);

#ifdef __cplusplus
}
#endif

#endif
