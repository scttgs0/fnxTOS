/*
 * memory2.c - Memory functions
 *
 * Copyright (C) 2016-2022 The EmuTOS development team
 *
 * Authors:
 *  VRI   Vincent Rivière
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/* #define ENABLE_KDEBUG */

#include "emutos.h"
#include "asm.h"
#include "memory.h"
#include "tosvars.h"
#include "machine.h"
#include "has.h"
#include "cookie.h"
#include "biosext.h"    /* for cache control routines */
#include "bios.h"
#include "vectors.h"
#include "../bdos/bdosstub.h"
#include "string.h"

#define ZONECOUNT   32      /* for memory test */

UBYTE meminit_flags;

#if CONF_WITH_ALT_RAM

/* Initialize all Alt-RAM */
void altram_init(void)
{
#if CONF_WITH_STATIC_ALT_RAM && defined(STATIC_ALT_RAM_SIZE)
    KDEBUG(("xmaddalt() static adr=%p size=%ld\n",
        (UBYTE *)STATIC_ALT_RAM_ADDRESS, STATIC_ALT_RAM_SIZE));
    xmaddalt((UBYTE *)STATIC_ALT_RAM_ADDRESS, STATIC_ALT_RAM_SIZE);
    return;
#endif
}

#endif /* CONF_WITH_ALT_RAM */

#if CONF_WITH_MEMORY_TEST

/*
 * test one memory 'zone' (contiguous memory area)
 *
 * returns TRUE if ok, FALSE if error
 */
static BOOL testzone(UBYTE *start, LONG size)
{
    ULONG *p;
    ULONG *end = (ULONG *)(start + size);
    ULONG n;

    /* set all bits on & verify */
    memset(start, 0xff, size);
    if (!memtest_verify((ULONG *)start, 0xffffffffUL, size))
        return FALSE;

    /* rotate bit & verify */
    /* note: converting the setup part to assembler gives only a slight speedup */
    for (p = (ULONG *)start, n = 1; p < end; )
    {
        roll(n,1);
        *p++ = n;
    }
    if (!memtest_rotate_verify((ULONG *)start, size))
        return FALSE;

    /* set all bits off & verify */
    memset(start, 0x00, size);
    if (!memtest_verify((ULONG *)start, 0UL, size))
        return FALSE;

    return TRUE;
}

static void init_line()
{
    /* disable line wrap, display title, switch to inverse video */
    cprintf("\x1bwST RAM \x1bp");
    /* save cursor posn, display 32 spaces, restore cursor posn */
    cprintf("\x1bj%32s\x1bk", " ");
}

static void end_line(LONG memsize)
{
    char type;

    /*
     * we display the size as KB when under 4MB, to allow for
     * sizes like 512K or 2.5MB
     */
    if (memsize < 4*1024*1024L)
    {
        memsize >>= 10;     /* convert to KB */
        type = 'K';
    }
    else
    {
        memsize >>= 20;     /* convert to MB */
        type = 'M';
    }

    /* backspace, display memory size, disable inverse video */
    cprintf("\b\b\b\b\b\b\b\b%5ld %cB\n\x1bq",memsize,type);
}

/*
 * test one type of RAM (ST RAM or TT RAM)
 */
static BOOL testtype(BOOL is_ttram, LONG memsize)
{
    UBYTE *testaddr, *startaddr;
    LONG zonesize;
    WORD i;
    BOOL ok;

    init_line();
    startaddr = (UBYTE *)0L;
    zonesize = (memsize / ZONECOUNT);
    for (i = 0, testaddr = startaddr; i < ZONECOUNT; i++, testaddr += zonesize)
    {
        ok = TRUE;
        /* we skip testing areas in use by the system! */
        if ((testaddr >= membot) && (testaddr+zonesize <= memtop))
            ok = testzone(testaddr, zonesize);
        cprintf(ok?"-":"X");
        if (bconstat(2))    /* abort */
        {
            bconin(2);
            cprintf("\n\x1bq"); /* new line, disable inverse video */
            return FALSE;
        }
    }
    end_line(memsize);      /* display memory size */

    return TRUE;
}

/*
 * perform a simple memory test with visual feedback and the option to
 * abort.  we test ST RAM, followed by TT RAM.  within each type, we
 * test a 'zone' of memory at a time, where a zone is 1/32 of the total
 * amount and is assumed to be an even length, aligned on an even boundary.
 *
 * returns TRUE if OK, FALSE if aborted
 */
BOOL memory_test(void)
{
    /* handle ST RAM */
    if (!testtype(FALSE, (LONG)phystop))
        return FALSE;

    return TRUE;
}

#endif
