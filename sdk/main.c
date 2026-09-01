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
 *  I2C slave multi - answer to multiple addresses
 *
 *  SDA = pin
 *  SCL = pin + 1
 *
 *  Add external pull ups, 1k - 3.3k
 *
 *  Define handlers, write buffer and enable addresses.
 *  Received write transactions are buffered in an internal ring queue;
 *  use i2c_multi_rx_available() / i2c_multi_rx_read() to dequeue them.
 *
 * -------------------------------------------------------------------------------
 */

#include <stdio.h>

#include "i2c_multi.h"
#include "pico/stdlib.h"

PIO pio = pio0;
uint pin = 0;
uint8_t tx_buffer[64] = {0};
/* Receive buffer: sized to the maximum expected single transaction. */
uint8_t rx_buf[I2C_MULTI_RX_BUFFER_SIZE];

void i2c_request_handler(uint8_t address) {
    switch (address) {
        case 0x70:
            tx_buffer[0] = 0x10;
            tx_buffer[1] = 0x11;
            tx_buffer[2] = 0x12;
            break;
        case 0x71:
            sprintf((char *)tx_buffer, "Hello, I'm %X", address);
            break;
    }
}

int main() {
    stdio_init_all();
    i2c_multi_init(pio, pin);
    i2c_multi_enable_address(0x70);
    i2c_multi_enable_address(0x71);
    i2c_multi_set_request_handler(i2c_request_handler);
    i2c_multi_set_write_buffer(tx_buffer);

    while (true) {
        i2c_rx_transaction_t rx;
        while (i2c_multi_rx_available()) {
            uint16_t len = i2c_multi_rx_peek_length();
            if (len <= sizeof(rx_buf) && i2c_multi_rx_read(&rx, rx_buf, sizeof(rx_buf))) {
                printf("\nWrite %uB (0x%X): ", rx.length, rx.address);
                for (uint16_t i = 0; i < rx.length; i++) {
                    printf("0x%X ", rx.data[i]);
                }
            }
        }
        if (i2c_multi_rx_overflow()) {
            printf("\n[WARNING] RX queue overflow: at least one transaction was dropped\n");
        }
        tight_loop_contents();
    }
}
