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

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// --------------- //
// start_condition //
// --------------- //

#define start_condition_wrap_target 1
#define start_condition_wrap 3

#define start_condition_offset_start 1u

static const uint16_t start_condition_program_instructions[] = {
    0xc004, //  0: irq    nowait 4                   
            //     .wrap_target
    0x20a0, //  1: wait   1 pin, 0                   
    0x2020, //  2: wait   0 pin, 0                   
    0x00c0, //  3: jmp    pin, 0                     
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program start_condition_program = {
    .instructions = start_condition_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline pio_sm_config start_condition_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + start_condition_wrap_target, offset + start_condition_wrap);
    return c;
}
#endif

// -------------- //
// stop_condition //
// -------------- //

#define stop_condition_wrap_target 1
#define stop_condition_wrap 3

#define stop_condition_offset_start 1u

static const uint16_t stop_condition_program_instructions[] = {
    0xc001, //  0: irq    nowait 1                   
            //     .wrap_target
    0x2020, //  1: wait   0 pin, 0                   
    0x20a0, //  2: wait   1 pin, 0                   
    0x00c0, //  3: jmp    pin, 0                     
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program stop_condition_program = {
    .instructions = stop_condition_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline pio_sm_config stop_condition_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + stop_condition_wrap_target, offset + stop_condition_wrap);
    return c;
}
#endif

// --------- //
// read_byte //
// --------- //

#define read_byte_wrap_target 0
#define read_byte_wrap 12

static const uint16_t read_byte_program_instructions[] = {
            //     .wrap_target
    0x20c4, //  0: wait   1 irq, 4                   
    0xe080, //  1: set    pindirs, 0                 
    0xe027, //  2: set    x, 7                       
    0x2021, //  3: wait   0 pin, 1                   
    0x20a1, //  4: wait   1 pin, 1                   
    0x4001, //  5: in     pins, 1                    
    0x0043, //  6: jmp    x--, 3                     
    0xa0d6, //  7: mov    isr, ::isr                 
    0x8000, //  8: push   noblock                    
    0x60f0, //  9: out    exec, 16                   
    0x60f0, // 10: out    exec, 16                   
    0x0009, // 11: jmp    9                          
    0xc005, // 12: irq    nowait 5                   
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program read_byte_program = {
    .instructions = read_byte_program_instructions,
    .length = 13,
    .origin = -1,
};

static inline pio_sm_config read_byte_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + read_byte_wrap_target, offset + read_byte_wrap);
    sm_config_set_sideset(&c, 2, true, false);
    return c;
}
#endif

// ------ //
// do_ack //
// ------ //

#define do_ack_wrap_target 0
#define do_ack_wrap 12

static const uint16_t do_ack_program_instructions[] = {
            //     .wrap_target
    0xa042, //  0: nop                               
    0x2021, //  1: wait   0 pin, 1                   
    0xf083, //  2: set    pindirs, 3             [16]
    0xc020, //  3: irq    wait 0                     
    0xe081, //  4: set    pindirs, 1                 
    0x20a1, //  5: wait   1 pin, 1                   
    0x2021, //  6: wait   0 pin, 1                   
    0x0001, //  7: jmp    1                          
    0x000c, //  8: jmp    12                         
    0xff80, //  9: set    pindirs, 0             [31]
    0x0000, // 10: jmp    0                          
    0xa042, // 11: nop                               
    0xa042, // 12: nop                               
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program do_ack_program = {
    .instructions = do_ack_program_instructions,
    .length = 13,
    .origin = -1,
};

static inline pio_sm_config do_ack_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + do_ack_wrap_target, offset + do_ack_wrap);
    return c;
}
#endif

// ---------- //
// write_byte //
// ---------- //

#define write_byte_wrap_target 0
#define write_byte_wrap 10

static const uint16_t write_byte_program_instructions[] = {
            //     .wrap_target
    0x20c5, //  0: wait   1 irq, 5                   
    0xe081, //  1: set    pindirs, 1                 
    0xe027, //  2: set    x, 7                       
    0x3f21, //  3: wait   0 pin, 1               [31]
    0x6001, //  4: out    pins, 1                    
    0x20a1, //  5: wait   1 pin, 1                   
    0x0043, //  6: jmp    x--, 3                     
    0x6060, //  7: out    null, 32                   
    0x60f0, //  8: out    exec, 16                   
    0x60f0, //  9: out    exec, 16                   
    0x0008, // 10: jmp    8                          
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program write_byte_program = {
    .instructions = write_byte_program_instructions,
    .length = 11,
    .origin = -1,
};

static inline pio_sm_config write_byte_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + write_byte_wrap_target, offset + write_byte_wrap);
    return c;
}
#endif

