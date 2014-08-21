/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "portability/toku_stdint.h"

#include "util/dbt.h"
#include "util/memarena.h"

namespace toku {

    // a key range buffer represents a set of key ranges that can
    // be stored, iterated over, and then destroyed all at once.
    class range_buffer {
    private:

        // the key range buffer is a bunch of records in a row.
        // each record has the following header, followed by the
        // left key and right key data payload, if applicable.
        // we limit keys to be 2^16, since we store lengths as 2 bytes.
        static const size_t MAX_KEY_SIZE = 1 << 16;

        struct record_header {
            bool left_neg_inf;
            bool left_pos_inf;
            bool right_pos_inf;
            bool right_neg_inf;
            uint16_t left_key_size;
            uint16_t right_key_size;

            bool left_is_infinite(void) const;

            bool right_is_infinite(void) const;

            void init(const DBT *left_key, const DBT *right_key);
        };
        static_assert(sizeof(record_header) == 8, "record header format is off");
        
    public:

        // the iterator abstracts reading over a buffer of variable length
        // records one by one until there are no more left.
        class iterator {
        public:
            iterator();
            iterator(const range_buffer *buffer);

            // a record represents the user-view of a serialized key range.
            // it handles positive and negative infinity and the optimized
            // point range case, where left and right points share memory.
            class record {
            public:
                // get a read-only pointer to the left key of this record's range
                const DBT *get_left_key(void) const;

                // get a read-only pointer to the right key of this record's range
                const DBT *get_right_key(void) const;

                // how big is this record? this tells us where the next record is
                size_t size(void) const;

                // populate a record header and point our DBT's
                // buffers into ours if they are not infinite.
                void deserialize(const char *buf);

            private:
                record_header _header;
                DBT _left_key;
                DBT _right_key;
            };

            // populate the given record object with the current
            // the memory referred to by record is valid for only
            // as long as the record exists.
            bool current(record *rec);

            // move the iterator to the next record in the buffer
            void next(void);

        private:
            void reset_current_chunk();

            // the key range buffer we are iterating over, the current
            // offset in that buffer, and the size of the current record.
            memarena::chunk_iterator _ma_chunk_iterator;
            const void *_current_chunk_base;
            size_t _current_chunk_offset;
            size_t _current_chunk_max;
            size_t _current_rec_size;
        };

        // allocate buffer space lazily instead of on creation. this way,
        // no malloc/free is done if the transaction ends up taking no locks.
        void create(void);

        // append a left/right key range to the buffer.
        // if the keys are equal, then only one copy is stored.
        void append(const DBT *left_key, const DBT *right_key);

        // is this range buffer empty?
        bool is_empty(void) const;

        // how much memory is being used by this range buffer?
        uint64_t total_memory_size(void) const;

        // how many ranges are stored in this range buffer?
        int get_num_ranges(void) const;

        void destroy(void);

    private:
        memarena _arena;
        int _num_ranges;

        void append_range(const DBT *left_key, const DBT *right_key);

        // append a point to the buffer. this is the space/time saving
        // optimization for key ranges where left == right.
        void append_point(const DBT *key);
    };

} /* namespace toku */
