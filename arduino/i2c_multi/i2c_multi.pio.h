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

#define start_condition_wrap_target 2
#define start_condition_wrap 4

#define start_condition_offset_start 2u

static const uint16_t start_condition_program_instructions[] = {
    0xc001, //  0: irq    nowait 1                   
    0xc004, //  1: irq    nowait 4                   
            //     .wrap_target
    0x20a0, //  2: wait   1 pin, 0                   
    0x2c20, //  3: wait   0 pin, 0               [12]
    0x00c0, //  4: jmp    pin, 0                     
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program start_condition_program = {
    .instructions = start_condition_program_instructions,
    .length = 5,
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
    0x2ca0, //  2: wait   1 pin, 0               [12]
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
#define read_byte_wrap 11

static const uint16_t read_byte_program_instructions[] = {
            //     .wrap_target
    0x20c4, //  0: wait   1 irq, 4                   
    0xe080, //  1: set    pindirs, 0                 
    0xe027, //  2: set    x, 7                       
    0x2021, //  3: wait   0 pin, 1                   
    0x20a1, //  4: wait   1 pin, 1                   
    0x4001, //  5: in     pins, 1                    
    0x0043, //  6: jmp    x--, 3                     
    0x8000, //  7: push   noblock                    
    0x60f0, //  8: out    exec, 16                   
    0x60f0, //  9: out    exec, 16                   
    0x0008, // 10: jmp    8                          
    0xc005, // 11: irq    nowait 5                   
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program read_byte_program = {
    .instructions = read_byte_program_instructions,
    .length = 12,
    .origin = -1,
};

static inline pio_sm_config read_byte_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + read_byte_wrap_target, offset + read_byte_wrap);
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
    0x2021, //  0: wait   0 pin, 1                   
    0xe083, //  1: set    pindirs, 3                 
    0xef00, //  2: set    pins, 0                [15]
    0xc020, //  3: irq    wait 0                     
    0xe081, //  4: set    pindirs, 1                 
    0x20a1, //  5: wait   1 pin, 1                   
    0x2021, //  6: wait   0 pin, 1                   
    0x0001, //  7: jmp    1                          
    0x000b, //  8: jmp    11                         
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
#endif

