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
 * has been enqueued in the RX queue; @p length is the number of data bytes.
 * Use i2c_multi_rx_read() inside or after this callback to dequeue it.
 * Not called when an RX transaction is dropped due to queue overflow; use
 * i2c_multi_rx_overflow() to detect that condition.
 *
 * For master-read (slave-transmit) transactions: called at the end of the
 * read; @p length is the number of bytes sent.
 */
typedef void (*i2c_multi_stop_handler_t)(uint16_t length);

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
 * in the receive queue.  Safe to call from the main thread without disabling
 * IRQs (single-core SPSC guarantee).
 */
bool i2c_multi_rx_available(void);

/**
 * Dequeues the oldest complete transaction.
 *
 * Copies the transaction's bytes into @p buf (caller-supplied; must be at
 * least @p buf_size bytes), sets @p out->data = buf, @p out->length, and
 * @p out->address.
 *
 * @p buf_size should be at least i2c_multi_rx_peek_length() bytes.  If the
 * transaction is larger than @p buf_size, the function returns false and does
 * NOT consume the transaction (it remains in the queue).
 *
 * @param out       Caller-supplied struct to receive metadata.
 * @param buf       Caller-supplied buffer for the data bytes.
 * @param buf_size  Size of buf in bytes.
 * @return          true on success; false if queue empty or buf too small.
 */
bool i2c_multi_rx_read(i2c_rx_transaction_t *out, uint8_t *buf, uint16_t buf_size);

/**
 * Returns the length in bytes of the next pending transaction without
 * consuming it.  Returns 0 if no transaction is available.
 */
uint16_t i2c_multi_rx_peek_length(void);

/**
 * Returns true if at least one transaction has been dropped (overflow) since
 * the last call, and clears the flag.
 *
 * The two causes of overflow are:
 *   1. Byte ring full — not enough space for the incoming bytes.
 *   2. Descriptor FIFO full — no free descriptor slots.
 *
 * Both are reported through this single flag.
 */
bool i2c_multi_rx_overflow(void);

#ifdef __cplusplus
}
#endif

#endif
