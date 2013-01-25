/*
  Provide aliases for several symbols, to support drop-in replacement for
  MariaDB on Fedora and several derives distributions.

  These distributions redefine several symbols (in a way that is no compatible
  with either MySQL or MariaDB) and export it from the client library ( as seen
  e.g from this patch)
http://lists.fedoraproject.org/pipermail/scm-commits/2010-December/537257.html

  MariaDB handles compatibility distribution by providing the same symbols from 
  the client library if it is built with -DRPM

*/
#include <errmsg.h>
#include <my_sys.h>
#include <mysql.h>
extern "C" {

CHARSET_INFO *mysql_default_charset_info = default_charset_info;

CHARSET_INFO *mysql_get_charset(uint cs_number, myf flags)
{
  return get_charset(cs_number, flags);
}

CHARSET_INFO *mysql_get_charset_by_csname(const char *cs_name,
                                           uint cs_flags, myf my_flags)
{
   return get_charset_by_csname(cs_name, cs_flags, my_flags);
}


my_bool mysql_net_realloc(NET *net, size_t length)
{
   return net_realloc(net,length);
}

const char **mysql_client_errors = client_errors;

} /*extern "C" */

