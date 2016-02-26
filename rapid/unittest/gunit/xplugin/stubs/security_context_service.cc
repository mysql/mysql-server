/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_global.h"
#include "mysql/plugin.h"
#include "mysql/service_security_context.h"


#ifdef __cplusplus
extern "C" {
#endif

  my_svc_bool thd_get_security_context(MYSQL_THD, MYSQL_SECURITY_CONTEXT *out_ctx)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  my_svc_bool thd_set_security_context(MYSQL_THD, MYSQL_SECURITY_CONTEXT in_ctx)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  my_svc_bool security_context_create(MYSQL_SECURITY_CONTEXT *out_ctx)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  my_svc_bool security_context_destroy(MYSQL_SECURITY_CONTEXT ctx)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  my_svc_bool security_context_copy(MYSQL_SECURITY_CONTEXT in_ctx, MYSQL_SECURITY_CONTEXT *out_ctx)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  my_svc_bool security_context_lookup(MYSQL_SECURITY_CONTEXT ctx,
                                      const char *user, const char *host,
                                      const char *ip, const char *db)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  my_svc_bool security_context_get_option(MYSQL_SECURITY_CONTEXT, const char *name, void *inout_pvalue)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  my_svc_bool security_context_set_option(MYSQL_SECURITY_CONTEXT, const char *name, void *pvalue)
  {
    DBUG_ASSERT(0);
    return 0;
  }


#ifdef __cplusplus
}
#endif
