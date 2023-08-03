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
#include "pico/stdlib.h"
#include "i2c_multi.h"

PIO pio = pio0;
uint pin = 0;
uint8_t buffer[64] = {0};

void i2c_receive_handler(uint8_t data, bool is_address)
{
    if (is_address)
        printf("\nAddress: %X, receiving...", data);
    else
        printf("\nReceived: %X", data);
}

void i2c_request_handler(uint8_t address)
{
    printf("\nAddress: %X, request...", address);

    switch (address)
    {
    case 0x70:
        buffer[0] = 0x10;
        buffer[1] = 0x11;
        buffer[2] = 0x12;
        break;
    case 0x71:
        sprintf((char *)buffer, "Hello, I'm %X", address);
        break;
    }
}

void i2c_stop_handler(uint8_t length)
{
    printf("\nTotal bytes: %u", length);
}

int main()
{
    stdio_init_all();

    i2c_multi_init(pio, pin);
    i2c_multi_enable_address(0x70);
    i2c_multi_enable_address(0x71);
    i2c_multi_set_receive_handler(i2c_receive_handler);
    i2c_multi_set_request_handler(i2c_request_handler);
    i2c_multi_set_stop_handler(i2c_stop_handler);
    i2c_multi_set_write_buffer(buffer);

    while (true)
    {
    }
}
