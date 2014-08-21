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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "test.h"

#include <util/dmt.h>

static void
parse_args (int argc, const char *argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
        int resultcode=0;
        if (strcmp(argv[1], "-v")==0) {
            verbose++;
        } else if (strcmp(argv[1], "-q")==0) {
            verbose = 0;
        } else if (strcmp(argv[1], "-h")==0) {
        do_usage:
            fprintf(stderr, "Usage:\n%s [-v|-h]\n", argv0);
            exit(resultcode);
        } else {
            resultcode=1;
            goto do_usage;
        }
        argc--;
        argv++;
    }
}
/* End ".h like" stuff. */

struct value {
    uint32_t number;
};
#define V(x) ((struct value *)(x))



const uint32_t MAXNUM = 1024;
const uint32_t MAXLEN = 32;
char data[MAXNUM][MAXLEN];

struct val_type {
    char c[MAXLEN];
};

namespace toku {
class vwriter {
    public:
        size_t get_size(void) const {
            size_t len = strlen(v.c);
            invariant(len < sizeof(val_type));
            return len + 1;
        }
        void write_to(val_type *const dest) const {
            strcpy(dest->c, v.c);
        }

        vwriter(const char* c) {
            invariant(strlen(c) < sizeof(val_type));
            strcpy(v.c, c);
        }

        vwriter(const uint32_t klpair_len, val_type *const src) {
            invariant(strlen(src->c) < sizeof(val_type));
            strcpy(v.c, src->c);
            invariant(klpair_len == get_size());
        }
    private:
        val_type v;
};
}

/* Globals */
typedef toku::dmt<val_type, val_type*, toku::vwriter> vdmt;

const unsigned int random_seed = 0xFEADACBA;

///////////////


static void fail_one_verify(uint32_t len, uint32_t num, vdmt *v) {
    val_type* fetched_data;
    int count = 0;
    v->verify();
    for (uint32_t i = 0; i < num; i++) {
        uint32_t fetched_len;
        int r = v->fetch(i-count, &fetched_len, &fetched_data);
        if (r != 0 || fetched_len != len || strcmp(fetched_data->c, data[i])) {
            count++;
            continue;
        }
    }
    invariant(count == 1);
}

static void verify(uint32_t len, uint32_t num, vdmt *v) {
    v->verify();
    val_type* fetched_data;
    for (uint32_t i = 0; i < num; i++) {
        uint32_t fetched_len;
        int r = v->fetch(i, &fetched_len, &fetched_data);
        CKERR(r);
        invariant(fetched_len == len);
        invariant(!strcmp(fetched_data->c, data[i]));
    }
}


static void test_builder_fixed(uint32_t len, uint32_t num) {
    srandom(random_seed);
    assert(len > 1);
    assert(len <= MAXLEN);
    assert(num <= MAXNUM);
    for (uint32_t i = 0; i < num; i++) {
        for (uint32_t j = 0; j < len-1; j++) {
            data[i][j] = random() % 255 + 1; //This way it doesn't end up being 0 and thought of as NUL
        }
        data[i][len-1] = '\0'; //cap it
    }

    vdmt::builder builder;
    builder.create(num, num * len);

    for (uint32_t i = 0; i < num; i++) {
        vwriter vfun(data[i]);
        builder.append(vfun);
    }
    invariant(builder.value_length_is_fixed());
    vdmt v;
    builder.build(&v);
    invariant(v.value_length_is_fixed());
    invariant(v.get_fixed_length() == len || num == 0);

    invariant(v.size() == num);

    verify(len, num, &v);

    for (uint32_t change = 0; change < num; change++) {
        vdmt v2;
        v2.clone(v);
        v2.delete_at(change);
        fail_one_verify(len, num, &v2);

        vwriter vfun(data[change]);
        v2.insert_at(vfun, change);
        verify(len, num, &v2);
        v2.destroy();
    }

    v.destroy();
}

