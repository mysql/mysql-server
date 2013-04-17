/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
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
