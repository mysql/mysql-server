#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

u_int32_t toku_le_crc(LEAFENTRY v) {
    return x1764_memory(v, leafentry_memsize(v));
}


void wbuf_LEAFENTRY(struct wbuf *w, LEAFENTRY le) {
    wbuf_literal_bytes(w, le, leafentry_disksize(le));
}

