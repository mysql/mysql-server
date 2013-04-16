/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#pragma once
#include <util/partitioned_counter.h>
#include <util/constexpr.h>

#define TOKUDB_STATUS_INIT(array,k,c,t,l,inc) do { \
    array.status[k].keyname = #k;                    \
    array.status[k].columnname = #c;                 \
    array.status[k].type    = t;                     \
    array.status[k].legend  = l;                     \
    static_assert((inc) != 0, "Var must be included in at least one place"); \
    constexpr_static_assert(strcmp(#c, "NULL") && strcmp(#c, "0"),           \
            "Use nullptr for no column name instead of NULL, 0, etc...");    \
    constexpr_static_assert((inc) == TOKU_ENGINE_STATUS                      \
            || strcmp(#c, "nullptr"), "Missing column name.");               \
    constexpr_static_assert(static_strncasecmp(#c, "TOKU", strlen("TOKU")),  \
                  "Do not start column names with toku/tokudb.  Names get TOKUDB_ prefix automatically."); \
    array.status[k].include = static_cast<toku_engine_status_include_type>(inc);  \
    if (t == PARCOUNT) {                                               \
        array.status[k].value.parcount = create_partitioned_counter(); \
    }                                                                  \
} while (0)

