
//  Part of the Raspberry-Pi Bare Metal Tutorials
//  Copyright (c) 2013-2015, Brian Sidebotham
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.

//  Relocate to just below 32MB

#include "defs.h"
#include "rpi-base.h"

.equ    STACK_SIZE,           0x00100000

.equ    C0_SVR_STACK,        0
.equ    C0_IRQ_STACK,        (STACK_SIZE*12)
.equ    C0_FIQ_STACK,        STACK_SIZE*13
.equ    C0_USER_STACK,       STACK_SIZE*14
.equ    C0_ABORT_STACK,      STACK_SIZE*15
.equ    C0_UNDEFINED_STACK,  STACK_SIZE*16

.equ    C1_SVR_STACK,        STACK_SIZE*17
.equ    C1_IRQ_STACK,        STACK_SIZE*18
.equ    C1_FIQ_STACK,        STACK_SIZE*19
.equ    C1_USER_STACK,       STACK_SIZE*20
.equ    C1_ABORT_STACK,      STACK_SIZE*21
.equ    C1_UNDEFINED_STACK,  STACK_SIZE*22



.equ    SCTLR_ENABLE_DATA_CACHE,        0x4
.equ    SCTLR_ENABLE_BRANCH_PREDICTION, 0x800
.equ    SCTLR_ENABLE_INSTRUCTION_CACHE, 0x1000

.section ".text.startup"

.global _start
.global _get_cpsr
.global _init_cycle_counter
.global _get_cycle_counter
.global _get_stack_pointer
.global _exception_table
.global _enable_interrupts
.global _set_interrupts
.global _disable_interrupts
.global _enable_unaligned_access
.global _enable_l1_cache
.global _invalidate_icache
.global _invalidate_dcache
.global _clean_invalidate_dcache
.global _invalidate_dcache_mva
.global _clean_invalidate_dcache_mva
.global _invalidate_dtlb
.global _invalidate_dtlb_mva
.global _data_memory_barrier

.global _get_hardware_id
.global _get_peripheral_base
.global _get_GPLEV0_r4
.global _get_gpu_data_base_r4
.global _get_gpu_command_base_r10
.global _hardware_id
.global _peripheral_base


.global _get_core
.global _init_core
.global _spin_core

// From the ARM ARM (Architecture Reference Manual). Make sure you get the
// ARMv5 documentation which includes the ARMv6 documentation which is the
// correct processor type for the Broadcom BCM2835. The ARMv6-M manuals
// available on the ARM website are for Cortex-M parts only and are very
// different.
//
// See ARM section A2.2 (Processor Modes)

.equ    CPSR_MODE_USER,         0x10
.equ    CPSR_MODE_FIQ,          0x11
.equ    CPSR_MODE_IRQ,          0x12
.equ    CPSR_MODE_SVR,          0x13
.equ    CPSR_MODE_ABORT,        0x17
.equ    CPSR_MODE_HYP,          0x1A
.equ    CPSR_MODE_UNDEFINED,    0x1B
.equ    CPSR_MODE_SYSTEM,       0x1F

.equ    CPSR_MODE_MASK,         0x1F

// See ARM section A2.5 (Program status registers)
.equ    CPSR_A_BIT,             0x100
.equ    CPSR_IRQ_INHIBIT,       0x80
.equ    CPSR_FIQ_INHIBIT,       0x40
.equ    CPSR_THUMB,             0x20

_start:
    ldr pc, _reset_h
    ldr pc, _undefined_instruction_vector_h
    ldr pc, _software_interrupt_vector_h
    ldr pc, _prefetch_abort_vector_h
    ldr pc, _data_abort_vector_h
    ldr pc, _unused_handler_h
    ldr pc, _interrupt_vector_h
    ldr pc, _fast_interrupt_vector_h

_reset_h:                           .word   _reset_
_undefined_instruction_vector_h:    .word   _undefined_instruction_handler_
_software_interrupt_vector_h:       .word   _swi_handler_
_prefetch_abort_vector_h:           .word   _prefetch_abort_handler_
_data_abort_vector_h:               .word   _data_abort_handler_
_unused_handler_h:                  .word   _reset_
_interrupt_vector_h:                .word   arm_irq_handler
_fast_interrupt_vector_h:           .word   arm_fiq_handler

