#ifndef PROBES_MYSQL_H

#define PROBES_MYSQL_H

#include <my_global.h>

#if defined(HAVE_DTRACE) && !defined(DISABLE_DTRACE)
#include "probes_mysql_dtrace.h"
#else
#include "probes_mysql_nodtrace.h"
#endif

#endif /* PROBES_MYSQL_H */
