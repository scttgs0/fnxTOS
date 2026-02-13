/*
 * memmove.S - a fast memmove/memcpy for 68000
 *
 * Copyright (C) 2001 by Authors:
 * Copyright (C) 2013-2020 by The EmuTOS development team.
 *
 * Authors:
 *  LVL  Laurent Vogel
 *  DAL  Dale Schumacher
 *  KS   Klamer Schutte
 *  RAL  ???
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#include "asmdefs.h"

// This is taken from Minix 1.5 copy_68k.s fast copy routine.
// extracted phys_copy() and converted to modern syntax,
// Laurent Vogel, october 2001.
//
//****************************************************************************
//
//     C O P Y _ 6 8 K . S                                       M I N I X
//
//     Basic fast copy routines used by the kernel
//****************************************************************************
//
// Contents:
//
//   flipclicks   exchange two block of clicks
//   zeroclicks   zero a block of clicks
//   copyclicks   copy a block of clicks
//   phys_copy    copy a block of bytes
//
//============================================================================
// Edition history
//
//  #    Date                         Comments                       By
// --- -------- ---------------------------------------------------- ---
//   1 13.06.89 fast phys_copy by Dale Schumacher                    DAL
//   2 16.06.89 bug fixes and code impromvement by Klamer Schutte    KS
//   3 12.07.89 bug fix and further code improvement to phys_copy    RAL
//   4 14.07.89 flipclicks,zeroclicks,copyclicks added               RAL
//   5 15.07.89 fast copy routine for messages added to phys_copy    RAL
//   6 03.08.89 clr.l <ea> changed to move.l #0,<ea> (= moveq )      RAL
//
//****************************************************************************

#define BREAKEVEN 12    // dividing line between simple & complicated
                        // versions of memcpy(): good for 68000 & 68030

        .globl  _memmove
        .globl  _memcpy

        .text

//
// void * memmove(void * dst, void * src, size_t length);
// moves length bytes from src to dst, performing correctly
// if the two regions overlap. returns dst as passed.

//
// void * memcpy(void * RESTRICT dst, void * RESTRICT src, size_t length);
// moves length bytes from src to dst. returns dst as passed.
// the behaviour is undefined if the two regions overlap.

_memcpy:
        move.l   8(sp),a0        // load source pointer
        move.l   4(sp),a1        // load destination pointer
        move.l   12(sp),d0       // get length
        cmpi.l   #BREAKEVEN,d0   // is it worth doing the complicated version?
        jge      memmove         // yes, branch
        jra      sdecr
sloop:
        move.b   (a0)+,(a1)+
sdecr:
        dbra     d0,sloop
        move.l   4(sp),d0        // return destination pointer
        rts

_memmove:
        move.l   8(sp),a0        // load source pointer
        move.l   4(sp),a1        // load destination pointer
memmove:
        cmp.l    a1,a0
        jgt      memcopy         // if src > dst, copy will do fine.
        jeq      end             // if src == dst, nothing to do.
// now, src < dst.
        move.l   a0,d0
        move.l   a1,d1
        eor.b    d1,d0
        btst     #0,d0           // pointers mutually aligned?
        jne      back1
        move.l   12(sp),d0       // size
        jeq      end             // if size == 0, nothing to do
        add.l    d0,a0
        add.l    d0,a1
        move.l   a0,d1
        btst     #0,d1
        jeq      bcheck64
        move.b   -(a0),-(a1)
        sub.l    #1,d0
        jeq      end
bcheck64:
        move.l   #63,d1                // +
        cmp.l    d1,d0                 // +
        jle      back4                 // + count < 64
        movem.l  d2-d7/a2-a6,-(sp)     // save regs for movem use
        move.b   d0,d1                 // count mod 256
        lsr.l    #8,d0                 // count div 256
        jra      bend256
bloop256:
        lea      -256(a0),a0
        lea      -256(a1),a1
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,(a1)
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,44(a1)
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,88(a1)
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,132(a1)
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,176(a1)
        movem.l  (a0)+,d2-d7/a2-a4     // copy  9x4 bytes
        movem.l  d2-d7/a2-a4,220(a1)
        lea      -256(a0),a0

bend256:
        dbra     d0,bloop256           // decrement count, test and loop
        move.l   d1,d0                 // remainder becomes new count
        jeq      done                  // more to copy? no!
        and.b    #0x3F,d1              // + count mod 64
        lsr.b    #6,d0                 // + count div 64
        jra      bend64
bloop64:
        lea      -64(a0),a0
        lea      -64(a1),a1
        movem.l  (a0)+,d2-d7/a4-a5     // copy 8x4 bytes
        movem.l  d2-d7/a4-a5,(a1)
        movem.l  (a0)+,d2-d7/a4-a5     // copy 8x4 bytes
        movem.l  d2-d7/a4-a5,32(a1)
        lea      -64(a0),a0

bend64:
        dbra     d0,bloop64            // decrement count, test and loop
        movem.l  (a7)+,d2-d7/a2-a6     // restore regs for movem use
        move.l   d1,d0                 // remainder becomes new count
back4:
        move.b   d0,d1                 // +
        and.b    #3,d1                 // +
        lsr.b    #2,d0                 // +
        jra      bend4
bloop4:
        move.l   -(a0),-(a1)
bend4:
        dbra     d0,bloop4              // decrement count, test and loop
        move.l   d1,d0                  // remainder becomes new count
        jra      bend1
bloop1:
        move.b   -(a0),-(a1)
bend1:
        dbra     d0,bloop1              // decrement count, test and loop
end:
        move.l   4(sp),d0               // return destination pointer
        rts

// backwards, when pointers are not aligned.
back1:
        move.l   12(sp),d0       // size
        jeq      end             // if size == 0, nothing to do
// backwards, but 16 bytes by 16 bytes forward.
        add.l    d0,a0
        add.l    d0,a1
        move.l   #16,d1
        cmp.l    d1,d0
        jlt      bend1
back16:
        move.b   d0,d1
        and.b    #0x0F,d1
        lsr.l    #4,d0
        jra      bend16
bloop16:
        lea      -16(a0),a0
        lea      -16(a1),a1
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        lea      -16(a0),a0
        lea      -16(a1),a1
bend16:
        dbra     d0,bloop16
        sub.l    #0x10000,d0           // count can even be bigger (>1MB)
        jhi      bloop16               // (dbra handles only word counters)
        move.l   d1,d0
        jra      bend1

memcopy:
        move.l   a0,d0
        move.l   a1,d1
        eor.b    d1,d0
        btst     #0,d0                 // pointers mutually aligned?
        jne      copy1                 // +
        move.l   12(sp),d0             // +
        jeq      end                   // if cnt == 0 && pointers both odd ...
        btst     #0,d1                 // pointers aligned, but odd?
        jeq      check64               // no
        move.b   (a0)+,(a1)+           // copy odd byte
        sub.l    #1,d0                 // decrement count
check64:
        move.l   #63,d1                // +
        cmp.l    d1,d0                 // +
        jle      copy4                 // + count < 64
        movem.l  d2-d7/a2-a6,-(a7)     // save regs for movem use
        move.b   d0,d1                 // count mod 256
        lsr.l    #8,d0                 // count div 256
        jra      end256
loop256:
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,(a1)
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,44(a1)
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,88(a1)
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,132(a1)
        movem.l  (a0)+,d2-d7/a2-a6     // copy 11x4 bytes
        movem.l  d2-d7/a2-a6,176(a1)
        movem.l  (a0)+,d2-d7/a2-a4     // copy  9x4 bytes
        movem.l  d2-d7/a2-a4,220(a1)
        lea      256(a1),a1
end256:
        dbra     d0,loop256            // decrement count, test and loop
        move.l   d1,d0                 // remainder becomes new count
        jeq      done                  // more to copy? no!
        and.b    #0x3F,d1              // + count mod 64
        lsr.b    #6,d0                 // + count div 64
        jra      end64
done:
        movem.l  (a7)+,d2-d7/a2-a6     // restore regs for movem use
        jra      end

loop64:
        movem.l  (a0)+,d2-d7/a4-a5     // copy 8x4 bytes
        movem.l  d2-d7/a4-a5,(a1)
        movem.l  (a0)+,d2-d7/a4-a5     // copy 8x4 bytes
        movem.l  d2-d7/a4-a5,32(a1)
        lea      64(a1),a1
end64:
        dbra     d0,loop64             // decrement count, test and loop
        movem.l  (a7)+,d2-d7/a2-a6     // restore regs for movem use
        move.l   d1,d0                 // remainder becomes new count
copy4:
        move.b   d0,d1                 // +
        and.b    #3,d1                 // +
        lsr.b    #2,d0                 // +
        jra      end4
loop4:
        move.l   (a0)+,(a1)+
end4:
        dbra     d0,loop4              // decrement count, test and loop
        move.l   d1,d0                 // remainder becomes new count
        jra      end1
loop1:
        move.b   (a0)+,(a1)+
end1:
        dbra     d0,loop1              // decrement count, test and loop
        move.l   4(sp),d0              // return destination pointer
        rts

copy1:
        move.l   12(sp),d0
                                       // count can be big; test on it !
        move.l   #16,d1                // == moveq; 4
        cmp.l    d1,d0                 // 6
        jlt      end1
copy16:
        move.b   d0,d1
        and.b    #0x0F,d1
        lsr.l    #4,d0
        jra      end16
loop16:
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
        move.b   (a0)+,(a1)+
end16:
        dbra     d0,loop16
        sub.l    #0x10000,d0           // count can even be bigger (>1MB)
        jhi      loop16                // (dbra handles only word counters)
        move.l   d1,d0
        jra      end1
