/*
 * mkrom.c - Create an EmuTOS ROM image
 *
 * Copyright (C) 2012-2024 The EmuTOS development team
 *
 * Authors:
 *  VRI   Vincent Rivière
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/*
 * This tool adds padding bytes to an EmuTOS binary image,
 * and also creates special ROM formats.
 */

#define DBG_MKROM 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define SIZE_ERROR ((size_t)-1)
#define MIN(a, b) ((a)<=(b) ? (a) : (b))
#define BUFFER_SIZE (16*1024)

/* Command-line commands */
typedef enum
{
    CMD_NONE = 0,
    CMD_PAD,
    CMD_PAK3,
    CMD_STC
} CMD_TYPE;

/* Global variables */
static const char* g_argv0; /* Program name */
static uint8_t g_buffer[BUFFER_SIZE]; /* Global buffer to minimize stack usage */

/* Get an integer value from an integer string with a k, m, or g suffix */
static size_t get_size_value(const char* strsize)
{
    unsigned long val;
    char suffix;
    char tail; /* Will only be affected if the suffix is too long */
    int ret;

    ret = sscanf(strsize, "%lu%c%c", &val, &suffix, &tail);
    if (ret == 1) /* No suffix */
        ; /* val is already a number of bytes */
    else if (ret == 2 && (suffix == 'k' || suffix == 'K'))
        val *= 1024;
    else if (ret == 2 && (suffix == 'm' || suffix == 'M'))
        val *= 1024 * 1024;
    else if (ret == 2 && (suffix == 'g' || suffix == 'G'))
        val *= 1024 * 1024 * 1024;
    else
    {
        fprintf(stderr, "%s: %s: invalid size.\n", g_argv0, strsize);
        return SIZE_ERROR;
    }

    return (size_t)val;
}

/* Get the size of an open file */
static size_t get_file_size(FILE* file, const char* filename)
{
    long initial_pos; /* Initial file position */
    int err; /* Seek error */
    long end_pos; /* End file position */

    /* Remember the initial position */
    initial_pos = ftell(file);
    if (initial_pos == -1)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, filename, strerror(errno));
        return SIZE_ERROR;
    }

    /* Seek to end of file */
    err = fseek(file, 0, SEEK_END);
    if (err != 0)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, filename, strerror(errno));
        return SIZE_ERROR;
    }

    /* Get the end file position */
    end_pos = ftell(file);
    if (end_pos == -1)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, filename, strerror(errno));
        return SIZE_ERROR;
    }

    /* Restore the initial file position */
    err = fseek(file, initial_pos, SEEK_SET);
    if (err != 0)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, filename, strerror(errno));
        return SIZE_ERROR;
    }

    /* The end position is the file size */
    return (size_t)end_pos;
}

/* Write a block of identical bytes to a file */
static int write_byte_block(FILE* outfile, const char* outfilename, uint8_t value, size_t count)
{
    size_t towrite; /* Number of bytes to write this time */
    size_t written; /* Number of bytes written this time */

    memset(g_buffer, value, MIN(BUFFER_SIZE, count));
    while (count > 0)
    {
        towrite = MIN(BUFFER_SIZE, count);
        written = fwrite(g_buffer, 1, towrite, outfile);
        if (written != towrite)
        {
            fprintf(stderr, "%s: %s: %s\n", g_argv0, outfilename, strerror(errno));
            return 0;
        }

        count -= written;
    }

    return 1;
}

/* Copy a stream into another one */
static int copy_stream(FILE* infile, const char* infilename,
                       FILE* outfile, const char* outfilename,
                       size_t count)
{
    size_t toread; /* Number of bytes to read this time */
    size_t towrite; /* Number of bytes to write this time */
    size_t written; /* Number of bytes written this time */

    for(;;)
    {
        toread = MIN(BUFFER_SIZE, count);
        if (toread == 0)
            break;

        towrite = fread(g_buffer, 1, toread, infile);
        if (towrite == 0)
        {
            if (ferror(infile))
            {
                fprintf(stderr, "%s: %s: %s\n", g_argv0, infilename, strerror(errno));
                return 0;
            }
            else
            {
                fprintf(stderr, "%s: %s: premature end of file.\n", g_argv0, infilename);
                return 0;
            }
        }

        written = fwrite(g_buffer, 1, towrite, outfile);
        if (written != towrite)
        {
            fprintf(stderr, "%s: %s: %s\n", g_argv0, outfilename, strerror(errno));
            return 0;
        }

        count -= written;
    }

    return 1;
}

