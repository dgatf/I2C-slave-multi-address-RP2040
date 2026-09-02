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
 *  Define handlers, write buffer and enable addresses
 *
 * -------------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "i2c_multi.h"
#include "pico/stdlib.h"

PIO pio = pio0;
uint pin = 0;
uint8_t read_buffer[64] = {0};
uint8_t write_buffer[64] = {0};
static volatile bool stop_pending = false;
static volatile uint8_t stop_bytes = 0;
static volatile uint8_t address = 0;
static volatile bool is_read = false;

// I2C handlers run in interrupt context.
// Keep them short and non-blocking. Avoid printf/Serial, delays,
// or other slow operations.

void i2c_request_handler(uint8_t address) {
    is_read = false;
    switch (address) {
        case 0x70:
            write_buffer[0] = 0x10;
            write_buffer[1] = 0x11;
            write_buffer[2] = 0x12;
            write_buffer[3] = 0x13;
            break;
        case 0x71:
            sprintf((char *)write_buffer, "Hello, I'm %X", address);
            break;
    }
}

void i2c_stop_handler(uint8_t addr, bool is_rd, uint length) {
    stop_bytes = length;
    stop_pending = true;
    address = addr;
    is_read = is_rd;
    if (is_read) memcpy(read_buffer, i2c_multi_get_buffer(), length);
}

int main() {
    stdio_init_all();
    i2c_multi_init(pio, pin);
    i2c_multi_enable_address(0x70);
    i2c_multi_enable_address(0x71);
    i2c_multi_set_request_handler(i2c_request_handler);
    i2c_multi_set_stop_handler(i2c_stop_handler);
    i2c_multi_set_write_buffer(write_buffer);

    while (true) {
        if (stop_pending) {
            stop_pending = false;
            bool read = is_read;
            uint8_t bytes = stop_bytes;
            printf("\n%s %uB (0x%X): ", read ? "Read" : "Write", bytes, address);
            for (uint8_t i = 0; i < bytes; i++) {
                printf("0x%X ", read ? read_buffer[i] : write_buffer[i]);
            }
        }
        tight_loop_contents();
    }
}