.section ".text._reset_"
_reset_:

    bl _read_hardware_id

#if defined(RPI4)
	ldr	r1,=0xff842000
    mov  r2,#0
	str r2,[r1]		// disable GIC on rpi4
#endif
    //   BL _enable_l1_cache

    bl     _get_hardware_id
    cmp    r0, #_RPI2
    blt    rpi0_1_d
#ifdef KERNEL_OLD

    // if kernel_old=1 all cores are running and we need to sleep 1-3
    // if kernel_old=0 then just core0 is running, and core 1-3 are waiting
    // on a mailbox write to be woken up.
    //
    // Test which core we are running on
    mrc     p15, 0, r0, c0, c0, 5
    ands    r0, #3
    beq     _core_continue
    // Put cores 1-3 into a tight loop
_core_loop:
    wfi
    b       _core_loop
_core_continue:

#else

    //1. Set the CPACR for access to CP10 and CP11, and clear the ASEDIS and D32DIS bits:
    ldr     r0, =(0xf << 20)
    mcr     p15, 0, r0, c1, c0, 2

    // 2. Set the FPEXC EN bit to enable the NEON MPE:
    mov     r0, #0x40000000
    vmsr    fpexc, r0

    // if kernel_old=0 enter in HYP mode and need to force a switch to SVC mode
    //
    // for now we assume kernel_old=1 and don't execute this core
    //
    // The logs show:
    // SVC mode: cpsr ends with 1d3
    // HYP mode: cpsr ends with 1a3
    mrs     r0, cpsr
    eor     r0, r0, #CPSR_MODE_HYP
    tst     r0, #CPSR_MODE_MASK
    bic     r0 , r0 , #CPSR_MODE_MASK
    orr     r0 , r0 , #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR
    bne     _not_in_hyp_mode
    orr     r0, r0, #CPSR_A_BIT
    adr     lr, _reset_continue
    msr     spsr_cxsf, r0
    .word 0xE12EF30E  // msr_elr_hyp lr
    .word 0xE160006E  // eret
_not_in_hyp_mode:
    msr    cpsr_c, r0
_reset_continue:
#endif

rpi0_1_d:
    // We enter execution in supervisor mode. For more information on
    // processor modes see ARM Section A2.2 (Processor Modes)

    ldr     r0,=_start
    mov     r1, #0x00000000
    ldmia   r0!,{r2, r3, r4, r5, r6, r7, r8, r9}
    stmia   r1!,{r2, r3, r4, r5, r6, r7, r8, r9}
    ldmia   r0!,{r2, r3, r4, r5, r6, r7, r8, r9}
    stmia   r1!,{r2, r3, r4, r5, r6, r7, r8, r9}

    // Initialise Stack Pointers ---------------------------------------------
    ldr     r4,=_start

    // We're going to use interrupt mode, so setup the interrupt mode
    // stack pointer which differs to the application stack pointer:
    msr cpsr_c, #(CPSR_MODE_IRQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_IRQ_STACK

    // Also setup the stack used for FIQs

    msr cpsr_c, #(CPSR_MODE_FIQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_FIQ_STACK

    // Also setup the stack used for undefined exceptions
    msr cpsr_c, #(CPSR_MODE_UNDEFINED | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_UNDEFINED_STACK

    // Also setup the stack used for prefetch and data abort exceptions
    msr cpsr_c, #(CPSR_MODE_ABORT | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_ABORT_STACK

    // Finally, a user/system mode stack, although the application will likely reset this
    msr cpsr_c, #(CPSR_MODE_SYSTEM | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_USER_STACK

    // Switch back to supervisor mode (our application mode) and
    // set the stack pointer. Remember that the stack works its way
    // down memory, our heap will work it's way up from after the
    // application.
    msr cpsr_c, #(CPSR_MODE_SVR | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_SVR_STACK

    // Enable VFP ------------------------------------------------------------

    bl     _get_hardware_id
    cmp    r0, #_RPI2
    bge    rpi2_4_a

    // r1 = Access Control Register
    MRC p15, #0, r1, c1, c0, #2
    // enable full access for p10,11
    ORR r1, r1, #(0xf << 20)
    // ccess Control Register = r1
    MCR p15, #0, r1, c1, c0, #2
    MOV r1, #0
    // flush prefetch buffer because of FMXR below
    MCR p15, #0, r1, c7, c5, #4
    // and CP 10 & 11 were only just enabled
    // Enable VFP itself
    MOV r0,#0x40000000
    // FPEXC = r0
    FMXR FPEXC, r0

