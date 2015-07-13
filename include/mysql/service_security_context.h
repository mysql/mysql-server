/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_SERVICE_SECURITY_CONTEXT
#define MYSQL_SERVICE_SECURITY_CONTEXT

/**
  @file include/mysql/service_security_context.h

  This service provides functions for plugins and storage engines to
  manipulate the thread's security context.
*/

#ifdef __cplusplus
class Security_context;
#define MYSQL_SECURITY_CONTEXT Security_context*
#else
#define MYSQL_SECURITY_CONTEXT void*
#endif
typedef char my_svc_bool;

#ifdef __cplusplus
extern "C" {
#endif

extern struct security_context_service_st {
  my_svc_bool (*thd_get_security_context)(MYSQL_THD, MYSQL_SECURITY_CONTEXT *out_ctx);
  my_svc_bool (*thd_set_security_context)(MYSQL_THD, MYSQL_SECURITY_CONTEXT in_ctx);

  my_svc_bool (*security_context_create)(MYSQL_SECURITY_CONTEXT *out_ctx);
  my_svc_bool (*security_context_destroy)(MYSQL_SECURITY_CONTEXT);
  my_svc_bool (*security_context_copy)(MYSQL_SECURITY_CONTEXT in_ctx, MYSQL_SECURITY_CONTEXT *out_ctx);

  my_svc_bool (*security_context_lookup)(MYSQL_SECURITY_CONTEXT ctx,
                                         const char *user, const char *host,
                                         const char *ip, const char *db);

  my_svc_bool (*security_context_get_option)(MYSQL_SECURITY_CONTEXT, const char *name, void *inout_pvalue);
  my_svc_bool (*security_context_set_option)(MYSQL_SECURITY_CONTEXT, const char *name, void *pvalue);
} *security_context_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define thd_get_security_context(_THD, _CTX) \
  security_context_service->thd_get_security_context(_THD, _CTX)
#define thd_set_security_context(_THD, _CTX) \
  security_context_service->thd_set_security_context(_THD, _CTX)

#define security_context_create(_CTX) \
  security_context_service->security_context_create(_CTX)
#define security_context_destroy(_CTX) \
  security_context_service->security_context_destroy(_CTX)
#define security_context_copy(_CTX1, _CTX2) \
  security_context_service->security_context_copy(_CTX1,_CTX2)

#define security_context_lookup(_CTX, _U, _H, _IP, _DB) \
  security_context_service->security_context_lookup(_CTX, _U, _H, _IP, _DB)

#define security_context_get_option(_SEC_CTX, _NAME, _VALUE) \
  security_context_service->security_context_get_option(_SEC_CTX, _NAME, _VALUE)
#define security_context_set_option(_SEC_CTX, _NAME, _VALUE) \
  security_context_service->security_context_set_option(_SEC_CTX, _NAME, _VALUE)
#else
  my_svc_bool thd_get_security_context(MYSQL_THD, MYSQL_SECURITY_CONTEXT *out_ctx);
  my_svc_bool thd_set_security_context(MYSQL_THD, MYSQL_SECURITY_CONTEXT in_ctx);

  my_svc_bool security_context_create(MYSQL_SECURITY_CONTEXT *out_ctx);
  my_svc_bool security_context_destroy(MYSQL_SECURITY_CONTEXT ctx);
  my_svc_bool security_context_copy(MYSQL_SECURITY_CONTEXT in_ctx, MYSQL_SECURITY_CONTEXT *out_ctx);

  my_svc_bool security_context_lookup(MYSQL_SECURITY_CONTEXT ctx,
                                  const char *user, const char *host,
                                  const char *ip, const char *db);

  my_svc_bool security_context_get_option(MYSQL_SECURITY_CONTEXT, const char *name, void *inout_pvalue);
  my_svc_bool security_context_set_option(MYSQL_SECURITY_CONTEXT, const char *name, void *pvalue);
#endif /* !MYSQL_DYNAMIC_PLUGIN */

#ifdef __cplusplus
}
#endif /* _cplusplus */

#endif /* !MYSQL_SERVICE_SECURITY_CONTEXT */
