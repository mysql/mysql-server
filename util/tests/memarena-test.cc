/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

#include <string.h>

#include "portability/toku_assert.h"

#include "util/memarena.h"

class memarena_unit_test {
private:
    static const int magic = 37;

    template <typename F>
    void iterate_chunks(memarena *ma, F &fn) {
        for (memarena::chunk_iterator it(ma); it.more(); it.next()) {
            size_t used = 0;
            const void *buf = it.current(&used);
            fn(buf, used);
        }
    }

    void test_create(size_t size) {
        memarena ma;
        ma.create(size);
        invariant(ma._current_chunk.size == size);
        invariant(ma._current_chunk.used == 0);
        if (size == 0) {
            invariant_null(ma._current_chunk.buf);
        } else {
            invariant_notnull(ma._current_chunk.buf);
        }

        // make sure memory was allocated ok by
        // writing to buf and reading it back
        if (size > 0) {
            memset(ma._current_chunk.buf, magic, size);
        }
        for (size_t i = 0; i < size; i++) {
            const char *buf = reinterpret_cast<char *>(ma._current_chunk.buf);
            invariant(buf[i] == magic);
        }
        ma.destroy();
    }

    void test_malloc(size_t size) {
        memarena ma;
        ma.create(14);
        void *v = ma.malloc_from_arena(size);
        invariant_notnull(v);

        // make sure memory was allocated ok by
        // writing to buf and reading it back
        if (size > 0) {
            memset(ma._current_chunk.buf, magic, size);
        }
        for (size_t i = 0; i < size; i++) {
            const char *c = reinterpret_cast<char *>(ma._current_chunk.buf);
            invariant(c[i] == magic);
        }
        ma.destroy();
    }

    static void test_iterate_fn(const void *buf, size_t used) {
        for (size_t i = 0; i < used; i++) {
            const char *c = reinterpret_cast<const char *>(buf);
            invariant(c[i] == (char) ((intptr_t) &c[i]));
        }
    }

    void test_iterate(size_t size) {
        memarena ma;
        ma.create(14);
        for (size_t k = 0; k < size / 64; k += 64) {
            void *v = ma.malloc_from_arena(64);
            for (size_t i = 0; i < 64; i++) {
                char *c = reinterpret_cast<char *>(v);
                c[i] = (char) ((intptr_t) &c[i]);
            }
        }
        size_t rest = size % 64;
        if (rest != 0) {
            void *v = ma.malloc_from_arena(64);
            for (size_t i = 0; i < 64; i++) {
                char *c = reinterpret_cast<char *>(v);
                c[i] = (char) ((intptr_t) &c[i]);
            }
        }

        iterate_chunks(&ma, test_iterate_fn);
        ma.destroy();
    }

    void test_move_memory(size_t size) {
        memarena ma;
        ma.create(14);
        for (size_t k = 0; k < size / 64; k += 64) {
            void *v = ma.malloc_from_arena(64);
            for (size_t i = 0; i < 64; i++) {
                char *c = reinterpret_cast<char *>(v);
                c[i] = (char) ((intptr_t) &c[i]);
            }
        }
        size_t rest = size % 64;
        if (rest != 0) {
            void *v = ma.malloc_from_arena(64);
            for (size_t i = 0; i < 64; i++) {
                char *c = reinterpret_cast<char *>(v);
                c[i] = (char) ((intptr_t) &c[i]);
            }
        }

        memarena ma2;
        ma.move_memory(&ma2);
        iterate_chunks(&ma2, test_iterate_fn);

        ma.destroy();
        ma2.destroy();
    }

public:
    void test() {
        test_create(0);
        test_create(64);
        test_create(128 * 1024 * 1024);
        test_malloc(0);
        test_malloc(63);
        test_malloc(64);
        test_malloc(64 * 1024 * 1024);
        test_malloc((64 * 1024 * 1024) + 1);
        test_iterate(0);
        test_iterate(63);
        test_iterate(128 * 1024);
        test_iterate(64 * 1024 * 1024);
        test_iterate((64 * 1024 * 1024) + 1);
        test_move_memory(0);
        test_move_memory(1);
        test_move_memory(63);
        test_move_memory(65);
        test_move_memory(65 * 1024 * 1024);
        test_move_memory(101 * 1024 * 1024);
    }
};

int main(void) {
    memarena_unit_test test;
    test.test();
    return 0;
}
