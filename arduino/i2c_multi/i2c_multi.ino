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
 *  I2C slave - answer to multiple addresses
 *
 *  SDA = pin
 *  SCL = pin + 1
 *
 *  Add external pull ups, 1k - 3.3k
 *
 *  Define handlers and write buffer
 *
 * -------------------------------------------------------------------------------
 */

#include "i2c_multi.pio.h"

PIO pio = pio0;
uint pin = 7;
uint8_t buffer[64] = {0};
char str_out[64];

void i2c_receive_handler(uint8_t data, bool is_address)
{
    if (is_address)
        sprintf(str_out, "\nAddress: %X, receiving...", data);
    else
        sprintf(str_out, "\nReceived: %X", data);
    Serial.print(str_out);
}

void i2c_request_handler(uint8_t address)
{
    sprintf(str_out, "\nAddress: %X, request...", address);
    Serial.print(str_out);

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

void i2c_stop_handler(uint8_t lenght)
{
    sprintf(str_out, "\nTotal bytes: %u", lenght);
    Serial.print(str_out);
}

void setup()
{
    Serial.begin(115200);

    i2c_multi_init(pio, pin);
    i2c_multi_enable_address(0x70);
    i2c_multi_enable_address(0x71);
    i2c_multi_set_receive_handler(i2c_receive_handler);
    i2c_multi_set_request_handler(i2c_request_handler);
    i2c_multi_set_stop_handler(i2c_stop_handler);
    i2c_multi_set_write_buffer(buffer);
}

void loop()
{
}
