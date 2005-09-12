/* Copyright (C) 2000-2005 MySQL AB & Innobase Oy

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

/*
  InnoDB offline file checksum utility.  85% of the code in this file
  was taken wholesale fron the InnoDB codebase.

  The final 15% was originally written by Mark Smith of Danga
  Interactive, Inc. <junior@danga.com>

  Published with a permission.
*/

// needed to have access to 64 bit file functions
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// all of these ripped from InnoDB code from MySQL 4.0.22
#define UT_HASH_RANDOM_MASK     1463735687
#define UT_HASH_RANDOM_MASK2    1653893711
#define FIL_PAGE_LSN          16 
#define FIL_PAGE_FILE_FLUSH_LSN 26
#define FIL_PAGE_OFFSET     4
#define FIL_PAGE_DATA       38
#define FIL_PAGE_END_LSN_OLD_CHKSUM 8
#define FIL_PAGE_SPACE_OR_CHKSUM 0
#define UNIV_PAGE_SIZE          (2 * 8192)

// command line argument to do page checks (that's it)
// another argument to specify page ranges... seek to right spot and go from there

typedef unsigned long int ulint;
typedef unsigned char byte;

/* innodb function in name; modified slightly to not have the ASM version (lots of #ifs that didn't apply) */
ulint mach_read_from_4(byte *b) {
    return( ((ulint)(b[0]) << 24)
        + ((ulint)(b[1]) << 16)
        + ((ulint)(b[2]) << 8)
        + (ulint)(b[3])
          );
}

ulint
ut_fold_ulint_pair(
/*===============*/
            /* out: folded value */
    ulint   n1, /* in: ulint */
    ulint   n2) /* in: ulint */
{
    return(((((n1 ^ n2 ^ UT_HASH_RANDOM_MASK2) << 8) + n1)
                        ^ UT_HASH_RANDOM_MASK) + n2);
}

ulint
ut_fold_binary(
/*===========*/
            /* out: folded value */
    byte*   str,    /* in: string of bytes */
    ulint   len)    /* in: length */
{
    ulint   i;
    ulint   fold = 0;

    for (i = 0; i < len; i++) {
        fold = ut_fold_ulint_pair(fold, (ulint)(*str));

        str++;
    }

    return(fold);
}

ulint
buf_calc_page_new_checksum(
/*=======================*/
               /* out: checksum */
    byte*    page) /* in: buffer page */
{
    ulint checksum;

    /* Since the fields FIL_PAGE_FILE_FLUSH_LSN and ..._ARCH_LOG_NO
    are written outside the buffer pool to the first pages of data
    files, we have to skip them in the page checksum calculation.
    We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
    checksum is stored, and also the last 8 bytes of page because
    there we store the old formula checksum. */

    checksum = ut_fold_binary(page + FIL_PAGE_OFFSET,
                 FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET)
           + ut_fold_binary(page + FIL_PAGE_DATA,
                           UNIV_PAGE_SIZE - FIL_PAGE_DATA
                           - FIL_PAGE_END_LSN_OLD_CHKSUM);
    checksum = checksum & 0xFFFFFFFF;

    return(checksum);
}

ulint
buf_calc_page_old_checksum(
/*=======================*/
               /* out: checksum */
    byte*    page) /* in: buffer page */
{
    ulint checksum;

    checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);

    checksum = checksum & 0xFFFFFFFF;

    return(checksum);
}


