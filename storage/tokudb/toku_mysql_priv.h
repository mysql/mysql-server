#if !defined(TOKU_MYSQL_PRIV_H)
#define TOKU_MYSQL_PRIV_H
#include "mysql_version.h"
#if MYSQL_VERSION_ID < 50506
#include "mysql_priv.h"
#else
#include "sql_table.h"
#include "handler.h"
#include "table.h"
#include "log.h"
#include "sql_class.h"
#include "sql_show.h"
#include "discover.h"
#endif
#endif
