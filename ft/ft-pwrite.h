/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: ft-serialize.c 43686 2012-05-18 23:21:00Z leifwalsh $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef FT_PWRITE_H
#define FT_PWRITE_H

void toku_lock_for_pwrite(void);

void toku_unlock_for_pwrite(void);

void toku_full_pwrite_extend(int fd, const void *buf, size_t count, toku_off_t offset);

#endif