int main(int argc, char **argv) {
    FILE *f; // our input file
    byte *p; // storage of pages read
    int bytes; // bytes read count
    ulint ct; // current page number (0 based)
    int now; // current time
    int lastt; // last time
    ulint oldcsum, oldcsumfield, csum, csumfield, logseq, logseqfield; // ulints for checksum storage
    struct stat st; // for stat, if you couldn't guess
    unsigned long long int size; // size of file (has to be 64 bits)
    ulint pages; // number of pages in file
    ulint start_page = 0, end_page = 0, use_end_page = 0; // for starting and ending at certain pages
    off_t offset = 0;
    int just_count = 0; // if true, just print page count
    int verbose = 0;
    int debug = 0;
    int c;
    int fd;

    // remove arguments
    while ((c = getopt(argc, argv, "cvds:e:p:")) != -1) {
        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case 'c':
                just_count = 1;
                break;
            case 's':
                start_page = atoi(optarg);
                break;
            case 'e':
                end_page = atoi(optarg);
                use_end_page = 1;
                break;
            case 'p':
                start_page = atoi(optarg);
                end_page = atoi(optarg);
                use_end_page = 1;
                break;
            case 'd':
                debug = 1;
                break;
            case ':':
                fprintf(stderr, "option -%c requires an argument\n", optopt);
                return 1;
                break;
            case '?':
                fprintf(stderr, "unrecognized option: -%c\n", optopt);
                return 1;
                break;
        }
    }

    // debug implies verbose...
    if (debug) verbose = 1;

    // make sure we have the right arguments
    if (optind >= argc) {
	printf("InnoDB offline file checksum utility.\n");
        printf("usage: %s [-c] [-s <start page>] [-e <end page>] [-p <page>] [-v] [-d] <filename>\n", argv[0]);
        printf("\t-c\tprint the count of pages in the file\n");
        printf("\t-s n\tstart on this page number (0 based)\n");
        printf("\t-e n\tend at this page number (0 based)\n");
        printf("\t-p n\tcheck only this page (0 based)\n");
        printf("\t-v\tverbose (prints progress every 5 seconds)\n");
        printf("\t-d\tdebug mode (prints checksums for each page)\n");
        return 1;
    }

    // stat the file to get size and page count
    if (stat(argv[optind], &st)) {
        perror("error statting file");
        return 1;
    }
    size = st.st_size;
    pages = size / UNIV_PAGE_SIZE;
    if (just_count) {
        printf("%lu\n", pages);
        return 0;
    } else if (verbose) {
        printf("file %s = %llu bytes (%lu pages)...\n", argv[1], size, pages);
        printf("checking pages in range %lu to %lu\n", start_page, use_end_page ? end_page : (pages - 1));
    }

    // open the file for reading
    f = fopen(argv[optind], "r");
    if (!f) {
        perror("error opening file");
        return 1;
    }

    // seek to the necessary position
    if (start_page) {
        fd = fileno(f);
        if (!fd) {
            perror("unable to obtain file descriptor number");
            return 1;
        }

        offset = (off_t)start_page * (off_t)UNIV_PAGE_SIZE;

        if (lseek(fd, offset, SEEK_SET) != offset) {
            perror("unable to seek to necessary offset");
            return 1;
        }
    }

    // allocate buffer for reading (so we don't realloc every time)
    p = (byte *)malloc(UNIV_PAGE_SIZE);

    // main checksumming loop
    ct = start_page;
    lastt = 0;
    while (!feof(f)) {
        bytes = fread(p, 1, UNIV_PAGE_SIZE, f);
        if (!bytes && feof(f)) return 0;
        if (bytes != UNIV_PAGE_SIZE) {
            fprintf(stderr, "bytes read (%d) doesn't match universal page size (%d)\n", bytes, UNIV_PAGE_SIZE);
            return 1;
        }

        // check the "stored log sequence numbers"
        logseq = mach_read_from_4(p + FIL_PAGE_LSN + 4);
        logseqfield = mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM + 4);
        if (debug)
            printf("page %lu: log sequence number: first = %lu; second = %lu\n", ct, logseq, logseqfield);
        if (logseq != logseqfield) {
            fprintf(stderr, "page %lu invalid (fails log sequence number check)\n", ct);
            return 1;
        }

        // check old method of checksumming
        oldcsum = buf_calc_page_old_checksum(p);
        oldcsumfield = mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
        if (debug)
            printf("page %lu: old style: calculated = %lu; recorded = %lu\n", ct, oldcsum, oldcsumfield);
        if (oldcsumfield != mach_read_from_4(p + FIL_PAGE_LSN) && oldcsumfield != oldcsum) {
            fprintf(stderr, "page %lu invalid (fails old style checksum)\n", ct);
            return 1;
        }

        // now check the new method
        csum = buf_calc_page_new_checksum(p);
        csumfield = mach_read_from_4(p + FIL_PAGE_SPACE_OR_CHKSUM);
        if (debug)
            printf("page %lu: new style: calculated = %lu; recorded = %lu\n", ct, csum, csumfield);
        if (csumfield != 0 && csum != csumfield) {
            fprintf(stderr, "page %lu invalid (fails new style checksum)\n", ct);
            return 1;
        }

        // end if this was the last page we were supposed to check
        if (use_end_page && (ct >= end_page))
            return 0;

        // do counter increase and progress printing
        ct++;
        if (verbose) {
            if (ct % 64 == 0) {
                now = time(0);
                if (!lastt) lastt = now;
                if (now - lastt >= 1) {
                    printf("page %lu okay: %.3f%% done\n", (ct - 1), (float) ct / pages * 100);
                    lastt = now;
                }
            }
        }
    }
    return 0;
}