// -------- //
// wait_ack //
// -------- //

#define wait_ack_wrap_target 0
#define wait_ack_wrap 11

static const uint16_t wait_ack_program_instructions[] = {
            //     .wrap_target
    0x3f21, //  0: wait   0 pin, 1               [31]
    0xe082, //  1: set    pindirs, 2                 
    0xe000, //  2: set    pins, 0                    
    0xc020, //  3: irq    wait 0                     
    0xff80, //  4: set    pindirs, 0             [31]
    0x20a1, //  5: wait   1 pin, 1                   
    0xa042, //  6: nop                               
    0x00c0, //  7: jmp    pin, 0                     
    0xa042, //  8: nop                               
    0x0001, //  9: jmp    1                          
    0xe080, // 10: set    pindirs, 0                 
    0x6060, // 11: out    null, 32                   
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program wait_ack_program = {
    .instructions = wait_ack_program_instructions,
    .length = 12,
    .origin = -1,
};

static inline pio_sm_config wait_ack_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + wait_ack_wrap_target, offset + wait_ack_wrap);
    return c;
}

#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
typedef enum i2c_multi_status_t
{
    I2C_IDLE,
    I2C_READ,
    I2C_WRITE
} i2c_multi_status_t;
typedef void (*i2c_multi_receive_handler_t)(uint8_t data, bool is_address);
typedef void (*i2c_multi_request_handler_t)(uint8_t address);
typedef void (*i2c_multi_stop_handler_t)(uint8_t lenght);
typedef struct i2c_multi_t {
    PIO pio;
    uint offset_read, offset_write, sm_read, sm_write, offset_start, offset_stop, sm_start, sm_stop, pin;
    i2c_multi_status_t status;
    uint8_t *buffer, *buffer_start;
    uint8_t bytes_count;
    uint address[4];
} i2c_multi_t;
static i2c_multi_t *i2c_multi;
static void (*i2c_multi_receive_handler_p)(uint8_t data, bool is_address) = NULL;
static void (*i2c_multi_request_handler_p)(uint8_t address) = NULL;
static void (*i2c_multi_stop_handler_p)(uint8_t lenght) = NULL;
static inline uint8_t i2c_multi_transpond_byte(uint8_t byte);
static inline void i2c_multi_set_write_buffer(uint8_t *buffer);
static inline void i2c_multi_set_receive_handler(i2c_multi_receive_handler_t handler);
static inline void i2c_multi_set_request_handler(i2c_multi_request_handler_t handler);
static inline void i2c_multi_set_stop_handler(i2c_multi_stop_handler_t handler);
static inline void i2c_multi_enable_address(uint8_t address);
static inline void i2c_multi_disable_address(uint8_t address);
static inline void i2c_multi_enable_all_addresses();
static inline void i2c_multi_disable_all_addresses();
static inline bool i2c_multi_is_address_enabled(uint8_t address);
static inline void i2c_multi_disable();
static inline void i2c_multi_restart();
static inline void i2c_multi_remove();
static inline void i2c_multi_irq();
static inline void i2c_multi_stop_irq();
static inline void i2c_multi_init(PIO pio, uint pin);
static inline void i2c_multi_start_condition_program_init(PIO pio, uint sm, uint offset, uint pin);
static inline void i2c_multi_stop_condition_program_init(PIO pio, uint sm, uint offset, uint pin);
static inline void i2c_multi_read_byte_program_init(PIO pio, uint sm, uint offset, uint pin);
static inline void i2c_multi_write_byte_program_init(PIO pio, uint sm, uint offset, uint pin);
static inline void i2c_multi_init(PIO pio, uint pin)
{
    i2c_multi = (i2c_multi_t *)malloc(sizeof(i2c_multi_t));
    i2c_multi->pio = pio;
    i2c_multi->status = I2C_IDLE;
    i2c_multi->pin = pin;
    i2c_multi->bytes_count = 0;
    i2c_multi_disable_all_addresses();
    i2c_multi->buffer = NULL;
    i2c_multi->buffer_start = NULL;
    uint pio_irq0 = (pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0);
    uint pio_irq1 = (pio == pio0 ? PIO0_IRQ_1 : PIO1_IRQ_1);
    pio_gpio_init(pio, pin);
    pio_gpio_init(pio, pin + 1);
    i2c_multi->offset_start = pio_add_program(pio, &start_condition_program);
    i2c_multi->sm_start = pio_claim_unused_sm(pio, true);
    i2c_multi_start_condition_program_init(pio, i2c_multi->sm_start, i2c_multi->offset_start, pin);
    i2c_multi->offset_stop = pio_add_program(pio, &stop_condition_program);
    i2c_multi->sm_stop = pio_claim_unused_sm(pio, true);
    i2c_multi_stop_condition_program_init(pio, i2c_multi->sm_stop, i2c_multi->offset_stop, pin);
    i2c_multi->offset_read = pio_add_program(pio, &read_byte_program);
    i2c_multi->sm_read = pio_claim_unused_sm(pio, true);
    i2c_multi_read_byte_program_init(pio, i2c_multi->sm_read, i2c_multi->offset_read, pin);
    i2c_multi->offset_write = pio_add_program(pio, &write_byte_program);
    i2c_multi->sm_write = pio_claim_unused_sm(pio, true);
    i2c_multi_write_byte_program_init(pio, i2c_multi->sm_write, i2c_multi->offset_write, pin);
    pio_sm_put_blocking(pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[1]) << 16) | do_ack_program_instructions[0]);
    pio_sm_put_blocking(pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[3]) << 16) | do_ack_program_instructions[2]);
    irq_set_exclusive_handler(pio_irq0, i2c_multi_irq);
    irq_set_enabled(pio_irq0, true);
    irq_set_exclusive_handler(pio_irq1, i2c_multi_stop_irq);
    irq_set_enabled(pio_irq1, true);
}
static inline void i2c_multi_disable()
{
    pio_sm_set_enabled(i2c_multi->pio, i2c_multi->sm_read, false);
    pio_sm_set_enabled(i2c_multi->pio, i2c_multi->sm_write, false);
    pio_sm_set_enabled(i2c_multi->pio, i2c_multi->sm_start, false);
    pio_sm_set_enabled(i2c_multi->pio, i2c_multi->sm_stop, false);
    pio_sm_clear_fifos(i2c_multi->pio, i2c_multi->sm_read);
    pio_sm_clear_fifos(i2c_multi->pio, i2c_multi->sm_write);
    gpio_set_input_enabled(i2c_multi->pin, true);
    gpio_set_input_enabled(i2c_multi->pin + 1, true);
    i2c_multi->bytes_count = 0;
    i2c_multi->status = I2C_IDLE;
    i2c_multi->buffer = i2c_multi->buffer_start;
}
static inline void i2c_multi_restart()
{
    i2c_multi_disable();
    pio_sm_restart(i2c_multi->pio, i2c_multi->sm_start);
    pio_sm_restart(i2c_multi->pio, i2c_multi->sm_stop);
    pio_sm_restart(i2c_multi->pio, i2c_multi->sm_read);
    pio_sm_restart(i2c_multi->pio, i2c_multi->sm_write);
    pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[1]) << 16) | do_ack_program_instructions[0]);
    pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[3]) << 16) | do_ack_program_instructions[2]);
    pio_sm_set_enabled(i2c_multi->pio, i2c_multi->sm_read, true);
    pio_sm_set_enabled(i2c_multi->pio, i2c_multi->sm_write, true);
    pio_sm_set_enabled(i2c_multi->pio, i2c_multi->sm_start, true);
    pio_sm_set_enabled(i2c_multi->pio, i2c_multi->sm_stop, true);
}
static inline void i2c_multi_remove()
{
    i2c_multi_receive_handler_p = NULL;
    i2c_multi_request_handler_p = NULL;
    i2c_multi_stop_handler_p = NULL;
    pio_set_irq0_source_enabled(i2c_multi->pio, pis_interrupt0, false);
    pio_set_irq1_source_enabled(i2c_multi->pio, pis_interrupt1, false);
    pio_clear_instruction_memory(i2c_multi->pio);
    pio_sm_unclaim(i2c_multi->pio, i2c_multi->sm_start);
    pio_sm_unclaim(i2c_multi->pio, i2c_multi->sm_stop);
    pio_sm_unclaim(i2c_multi->pio, i2c_multi->sm_read);
    pio_sm_unclaim(i2c_multi->pio, i2c_multi->sm_write);
    i2c_multi->buffer = NULL;
    i2c_multi->buffer_start = NULL;
    i2c_multi->bytes_count = 0;
    i2c_multi->status = I2C_IDLE;
    gpio_set_input_enabled(i2c_multi->pin, true);
    gpio_set_input_enabled(i2c_multi->pin + 1, true);
    free(i2c_multi);
}
static inline void i2c_multi_enable_address(uint8_t address)
{
    i2c_multi->address[address / 32] |= 1 << (address % 32);
}
static inline void i2c_multi_disable_address(uint8_t address)
{
    i2c_multi->address[address / 32] &= ~(1 << (address % 32));
}
static inline void i2c_multi_enable_all_addresses()
{
    i2c_multi->address[0] = 0xFFFFFFFF;
    i2c_multi->address[1] = 0xFFFFFFFF;
    i2c_multi->address[2] = 0xFFFFFFFF;
    i2c_multi->address[3] = 0xFFFFFFFF;
}
static inline void i2c_multi_disable_all_addresses()
{
    i2c_multi->address[0] = 0;
    i2c_multi->address[1] = 0;
    i2c_multi->address[2] = 0;
    i2c_multi->address[3] = 0;
}
static inline bool i2c_multi_is_address_enabled(uint8_t address)
{
    return i2c_multi->address[address / 32] & (1 << (address % 32));
}
static inline void i2c_multi_set_write_buffer(uint8_t *buffer)
{
    i2c_multi->buffer = buffer;
    i2c_multi->buffer_start = buffer;
}
static inline void i2c_multi_set_receive_handler(i2c_multi_receive_handler_t handler)
{
    i2c_multi_receive_handler_p = handler;
}
static inline void i2c_multi_set_request_handler(i2c_multi_request_handler_t handler)
{
    i2c_multi_request_handler_p = handler;
}
static inline void i2c_multi_set_stop_handler(i2c_multi_stop_handler_t handler)
{
    i2c_multi_stop_handler_p = handler;
}
static inline uint8_t i2c_multi_transpond_byte(uint8_t byte)
{
    uint8_t transponded = ((byte & 0x1) << 7) |
                          (((byte & 0x2) >> 1) << 6) |
                          (((byte & 0x4) >> 2) << 5) |
                          (((byte & 0x8) >> 3) << 4) |
                          (((byte & 0x10) >> 4) << 3) |
                          (((byte & 0x20) >> 5) << 2) |
                          (((byte & 0x40) >> 6) << 1) |
                          (((byte & 0x80) >> 7));
    return transponded;
}
static inline void i2c_multi_irq()
{
    pio_interrupt_clear(i2c_multi->pio, 0);
    uint8_t received = 0;
    bool is_address = false;
    i2c_multi->bytes_count++;
    if (i2c_multi->status != I2C_WRITE)
    {
        received = pio_sm_get_blocking(i2c_multi->pio, i2c_multi->sm_read);
    }
    if (i2c_multi->status == I2C_IDLE)
    {
        if (!i2c_multi_is_address_enabled(received >> 1))
        {
            i2c_multi->status = I2C_IDLE;
            i2c_multi->bytes_count = 0;
            pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[10] + i2c_multi->offset_read) << 16) | do_ack_program_instructions[9]);
            pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[1]) << 16) | do_ack_program_instructions[0]);
            pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[3]) << 16) | do_ack_program_instructions[2]);
            return;
        }
        if (received & 1)
        {
            i2c_multi->status = I2C_WRITE;
        }
        else
        {
            i2c_multi->status = I2C_READ;
        }
        is_address = true;
    }
    if (i2c_multi->status == I2C_READ)
    {
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[5]) << 16) | do_ack_program_instructions[4]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[7] + i2c_multi->offset_read) << 16) | do_ack_program_instructions[6]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[1]) << 16) | do_ack_program_instructions[0]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[3]) << 16) | do_ack_program_instructions[2]);
        if (i2c_multi_receive_handler_p)
        {
            if (is_address)
                i2c_multi_receive_handler_p(received >> 1, true);
            else
                i2c_multi_receive_handler_p(received, false);
        }
    }
    if (i2c_multi->status == I2C_WRITE && is_address)
    {
        if (i2c_multi_request_handler_p)
        {
            i2c_multi_request_handler_p(received >> 1);
        }
        uint8_t value = 0;
        if (i2c_multi->buffer)
        {
            value = i2c_multi_transpond_byte(*i2c_multi->buffer);
            i2c_multi->buffer++;
        }
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[5]) << 16) | do_ack_program_instructions[4]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[8] + i2c_multi->offset_read) << 16) | do_ack_program_instructions[6]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[1]) << 16) | do_ack_program_instructions[0]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_read, (((uint32_t)do_ack_program_instructions[3]) << 16) | do_ack_program_instructions[2]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, value);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, (((uint32_t)wait_ack_program_instructions[1]) << 16) | wait_ack_program_instructions[0]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, (((uint32_t)wait_ack_program_instructions[3]) << 16) | wait_ack_program_instructions[2]);
    }
    if (i2c_multi->status == I2C_WRITE && !is_address)
    {
        uint8_t value = 0;
        if (i2c_multi->buffer)
        {
            value = i2c_multi_transpond_byte(*i2c_multi->buffer);
            i2c_multi->buffer++;
        }
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, (((uint32_t)wait_ack_program_instructions[5]) << 16) | wait_ack_program_instructions[4]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, (((uint32_t)wait_ack_program_instructions[7] + i2c_multi->offset_write) << 16) | (wait_ack_program_instructions[6]));
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, (((uint32_t)wait_ack_program_instructions[9] + i2c_multi->offset_write) << 16) | (wait_ack_program_instructions[8]));
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, value);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, (((uint32_t)wait_ack_program_instructions[1]) << 16) | wait_ack_program_instructions[0]);
        pio_sm_put_blocking(i2c_multi->pio, i2c_multi->sm_write, (((uint32_t)wait_ack_program_instructions[3]) << 16) | wait_ack_program_instructions[2]);
    }
}
static inline void i2c_multi_stop_irq()
{
    pio_interrupt_clear(i2c_multi->pio, 1);
    if (i2c_multi->status == I2C_IDLE)
        return;
    pio_sm_exec(i2c_multi->pio, i2c_multi->sm_read, i2c_multi->offset_read);
    pio_sm_clear_fifos(i2c_multi->pio, i2c_multi->sm_write);
    pio_sm_exec(i2c_multi->pio, i2c_multi->sm_write, wait_ack_program_instructions[11]);
    pio_sm_exec(i2c_multi->pio, i2c_multi->sm_write, i2c_multi->offset_write);
    i2c_multi->buffer = i2c_multi->buffer_start;
    if (i2c_multi_stop_handler_p)
    {
        i2c_multi_stop_handler_p(i2c_multi->bytes_count - 1);
    }
    i2c_multi->bytes_count = 0;
    i2c_multi->status = I2C_IDLE;
}
static inline void i2c_multi_start_condition_program_init(PIO pio, uint sm, uint offset, uint pin)
{
    pio_sm_config c = start_condition_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_clkdiv(&c, 10);
    sm_config_set_jmp_pin(&c, pin + 1);
    pio_sm_init(pio, sm, offset + start_condition_offset_start, &c);
    pio_sm_set_enabled(pio, sm, true);
}
static inline void i2c_multi_stop_condition_program_init(PIO pio, uint sm, uint offset, uint pin)
{
    pio_sm_config c = stop_condition_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_clkdiv(&c, 10);
    sm_config_set_jmp_pin(&c, pin + 1);
    pio_sm_init(pio, sm, offset + stop_condition_offset_start, &c);
    pio_sm_set_enabled(pio, sm, true);
    pio_set_irq1_source_enabled(pio, pis_interrupt1, true);
    pio_interrupt_clear(pio, 1);
}
static inline void i2c_multi_read_byte_program_init(PIO pio, uint sm, uint offset, uint pin)
{
    pio_sm_config c = read_byte_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_clkdiv(&c, 10);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, true, true, 32);
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    pio_interrupt_clear(pio, 0);
    sm_config_set_set_pins(&c, pin, 2);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
static inline void i2c_multi_write_byte_program_init(PIO pio, uint sm, uint offset, uint pin)
{
    pio_sm_config c = write_byte_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_out_pins(&c, pin, 1);
    sm_config_set_set_pins(&c, pin, 2);
    sm_config_set_clkdiv(&c, 10);
    sm_config_set_out_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_jmp_pin(&c, pin);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

#endif