/* Append a file into a stream, and pad it with zeros */
static int append_and_pad(FILE* infile, const char* infilename,
                          FILE* outfile, const char* outfilename,
                          size_t target_size, size_t *psource_size)
{
    size_t free_size;
    int ret; /* boolean return value: 0 == error, 1 == OK */

    /* Get the input file size */
    *psource_size = get_file_size(infile, infilename);
    if (*psource_size == SIZE_ERROR)
        return 0;

    /* Check if the input file size is not too big */
    if (*psource_size > target_size)
    {
        fprintf(stderr, "%s: %s is too big: %lu extra bytes\n", g_argv0, infilename, (unsigned long)(*psource_size - target_size));
        return 0;
    }

    /* Copy the input file */
    ret = copy_stream(infile, infilename, outfile, outfilename, *psource_size);
    if (!ret)
        return ret;

    /* Pad with zeros */
    free_size = target_size - *psource_size;
    ret = write_byte_block(outfile, outfilename, 0, free_size);
    if (!ret)
        return ret;

    return 1;
}

/* Copy and pad with zeros up to target_size */
static int cmd_pad(FILE* infile, const char* infilename,
                   FILE* outfile, const char* outfilename,
                   size_t target_size)
{
    size_t source_size;
    size_t free_size;
    int ret; /* boolean return value: 0 == error, 1 == OK */

    printf("# Padding %s to %ld KB image into %s\n", infilename, ((long)target_size) / 1024, outfilename);

    ret = append_and_pad(infile, infilename, outfile, outfilename, target_size, &source_size);
    if (!ret)
        return ret;

    free_size = target_size - source_size;
    printf("# %s done (%lu bytes free)\n", outfilename, (unsigned long)free_size);

    return 1;
}

/* PAK/3 512kB image */
static int cmd_pak3(FILE* infile, const char* infilename,
                   FILE* outfile, const char* outfilename)
{
    size_t source_size;
    size_t max_size = 256 * 1024;       /* input must be a 256KB image */
    size_t target_size = 512 * 1024;    /* which we pad to 512KB (and patch) */
    long jmp_address = 0x40030L;
    const unsigned char jmp_instr[] = { 0x4e, 0xf9, 0x00, 0xe0, 0x00, 0x00 };
    int ret; /* boolean return value: 0 == error, 1 == OK */

    printf("# Padding %s to %ld KB image into %s\n", infilename, ((long)target_size) / 1024, outfilename);

    /* Get the input file size */
    source_size = get_file_size(infile, infilename);
    if (source_size == SIZE_ERROR)
        return 0;

    /* Check if the input file size is too big */
    if (source_size > max_size)
    {
        fprintf(stderr, "%s: %s is too big: %lu extra bytes\n", g_argv0, infilename, (unsigned long)(source_size - max_size));
        return 0;
    }

    ret = append_and_pad(infile, infilename, outfile, outfilename, target_size, &source_size);
    if (!ret)
        return ret;

    /* Rewind to the patch address */
    if (fseek(outfile, jmp_address, SEEK_SET) != 0)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, outfilename, strerror(errno));
        return 0;
    }

    /* Write the 'JMP' instruction */
    if (fwrite(jmp_instr, 1, sizeof jmp_instr, outfile) != sizeof jmp_instr)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, outfilename, strerror(errno));
        return 0;
    }

    printf("# %s done\n", outfilename);

    return 1;
}

/* Steem Engine cartridge image */
static int cmd_stc(FILE* infile, const char* infilename,
                   FILE* outfile, const char* outfilename)
{
    size_t source_size;
    size_t target_size = 128 * 1024;
    size_t free_size;
    int ret; /* boolean return value: 0 == error, 1 == OK */

    printf("# Padding %s to %ld KB Steem Engine cartridge image into %s\n", infilename, ((long)target_size) / 1024, outfilename);

    /* Get the input file size */
    source_size = get_file_size(infile, infilename);
    if (source_size == SIZE_ERROR)
        return 0;

    /* Check if the input file size is not too big */
    if (source_size > target_size)
    {
        fprintf(stderr, "%s: %s is too big: %lu extra bytes\n", g_argv0, infilename, (unsigned long)(source_size - target_size));
        return 0;
    }

    /* Insert a long zero at the beginning */
    ret = write_byte_block(outfile, outfilename, 0, 4);
    if (!ret)
        return ret;

    /* Copy the input file */
    ret = copy_stream(infile, infilename, outfile, outfilename, source_size);
    if (!ret)
        return ret;

    /* Pad with zeros */
    free_size = target_size - source_size;
    ret = write_byte_block(outfile, outfilename, 0, free_size);
    if (!ret)
        return ret;

    printf("# %s done (%lu bytes free)\n", outfilename, (unsigned long)free_size);

    return 1;
}

