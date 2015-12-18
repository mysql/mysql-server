/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
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

#include "sql_security_ctx.h"
#include "auth_common.h"
#include "sql_class.h"

void Security_context::init()
{
  DBUG_ENTER("Security_context::init");

  m_user.set((const char*) 0, 0, system_charset_info);
  m_host.set("", 0, system_charset_info);
  m_ip.set("", 0, system_charset_info);
  m_host_or_ip.set(STRING_WITH_LEN("connecting host"), system_charset_info);
  m_external_user.set("", 0, system_charset_info);
  m_priv_user[0]= m_priv_host[0]= m_proxy_user[0]= '\0';
  m_priv_user_length= m_priv_host_length= m_proxy_user_length= 0;
  m_master_access= 0;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  m_db_access= NO_ACCESS;
#endif
  m_password_expired= false;

  DBUG_VOID_RETURN;
}


void Security_context::destroy()
{
  DBUG_ENTER("Security_context::destroy");

  if (m_user.length())
    m_user.set((const char *) 0, 0, system_charset_info);

  if (m_host.length())
    m_host.set("", 0, system_charset_info);

  if (m_ip.length())
    m_ip.set("", 0, system_charset_info);

  if (m_host_or_ip.length())
    m_host_or_ip.set("", 0, system_charset_info);

  if (m_external_user.length())
    m_external_user.set("", 0, system_charset_info);

  m_priv_user[0]= m_priv_host[0]= m_proxy_user[0]= 0;
  m_priv_user_length= m_priv_host_length= m_proxy_user_length= 0;

  m_master_access= m_db_access=0;
  m_password_expired= false;

  DBUG_VOID_RETURN;
}


void Security_context::skip_grants()
{
  DBUG_ENTER("Security_context::skip_grants");

  /* privileges for the user are unknown everything is allowed */
  set_host_or_ip_ptr("", 0);
  assign_priv_user(C_STRING_WITH_LEN("skip-grants user"));
  assign_priv_host(C_STRING_WITH_LEN("skip-grants host"));
  m_master_access= ~NO_ACCESS;

  DBUG_VOID_RETURN;
}


/**
  Deep copy status of sctx object to this.

  @param[in]    src_sctx   Object from which status should be copied.
*/

void Security_context::copy_security_ctx (const Security_context &src_sctx)
{
  DBUG_ENTER("Security_context::copy_security_ctx");

  assign_user(src_sctx.m_user.ptr(), src_sctx.m_user.length());
  assign_host(src_sctx.m_host.ptr(), src_sctx.m_host.length());
  assign_ip(src_sctx.m_ip.ptr(), src_sctx.m_ip.length());
  if (!strcmp(src_sctx.m_host_or_ip.ptr(), my_localhost))
    set_host_or_ip_ptr((char *) my_localhost, strlen(my_localhost));
  else
    set_host_or_ip_ptr();
  assign_external_user(src_sctx.m_external_user.ptr(),
                       src_sctx.m_external_user.length());
  assign_priv_user(src_sctx.m_priv_user, src_sctx.m_priv_user_length);
  assign_proxy_user(src_sctx.m_proxy_user, src_sctx.m_proxy_user_length);
  assign_priv_host(src_sctx.m_priv_host, src_sctx.m_priv_host_length);
  m_db_access= src_sctx.m_db_access;
  m_master_access= src_sctx.m_master_access;
  m_password_expired= src_sctx.m_password_expired;

  DBUG_VOID_RETURN;
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
/**
  Initialize this security context from the passed in credentials
  and activate it in the current thread.

  @param       thd
  @param       definer_user
  @param       definer_host
  @param       db
  @param[out]  backup  Save a pointer to the current security context
                       in the thread. In case of success it points to the
                       saved old context, otherwise it points to NULL.


  During execution of a statement, multiple security contexts may
  be needed:
  - the security context of the authenticated user, used as the
    default security context for all top-level statements
  - in case of a view or a stored program, possibly the security
    context of the definer of the routine, if the object is
    defined with SQL SECURITY DEFINER option.

  The currently "active" security context is parameterized in THD
  member security_ctx. By default, after a connection is
  established, this member points at the "main" security context
  - the credentials of the authenticated user.

  Later, if we would like to execute some sub-statement or a part
  of a statement under credentials of a different user, e.g.
  definer of a procedure, we authenticate this user in a local
  instance of Security_context by means of this method (and
  ultimately by means of acl_getroot), and make the
  local instance active in the thread by re-setting
  thd->m_security_ctx pointer.

  Note, that the life cycle and memory management of the "main" and
  temporary security contexts are different.
  For the main security context, the memory for user/host/ip is
  allocated on system heap, and the THD class frees this memory in
  its destructor. The only case when contents of the main security
  context may change during its life time is when someone issued
  CHANGE USER command.
  Memory management of a "temporary" security context is
  responsibility of the module that creates it.

  @retval TRUE  there is no user with the given credentials. The erro
                is reported in the thread.
  @retval FALSE success
*/

bool
Security_context::
change_security_context(THD *thd,
                        const LEX_CSTRING &definer_user,
                        const LEX_CSTRING &definer_host,
                        LEX_STRING *db,
                        Security_context **backup)
{
  bool needs_change;

  DBUG_ENTER("Security_context::change_security_context");

  DBUG_ASSERT(definer_user.str && definer_host.str);

  *backup= NULL;
  needs_change= (strcmp(definer_user.str,
                        thd->security_context()->priv_user().str) ||
                 my_strcasecmp(system_charset_info, definer_host.str,
                               thd->security_context()->priv_host().str));
  if (needs_change)
  {
    if (acl_getroot(this,
                    const_cast<char*>(definer_user.str),
                    const_cast<char*>(definer_host.str),
                    const_cast<char*>(definer_host.str),
                    db->str))
    {
      my_error(ER_NO_SUCH_USER, MYF(0), definer_user.str,
               definer_host.str);
      DBUG_RETURN(TRUE);
    }
    *backup= thd->security_context();
    thd->set_security_context(this);
  }

  DBUG_RETURN(FALSE);
}


void
Security_context::restore_security_context(THD *thd,
                                           Security_context *backup)
{
  if (backup)
    thd->set_security_context(backup);
}
#endif


bool Security_context::user_matches(Security_context *them)
{
  DBUG_ENTER("Security_context::user_matches");

  const char* them_user= them->user().str;

  DBUG_RETURN((m_user.ptr() != NULL) && (them_user != NULL) &&
              !strcmp(m_user.ptr(), them_user));
}
