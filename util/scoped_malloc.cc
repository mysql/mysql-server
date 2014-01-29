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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <pthread.h>

#include <toku_include/memory.h>

#include <util/scoped_malloc.h>

// The __thread storage class modifier isn't well supported on osx, but we
// aren't worried about the performance on osx, so we provide a
// pass-through implementation of scoped mallocs.
#ifdef __APPLE__

namespace toku {

    scoped_malloc::scoped_malloc(const size_t size)
        : m_size(size),
          m_local(false),
          m_buf(toku_xmalloc(size)) {}

    scoped_malloc::~scoped_malloc() {
        toku_free(m_buf);
    }

} // namespace toku

void toku_scoped_malloc_init(void) {}
void toku_scoped_malloc_destroy(void) {}

#else // __APPLE__

namespace toku {

    // see pthread_key handling at the bottom
    //
    // when we use gcc 4.8, we can use the 'thread_local' keyword and proper
    // c++ constructors/destructors instead of this pthread wizardy.
    static pthread_key_t tl_stack_destroy_pthread_key;

    class tl_stack {
        // 1MB
        static const size_t STACK_SIZE = 1 * 1024 * 1024;
        
    public:
        void init() {
            m_stack = reinterpret_cast<char *>(toku_xmalloc(STACK_SIZE));
            m_current_offset = 0;
            int r = pthread_setspecific(tl_stack_destroy_pthread_key, this);
            invariant_zero(r);
        }

        void destroy() {
            if (m_stack != NULL) {
                toku_free(m_stack);
                m_stack = NULL;
            }
        }

        // Allocate 'size' bytes and return a pointer to the first byte
        void *alloc(const size_t size) {
            if (m_stack == NULL) {
                init();
            }
            invariant(m_current_offset + size <= STACK_SIZE);
            void *mem = &m_stack[m_current_offset];
            m_current_offset += size;
            return mem;
        }

        // Give back a previously allocated region of 'size' bytes.
        void dealloc(const size_t size) {
            invariant(m_current_offset >= size);
            m_current_offset -= size;
        }

        // Get the current size of free-space in bytes.
        size_t get_free_space() const {
            invariant(m_current_offset <= STACK_SIZE);
            return STACK_SIZE - m_current_offset;
        }

    private:
        // Offset of the free region in the stack
        size_t m_current_offset;
        char *m_stack;
    };

    // Each thread has its own local stack.
    static __thread tl_stack local_stack;

    // Memory is allocated from thread-local storage if available, otherwise from malloc(1).
    scoped_malloc::scoped_malloc(const size_t size) :
        m_size(size),
        m_local(local_stack.get_free_space() >= m_size),
        m_buf(m_local ? local_stack.alloc(m_size) : toku_xmalloc(m_size)) {
    }

    scoped_malloc::~scoped_malloc() {
        if (m_local) {
            local_stack.dealloc(m_size);
        } else {
            toku_free(m_buf);
        }
    }

} // namespace toku

// pthread key handling:
// - there is a process-wide pthread key that is associated with the destructor for a tl_stack
// - on process construction, we initialize the key; on destruction, we clean it up.
// - when a thread first uses its tl_stack, it calls pthread_setspecific(&destroy_key, "some key"),
//   associating the destroy key with the tl_stack_destroy destructor
// - when a thread terminates, it calls the associated destructor; tl_stack_destroy.

static void tl_stack_destroy(void *key) {
    invariant_notnull(key);
    toku::tl_stack *st = reinterpret_cast<toku::tl_stack *>(key);
    st->destroy();
}

void toku_scoped_malloc_init(void) {
    int r = pthread_key_create(&toku::tl_stack_destroy_pthread_key, tl_stack_destroy);
    invariant_zero(r);
}

void toku_scoped_malloc_destroy(void) {
    // We're deregistering the destructor key here. When this thread exits,
    // the tl_stack destructor won't get called, so we need to do that first.
    tl_stack_destroy(&toku::local_stack);
    int r = pthread_key_delete(toku::tl_stack_destroy_pthread_key);
    invariant_zero(r);
}

#endif // !__APPLE__