rpi2_4_a:

    // The c-startup function which we never return from. This function will
    // initialise the ro data section (most things that have the const
    // declaration) and initialise the bss section variables to 0 (generally
    // known as automatics). It'll then call main

    b      _cstartup

       .ltorg
_read_hardware_id:
        mrc    p15,0,r5,c0,c0,0
        bic    r5, r5, #0xff000000
        bic    r5, r5, #0x00ff0000
        bic    r5, r5, #0x0000000f
        mov    r0, #0
        ldr    r1, =0x0000B760      //0x410FB767   pi zero or 1
        ldr    r2, =0x0000C070      //0x410FC075   pi 2
        ldr    r3, =0x0000D030      //0x410FD034   pi zero 2W or 3
        ldr    r4, =0x0000D080      //0x410FD083   pi 4
        ldr    r6, =_PERIPHERAL_BASE_RPI
        ldr    r7, =_PERIPHERAL_BASE_RPI3   //also RPI2
        ldr    r8, =_PERIPHERAL_BASE_RPI4
        cmp    r1, r5
        moveq  r0, #1
        cmp    r2, r5
        moveq  r0, #2
        moveq  r6, r7
        cmp    r3, r5
        moveq  r0, #3
        moveq  r6, r7
        cmp    r4, r5
        moveq  r0, #4
        moveq  r6, r8
        str    r0, _hardware_id
        str    r6, _peripheral_base
        ldr    r1, =(GPU_COMMAND_BASE_OFFSET + GPU_DATA_0_offset)
        add    r1, r1, r6
        str    r1, _gpu_data_0
        ldr    r1, =GPU_COMMAND_BASE_OFFSET
        add    r1, r1, r6
        str    r1, _gpu_command_base
        ldr    r1, =(GPIO_BASE_OFFSET + GPLEV0_OFFSET)
        add    r1, r1, r6
        str    r1, _gplev0_base
        bx     lr

_get_hardware_id:
        ldr    r0, _hardware_id
        bx     lr

_get_peripheral_base:
        ldr    r0, _peripheral_base
        bx     lr

_get_gpu_data_base_r4:
        ldr    r4, _gpu_data_0
        bx     lr

_get_gpu_command_base_r10:
        ldr    r10, _gpu_command_base
        bx     lr

_get_GPLEV0_r4:
        ldr    r4, _gplev0_base
        bx     lr

_hardware_id:
        .word 0

_peripheral_base:
        .word 0

_gpu_data_0:
        .word 0

_gpu_command_base:
        .word 0

_gplev0_base:
        .word 0

        .ltorg

.section ".text._get_stack_pointer"
_get_stack_pointer:
    mov     r0, sp
    mov     pc, lr

.section ".text._get_cpsr"
_get_cpsr:
    mrs     r0, cpsr
    mov     pc, lr

.section ".text._init_cycle_counter"
_init_cycle_counter:
    // Enable the cycle counter, and run at the ARM clock rate
    push   {r0, lr}
    bl     _get_hardware_id
    cmp    r0, #_RPI2
    blt    rpi0_1_a
    mov    r0, #7
    mcr    p15, 0, r0, c9, c12, 0
    mov    r0, #(1 << 31)
    mcr    p15, 0, r0, c9, c12, 1
    b      donerpi0_1_a
rpi0_1_a:
    mov    r0, #7
    mcr    p15, 0, r0, c15, c12, 0
