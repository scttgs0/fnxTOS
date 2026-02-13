/*
 * bootram.S - Run EmuTOS from RAM
 *
 * Copyright (C) 2016-2019 The EmuTOS development team
 *
 * Authors:
 *  LVL  Laurent Vogel
 *  VRI  Vincent Rivière
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/*
 * This code is common to all RAM loaders.
 * It may be included in a bootsector, so everything must be pc-relative.
 * It may also run on foreign operating system, so there must not be any usage
 * of TOS features.
 */

 #include "asmdefs.h"

        .globl  _bootram
/*
 * void bootram(const UBYTE *src, ULONG size, ULONG cpu);
 *
 * Copy emutos.img from a temporary buffer to its hardcoded location,
 * then run it.
 * The temporary ramtos starts at 'src', for 'size' bytes.
 * 'cpu' is 0 for 68000, 30 for 68030, etc.
 *
 * This needs to be done in assembly language, because the copy routine
 * may need to be copied to a safe place to avoid being overwritten
 * when ramtos is copied to its final destination.
 */

_bootram:
        // Disable the interrupts, since we are going to destroy everything
        move.w  #0x2700,sr

        // Get the parameters from the stack
        move.l  4(sp),a4        // src
        move.l  8(sp),d4        // size
        move.l  12(sp),d5       // cpu

        // Get destination address from the OS header
        move.l  8(a4),a5        // dest = _os_beg

        // Round size to next long
        addq.l  #3,d4
        andi.l  #-4,d4

        // If the end of the boot routine is before the destination buffer,
        // then it is already in a safe place. It will not be overwritten
        // so it can be run from its current location.
        lea     endboot(pc),a1
        cmp.l   a1,a5           // compare dest to end of boot routine
        jhs     boot            // already safe place

        move.l  a5,a0           // dest
        add.l   d4,a0           // + size

        // If the start of the boot routine is after the destination buffer,
        // then it is already in a safe place. It will not be overwritten
        // so it can be run from its current location.
        lea     boot(pc),a1
        cmp.l   a1,a0           // compare end of dest to begin of boot routine
        jls     boot            // already safe place

        // The boot routine is going to be overwritten by ramtos copy.
        // Move it after both the destination buffer and the source buffer.
        // Compute the highest address in a1 for destination of boot routine.
        move.l  a4,a1           // src
        add.l   d4,a1           // + size

        cmp.l   a0,a1           // compare end of src to end of dest
        jhs     copyboot        // end of src is the highest address

        move.l  a0,a1           // end of dest is the highest address

copyboot:
        lea     boot(pc),a2     // src boot
        move.l  a1,a3           // dest boot
        moveq   #((endboot-boot)+3)&-4,d2   // size boot rounded to next long
        move.l  a3,a6           // return to destination
        jra     copy_a2_a3_d2_sync_caches

boot:
        // This boot is a first boot
        clr.l   0x6fc.w         // warm_magic

        // Prevent reset vector from being called during ramtos startup.
        // This must be done here, because ramtos checks for magic values
        // very early, even before clearing sysvars.
        clr.l   0x426.w         // resvalid

        // Copy emutos.img to its final destination
        move.l  a4,a2           // src
        move.l  a5,a3           // dest
        move.l  d4,d2           // size
        move.l  4(a2),a6        // return to ramtos reset vector

        // Fall into copy_a2_a3_d2_sync_caches

/*
 * Copy buffer from a2 to a3 with size d2,
 * synchronize the CPU instruction and data caches,
 * then return to a6.
 */
copy_a2_a3_d2_sync_caches:
        cmp.l   a2,a3
        jhi     bcopy_backwards

        lsr.l   #2,d2
        subq.l  #1,d2
bcopy:
        move.l  (a2)+,(a3)+
        dbra    d2,bcopy
        sub.l   #0x10000,d2
        jhi     bcopy
        jra     sync_caches

bcopy_backwards:
        add.l   d2,a2   // after last source long
        add.l   d2,a3   // after last destination long
        lsr.l   #2,d2
        subq.l  #1,d2
bcopy2:
        move.l  -(a2),-(a3)
        dbra    d2,bcopy2
        sub.l   #0x10000,d2
        jhi     bcopy2

sync_caches:
        moveq   #20,d0
        cmp.l   d0,d5
        jeq     inv_020or030
        moveq   #30,d0
        cmp.l   d0,d5
        jne     not68030
inv_020or030:
        MOVEC_CACR_D0
        ori.b   #0x08,d0        // invalidate instruction cache
        MOVEC_D0_CACR
not68030:
        jmp     (a6)            // return without using the stack

endboot:
