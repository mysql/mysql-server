#include "db_cxx.h"

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