static void test_builder_variable(uint32_t len, uint32_t len2, uint32_t num) {
    srandom(random_seed);
    assert(len > 1);
    assert(len <= MAXLEN);
    assert(num <= MAXNUM);
    assert(num > 3);
    uint32_t which2 = random() % num;
    for (uint32_t i = 0; i < num; i++) {
        uint32_t thislen = i == which2 ? len2 : len;
        for (uint32_t j = 0; j < thislen-1; j++) {
            data[i][j] = random() % 255 + 1; //This way it doesn't end up being 0 and thought of as NUL
        }
        data[i][thislen-1] = '\0'; //cap it
    }

    vdmt::builder builder;
    builder.create(num, (num-1) * len + len2);

    for (uint32_t i = 0; i < num; i++) {
        vwriter vfun(data[i]);
        builder.append(vfun);
    }
    invariant(!builder.value_length_is_fixed());
    vdmt v;
    builder.build(&v);
    invariant(!v.value_length_is_fixed());

    invariant(v.size() == num);

    val_type* fetched_data;
    for (uint32_t i = 0; i < num; i++) {
        uint32_t fetched_len;
        int r = v.fetch(i, &fetched_len, &fetched_data);
        CKERR(r);
        if (i == which2) {
            invariant(fetched_len == len2);
            invariant(!strcmp(fetched_data->c, data[i]));
        } else {
            invariant(fetched_len == len);
            invariant(!strcmp(fetched_data->c, data[i]));
        }
    }

    v.destroy();
}

static void test_create_from_sorted_memory_of_fixed_sized_elements_and_serialize(uint32_t len, uint32_t num) {
    srandom(random_seed);
    assert(len <= MAXLEN);
    assert(num <= MAXNUM);
    for (uint32_t i = 0; i < num; i++) {
        for (uint32_t j = 0; j < len-1; j++) {
            data[i][j] = random() % 255 + 1; //This way it doesn't end up being 0 and thought of as NUL
        }
        data[i][len-1] = '\0'; //cap it
    }

    char *flat = (char*)toku_xmalloc(len * num);
    char *p = flat;
    for (uint32_t i = 0; i < num; i++) {
        memcpy(p, data[i], len);
        p += len;
    }
    vdmt v;

    v.create_from_sorted_memory_of_fixed_size_elements(flat, num, len*num, len);
    invariant(v.value_length_is_fixed());
    invariant(v.get_fixed_length() == len);

    invariant(v.size() == num);

    val_type* fetched_data;
    for (uint32_t i = 0; i < num; i++) {
        uint32_t fetched_len;
        int r = v.fetch(i, &fetched_len, &fetched_data);
        CKERR(r);
        invariant(fetched_len == len);
        invariant(!strcmp(fetched_data->c, data[i]));
    }

    char *serialized_flat = (char*)toku_xmalloc(len*num);
    struct wbuf wb;
    wbuf_nocrc_init(&wb, serialized_flat, len*num);
    v.prepare_for_serialize();
    v.serialize_values(len*num, &wb);
    invariant(!memcmp(serialized_flat, flat, len*num));

    
    if (num > 1) {
        //Currently converting to dtree treats the entire thing as NOT fixed length.
        //Optional additional perf here.
        uint32_t which = (random() % (num-1)) + 1;  // Not last, not first
        invariant(which > 0 && which < num-1);
        v.delete_at(which);

        memmove(flat + which*len, flat+(which+1)*len, (num-which-1) * len);
        v.prepare_for_serialize();
        wbuf_nocrc_init(&wb, serialized_flat, len*(num-1));
        v.serialize_values(len*(num-1), &wb);
        invariant(!memcmp(serialized_flat, flat, len*(num-1)));
    }

    toku_free(flat);
    toku_free(serialized_flat);

    v.destroy();
}

int
test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    // Do test with size divisible by 4 and not
    test_builder_fixed(4, 0);
    test_builder_fixed(5, 0);
    test_builder_fixed(4, 1);
    test_builder_fixed(5, 1);
    test_builder_fixed(4, 100);
    test_builder_fixed(5, 100);
    // Do test with zero, one, or both sizes divisible
    test_builder_variable(4, 8, 100);
    test_builder_variable(4, 5, 100);
    test_builder_variable(5, 8, 100);
    test_builder_variable(5, 10, 100);

    test_create_from_sorted_memory_of_fixed_sized_elements_and_serialize(4, 0);
    test_create_from_sorted_memory_of_fixed_sized_elements_and_serialize(5, 0);
    test_create_from_sorted_memory_of_fixed_sized_elements_and_serialize(4, 1);
    test_create_from_sorted_memory_of_fixed_sized_elements_and_serialize(5, 1);
    test_create_from_sorted_memory_of_fixed_sized_elements_and_serialize(4, 100);
    test_create_from_sorted_memory_of_fixed_sized_elements_and_serialize(5, 100);

    return 0;
}

