#ifndef _TOKU_SUB_BLOCK_MAP_H
#define TOKU_SUB_BLOCK_MAP_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// Map objects to a sequence of sub block
struct sub_block_map {
    u_int32_t idx;
    u_int32_t offset;
    u_int32_t size;
};

enum {
    stored_sub_block_map_size = sizeof (struct sub_block_map), // size of a sub-block map on disk
};

static inline void
sub_block_map_init(struct sub_block_map *sbmap, u_int32_t idx, u_int32_t offset, u_int32_t size) {
    sbmap->idx = idx;
    sbmap->offset = offset;
    sbmap->size = size;
}

static inline void
sub_block_map_serialize(struct sub_block_map *sbmap, struct wbuf *wbuf) {
    wbuf_nocrc_int(wbuf, sbmap->idx);
    wbuf_nocrc_int(wbuf, sbmap->offset);
    wbuf_nocrc_int(wbuf, sbmap->size);
}

static inline void
sub_block_map_deserialize(struct sub_block_map *sbmap, struct rbuf *rbuf) {
    sbmap->idx = rbuf_int(rbuf);
    sbmap->offset = rbuf_int(rbuf);
    sbmap->size = rbuf_int(rbuf);
}

#endif
