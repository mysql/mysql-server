/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#pragma once

#include <db.h>
#include <string.h>

#include <ft/ybt.h>
#include <ft/fttypes.h>

namespace toku {

// a comparator object encapsulates the data necessary for 
// comparing two keys in a fractal tree. it further understands
// that points may be positive or negative infinity.

class comparator {
public:
    void set_descriptor(DESCRIPTOR desc) {
        m_fake_db.cmp_descriptor = desc;
    }

    void create(ft_compare_func cmp, DESCRIPTOR desc) {
        m_cmp = cmp;
        memset(&m_fake_db, 0, sizeof(m_fake_db));
        m_fake_db.cmp_descriptor = desc;
    }

    int compare(const DBT *a, const DBT *b) {
        if (toku_dbt_is_infinite(a) || toku_dbt_is_infinite(b)) {
            return toku_dbt_infinite_compare(a, b);
        } else {
            return m_cmp(&m_fake_db, a, b);
        }
    }

private:
    struct __toku_db m_fake_db;
    ft_compare_func m_cmp;
};

} /* namespace toku */