donerpi0_1_a:
    pop    {r0, pc}

.section ".text._get_cycle_counter"
_get_cycle_counter:
    push   {r0, lr}
    bl     _get_hardware_id
    cmp    r0, #_RPI2
    blt    rpi0_1_b
    mrc    p15, 0, r0, c9, c13, 0
    b      donerpi0_1_b
rpi0_1_b:
    mrc    p15, 0, r0, c15, c12, 1
donerpi0_1_b:
    pop    {r0, pc}

.section ".text._set_interrupts"
_set_interrupts:
    and     r0, r0, #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT    // extract the IRQ/FIQ bits from the value of cpsr passed in
    mrs     r1, cpsr
    bic     r1, r1, #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT
    orr     r1, r1, r0
    msr     cpsr_c, r1
    mov     pc, lr

.section ".text._enable_interrupts"
_enable_interrupts:
    mrs     r0, cpsr
    bic     r0, r0, #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT
    msr     cpsr_c, r0
    mov     pc, lr

.section ".text._disable_interrupts"
_disable_interrupts:
    mrs     r0, cpsr
    orr     r1, r0, #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT
    msr     cpsr_c, r1
    mov     pc, lr

.section ".text._undefined_instruction_handler_"
_undefined_instruction_handler_:
    stmfd    sp!, {r0-r12, lr}
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0}            // Store spsr onto stack.
    mov      r0, sp
    bl       undefined_instruction_handler

.section ".text._prefetch_abort_handler_"
_prefetch_abort_handler_:
    stmfd    sp!, {r0-r12, lr}
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0}            // Store spsr onto stack.
    mov      r0, sp
    bl       prefetch_abort_handler

.section ".text._data_abort_handler_"
_data_abort_handler_:
    stmfd    sp!, {r0-r12, lr}
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0}            // Store spsr onto stack.
    mov      r0, sp
    bl       data_abort_handler

.section ".text._swi_handler_"
_swi_handler_:
    stmfd    sp!, {r0-r12, lr}
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0}            // Store spsr onto stack.
    mov      r0, sp
    bl       swi_handler

.section ".text._enable_unaligned_access"
_enable_unaligned_access:
    mrc      p15, 0, r0, c1, c0, 0   // read SCTLR
    bic      r0, r0, #2              // A (no unaligned access fault)
    orr      r0, r0, #1 << 22        // U (v6 unaligned access model) not available on arm v8
    mcr      p15, 0, r0, c1, c0, 0   // write SCTLR
    mov      pc, lr

    // Enable L1 Cache -------------------------------------------------------
.section ".text._enable_l1_cache"
_enable_l1_cache:

    // R0 = System Control Register
    mrc p15,0,r0,c1,c0,0

    // Enable caches and branch prediction
    orr r0,#SCTLR_ENABLE_BRANCH_PREDICTION
    orr r0,#SCTLR_ENABLE_DATA_CACHE
    orr r0,#SCTLR_ENABLE_INSTRUCTION_CACHE

    // System Control Register = R0
    mcr p15,0,r0,c1,c0,0

    bx       lr

.section ".text._invalidate_icache"
_invalidate_icache:
    mov      r0, #0
    mcr      p15, 0, r0, c7, c5, 0
    bx       lr

.section ".text._invalidate_dcache"
_invalidate_dcache:
    mov      r0, #0
    mcr      p15, 0, r0, c7, c6, 0
    bx       lr

.section ".text._clean_invalidate_dcache"
_clean_invalidate_dcache:
    mov      r0, #0
    mcr      p15, 0, r0, c7, c14, 0
    bx       lr

.section ".text._invalidate_dcache_mva"
_invalidate_dcache_mva:
    mcr      p15, 0, r0, c7, c6, 1
    bx       lr

.section ".text._clean_invalidate_dcache_mva"
_clean_invalidate_dcache_mva:
    mcr      p15, 0, r0, c7, c14, 1
    bx       lr

.section ".text._invalidate_dtlb"
_invalidate_dtlb:
    mov      r0, #0
    mcr      p15, 0, r0, c8, c6, 0
    bx       lr

