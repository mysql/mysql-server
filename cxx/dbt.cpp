#include "db_cxx.h"

Dbt::Dbt(void) {
    DBT *dbt = this;
    memset(dbt, 0, sizeof(*dbt));
}
