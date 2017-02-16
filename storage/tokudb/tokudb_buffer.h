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
#if !defined(_TOKUDB_BUFFER_H)
#define _TOKUDB_BUFFER_H

#include "tokudb_vlq.h"

namespace tokudb {

// A Buffer manages a contiguous chunk of memory and supports appending new data to the end of the buffer, and
// consuming chunks from the beginning of the buffer.  The buffer will reallocate memory when appending
// new data to a full buffer. 

class buffer {
public:
    buffer(void *the_data, size_t s, size_t l) : m_data(the_data), m_size(s), m_limit(l), m_is_static(true) {
    }
    buffer() : m_data(NULL), m_size(0), m_limit(0), m_is_static(false) {
    }
    virtual ~buffer() {
        if (!m_is_static)
            free(m_data);
    }

    // Return a pointer to the end of the buffer suitable for appending a fixed number of bytes.
    void *append_ptr(size_t s) {
        maybe_realloc(s);
        void *p = (char *) m_data + m_size;
        m_size += s;
        return p;
    }

    // Append bytes to the buffer
    void append(void *p, size_t s) {
        memcpy(append_ptr(s), p, s);
    }

    // Append an unsigned int to the buffer.
    // Returns the number of bytes used to encode the number.
    // Returns 0 if the number could not be encoded.
    template<class T> size_t append_ui(T n) {
        maybe_realloc(10); // 10 bytes is big enough for up to 64 bit number
        size_t s = tokudb::vlq_encode_ui<T>(n, (char *) m_data + m_size, 10);
        m_size += s;
        return s;
    }

    // Return a pointer to the next location in the buffer where bytes are consumed from.
    void *consume_ptr(size_t s) {
        if (m_size + s > m_limit)
            return NULL;
        void *p = (char *) m_data + m_size;
        m_size += s;
        return p;
    }

    // Consume bytes from the buffer.
    void consume(void *p, size_t s) {
        memcpy(p, consume_ptr(s), s);
    }

    // Consume an unsigned int from the buffer.
    // Returns 0 if the unsigned int could not be decoded, probably because the buffer is too short.
    // Otherwise return the number of bytes consumed, and stuffs the decoded number in *p.
    template<class T> size_t consume_ui(T *p) {
        size_t s = tokudb::vlq_decode_ui<T>(p, (char *) m_data + m_size, m_limit - m_size);
        m_size += s;
        return s;
    }

    // Write p_length bytes at an offset in the buffer
    void write(void *p, size_t p_length, size_t offset) {
        assert(offset + p_length <= m_size);
        memcpy((char *)m_data + offset, p, p_length);
    }

    // Read p_length bytes at an offset in the buffer
    void read(void *p, size_t p_length, size_t offset) {
        assert(offset + p_length <= m_size);
        memcpy(p, (char *)m_data + offset, p_length);
    }

    // Replace a field in the buffer with new data.  If the new data size is different, then readjust the 
    // size of the buffer and move things around.
    void replace(size_t offset, size_t old_s, void *new_p, size_t new_s) {
        assert(offset + old_s <= m_size);
        if (new_s > old_s)
            maybe_realloc(new_s - old_s);
        char *data_offset = (char *) m_data + offset;
        if (new_s != old_s) {
            size_t n = m_size - (offset + old_s);
            assert(offset + new_s + n <= m_limit && offset + old_s + n <= m_limit);
            memmove(data_offset + new_s, data_offset + old_s, n);
            if (new_s > old_s)
                m_size += new_s - old_s;
            else
                m_size -= old_s - new_s;
            assert(m_size <= m_limit);
        }
        memcpy(data_offset, new_p, new_s);
    }

    // Return a pointer to the data in the buffer
    void *data() const {
        return m_data;
    }

    // Return the size of the data in the buffer
    size_t size() const {
        return m_size;
    }

    // Return the size of the underlying memory in the buffer
    size_t limit() const {
        return m_limit;
    }

private:
    // Maybe reallocate the buffer when it becomes full by doubling its size.
    void maybe_realloc(size_t s) {
        if (m_size + s > m_limit) {
            size_t new_limit = m_limit * 2;
            if (new_limit < m_size + s)
                new_limit = m_size + s;
            assert(!m_is_static);
            void *new_data = realloc(m_data, new_limit);
            assert(new_data != NULL);
            m_data = new_data;
            m_limit = new_limit;
        }
    }   
private:
    void *m_data;
    size_t m_size;
    size_t m_limit;
    bool m_is_static;
};

};

#endif