.section ".text._invalidate_dtlb_mva"
_invalidate_dtlb_mva:
    mcr      p15, 0, r0, c8, c6, 1
    bx       lr

.section ".text._data_memory_barrier"
_data_memory_barrier:
    push   {r0, lr}
    bl     _get_hardware_id
    cmp    r0, #_RPI2
    blt    rpi0_1_c
    dmb
    b      donerpi0_1_c
rpi0_1_c:
    mov      r0, #0
    mcr      p15, 0, r0, c7, c10, 5
donerpi0_1_c:
    pop    {r0, pc}

#ifdef USE_MULTICORE
.section ".text._init_core"
_init_core:     //only called with multicore Pi models
#if defined(RPI4)
	ldr	r1,=0xff842000
    mov  r2,#0
	str r2,[r1]		// disable GIC on rpi4
#endif

    //1. Set the CPACR for access to CP10 and CP11, and clear the ASEDIS and D32DIS bits:
    ldr     r0, =(0xf << 20)
    mcr     p15, 0, r0, c1, c0, 2

    // 2. Set the FPEXC EN bit to enable the NEON MPE:
    mov     r0, #0x40000000
    vmsr    fpexc, r0

    // if kernel_old=0 enter in HYP mode and need to force a switch to SVC mode
    //
    // for now we assume kernel_old=1 and don't execute this core
    //
    // The logs show:
    // SVC mode: cpsr ends with 1d3
    // HYP mode: cpsr ends with 1a3
    mrs     r0, cpsr
    eor     r0, r0, #CPSR_MODE_HYP
    tst     r0, #CPSR_MODE_MASK
    bic     r0 , r0 , #CPSR_MODE_MASK
    orr     r0 , r0 , #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR
    bne     _init_not_in_hyp_mode
    orr     r0, r0, #CPSR_A_BIT
    adr     lr, _init_continue
    msr     spsr_cxsf, r0
    .word 0xE12EF30E  // msr_elr_hyp lr
    .word 0xE160006E  // eret
_init_not_in_hyp_mode:
    msr    cpsr_c, r0

_init_continue:
    ldr  r4,=_start
    // Initialise Stack Pointers ---------------------------------------------

    // We're going to use interrupt mode, so setup the interrupt mode
    // stack pointer which differs to the application stack pointer:
    msr cpsr_c, #(CPSR_MODE_IRQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_IRQ_STACK

    // Also setup the stack used for FIQs
    msr cpsr_c, #(CPSR_MODE_FIQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_FIQ_STACK

    // Also setup the stack used for undefined exceptions
    msr cpsr_c, #(CPSR_MODE_UNDEFINED | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_UNDEFINED_STACK

    // Also setup the stack used for prefetch and data abort exceptions
    msr cpsr_c, #(CPSR_MODE_ABORT | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_ABORT_STACK

    // Finally, a user/system mode stack, although the application will likely reset this
    msr cpsr_c, #(CPSR_MODE_SYSTEM | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_USER_STACK

    // Switch back to supervisor mode (our application mode) and
    // set the stack pointer. Remember that the stack works its way
    // down memory, our heap will work it's way up from after the
    // application.
    msr cpsr_c, #(CPSR_MODE_SVR | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_SVR_STACK

    bl     run_core
skip_init:
#endif

.section ".text._spin_core"
    // If main does return for some reason, just catch it and stay here.
_spin_core:
#ifdef DEBUG_Multicore
    mov     r0, #'S'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'P'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'I'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'N'
    bl      RPI_AuxMiniUartWrite
    bl     _get_core
    add     r0, r0, #'0'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'\r'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'\n'
    bl      RPI_AuxMiniUartWrite
#endif
_spin_core1:
    wfe
    b       _spin_core1

.section ".text._get_core"
_get_core:
    mrc     p15, 0, r0, c0, c0, 5
    and     r0, #3
    bx      lr

// Default handlers for FIQ/IRQ do nothing

arm_fiq_handler:
arm_irq_handler:
        subs    pc, lr, #4
