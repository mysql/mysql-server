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

#include "portability/toku_portability.h"

#include "ft/msg.h"
#include "ft/txn/xids.h"
#include "util/dbt.h"

ft_msg::ft_msg(const DBT *key, const DBT *val, enum ft_msg_type t, MSN m, XIDS x) :
    _key(key ? *key : toku_empty_dbt()),
    _val(val ? *val : toku_empty_dbt()),
    _type(t), _msn(m), _xids(x) {
}

ft_msg ft_msg::deserialize_from_rbuf(struct rbuf *rb, XIDS *x, bool *is_fresh) {
    const void *keyp, *valp;
    uint32_t keylen, vallen;
    enum ft_msg_type t = (enum ft_msg_type) rbuf_char(rb);
    *is_fresh = rbuf_char(rb);
    MSN m = rbuf_MSN(rb);
    toku_xids_create_from_buffer(rb, x);
    rbuf_bytes(rb, &keyp, &keylen);
    rbuf_bytes(rb, &valp, &vallen);

    DBT k, v;
    return ft_msg(toku_fill_dbt(&k, keyp, keylen), toku_fill_dbt(&v, valp, vallen), t, m, *x);
}

ft_msg ft_msg::deserialize_from_rbuf_v13(struct rbuf *rb, MSN m, XIDS *x) {
    const void *keyp, *valp;
    uint32_t keylen, vallen;
    enum ft_msg_type t = (enum ft_msg_type) rbuf_char(rb);
    toku_xids_create_from_buffer(rb, x);
    rbuf_bytes(rb, &keyp, &keylen);
    rbuf_bytes(rb, &valp, &vallen);

    DBT k, v;
    return ft_msg(toku_fill_dbt(&k, keyp, keylen), toku_fill_dbt(&v, valp, vallen), t, m, *x);
}

const DBT *ft_msg::kdbt() const {
    return &_key;
}

const DBT *ft_msg::vdbt() const {
    return &_val;
}

enum ft_msg_type ft_msg::type() const {
    return _type;
}

MSN ft_msg::msn() const {
    return _msn;
}

XIDS ft_msg::xids() const {
    return _xids;
}

size_t ft_msg::total_size() const {
    // Must store two 4-byte lengths
    static const size_t key_val_overhead = 8;

    // 1 byte type, 1 byte freshness, then 8 byte MSN
    static const size_t msg_overhead = 2 + sizeof(MSN);

    static const size_t total_overhead = key_val_overhead + msg_overhead;

    const size_t keyval_size = _key.size + _val.size;
    const size_t xids_size = toku_xids_get_serialize_size(xids());
    return total_overhead + keyval_size + xids_size;
}

void ft_msg::serialize_to_wbuf(struct wbuf *wb, bool is_fresh) const {
    wbuf_nocrc_char(wb, (unsigned char) _type);
    wbuf_nocrc_char(wb, (unsigned char) is_fresh);
    wbuf_MSN(wb, _msn);
    wbuf_nocrc_xids(wb, _xids);
    wbuf_nocrc_bytes(wb, _key.data, _key.size);
    wbuf_nocrc_bytes(wb, _val.data, _val.size);
}

