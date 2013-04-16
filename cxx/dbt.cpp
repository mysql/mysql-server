/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <db_cxx.h>

Dbt::Dbt(void) {
    DBT *dbt = this;
    memset(dbt, 0, sizeof(*dbt));
}

Dbt::Dbt(void *data, u_int32_t size) {
    DBT *dbt = this;
    memset(dbt, 0, sizeof(*dbt));
    set_data(data);
    set_size(size);
}

Dbt::~Dbt(void)
{
}