/* Header of Apple Disk Copy 4.2 disk image.
 * https://wiki.68kmla.org/DiskCopy_4.2_format_specification
 * Each sector has a size of 512 bytes,
 * plus an extra area of 12 bytes called "tag".
 */
struct dc42_header
{
    char pascal_name[64]; /* Image name, first byte is length, padded with 0 */
    uint32_t data_size; /* Size of Data Block */
    uint32_t tag_size; /* Size of Tag Block */
    uint32_t data_checksum; /* Checksum of Data Block */
    uint32_t tag_checksum; /* Checksum of Tag Block */
    uint8_t encoding; /* Disk encoding */
    uint8_t format; /* Format byte */
    uint16_t magic; /* Magic number */
};

#define DC42_ENCODING_GCR_SSDD 0x00 /* GCR 400 KB */
#define DC42_FORMAT_MAC400K 0x02
#define DC42_MAGIC 0x0100

/* Main program */
int main(int argc, char* argv[])
{
    const char* bootfilename = NULL;
    FILE* bootfile = NULL;
    const char* infilename;
    FILE* infile;
    const char* outfilename;
    FILE* outfile;
    size_t target_size = 0;
    int err; /* stdio error: 0 == OK, EOF == error */
    int ret; /* boolean return value: 0 == error, 1 == OK */
    CMD_TYPE op = CMD_NONE;
    const char* outmode = "wb"; /* By default, write only */

    g_argv0 = argv[0]; /* Remember the program name */

    if (argc == 5 && !strcmp(argv[1], "pad"))
    {
        op = CMD_PAD;

        target_size = get_size_value(argv[2]);
        if (target_size == SIZE_ERROR)
            return 1;

        infilename = argv[3];
        outfilename = argv[4];
    }
    else if (argc == 4 && !strcmp(argv[1], "pak3"))
    {
        op = CMD_PAK3;
        infilename = argv[2];
        outfilename = argv[3];
    }
    else if (argc == 4 && !strcmp(argv[1], "stc"))
    {
        op = CMD_STC;
        infilename = argv[2];
        outfilename = argv[3];
    }
    else
    {
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "  # Generic zero padding\n");
        fprintf(stderr, "  %s pad <size> <source> <destination>\n", g_argv0);
        fprintf(stderr, "\n");
        fprintf(stderr, "  # Steem Engine cartridge image\n");
        fprintf(stderr, "  %s stc <source.img> <destination.stc>\n", g_argv0);
        fprintf(stderr, "\n");
        fprintf(stderr, "  # PAK/3 image\n");
        fprintf(stderr, "  %s pak3 <source.img> <destination.img>\n", g_argv0);
        return 1;
    }

    /* Open the boot file (if present) */
    if (bootfilename)
    {
        bootfile = fopen(bootfilename, "rb");
        if (bootfile == NULL)
        {
            fprintf(stderr, "%s: %s: %s\n", g_argv0, bootfilename, strerror(errno));
            return 1;
        }
    }

    /* Open the source file */
    infile = fopen(infilename, "rb");
    if (infile == NULL)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, infilename, strerror(errno));
        return 1;
    }

    /* Open the destination file */
    outfile = fopen(outfilename, outmode);
    if (outfile == NULL)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, outfilename, strerror(errno));
        return 1;
    }

    switch (op)
    {
        case CMD_PAD:
            ret = cmd_pad(infile, infilename, outfile, outfilename, target_size);
        break;

        case CMD_PAK3:
            ret = cmd_pak3(infile, infilename, outfile, outfilename);
        break;

        case CMD_STC:
            ret = cmd_stc(infile, infilename, outfile, outfilename);
        break;

        default:
            abort(); /* Should not happen */
        break;
    }

    if (!ret)
    {
        /* Error message already written */
        fclose(outfile);
        remove(outfilename);
        return 1;
    }

    /* Close the output file */
    err = fclose(outfile);
    if (err != 0)
    {
        remove(outfilename);
        fprintf(stderr, "%s: %s: %s\n", g_argv0, outfilename, strerror(errno));
        return 1;
    }

    /* Close the input file */
    err = fclose(infile);
    if (err != 0)
    {
        fprintf(stderr, "%s: %s: %s\n", g_argv0, infilename, strerror(errno));
        return 1;
    }

    /* Close the boot file (if present) */
    if (bootfilename)
    {
        err = fclose(bootfile);
        if (err != 0)
        {
            fprintf(stderr, "%s: %s: %s\n", g_argv0, bootfilename, strerror(errno));
            return 1;
        }
    }

    return 0;
}
