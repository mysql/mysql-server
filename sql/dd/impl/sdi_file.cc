/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/sdi_file.h"

#include "my_config.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "dd/impl/sdi_utils.h"      // dd::sdi_util::checked_return
#include "dd/types/entity_object.h" // dd::Entity_object
#include "dd/types/schema.h"        // dd::Schema
#include "dd/types/table.h"         // dd::Table
#include "handler.h"
#include "m_ctype.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_file.h" // mysql_file_create
#include "mysql/psi/psi_base.h"
#include "mysqld_error.h"
#include "sql_const.h"            // CREATE_MODE
#include "sql_table.h"            // build_table_filename
#include "table.h"


/**
  @file
  @ingroup sdi

  Storage and retrieval of SDIs to/from files. Default for SEs which do
  not have the ability to store SDIs in tablespaces. File storage is
  not transactional.
*/

using namespace dd::sdi_utils;

extern PSI_file_key key_file_sdi;
namespace {

bool write_sdi_file(const dd::String_type &fname, const MYSQL_LEX_CSTRING &sdi)
{
  File sdif= mysql_file_create(key_file_sdi, fname.c_str(), CREATE_MODE,
                               O_WRONLY | O_TRUNC, MYF(MY_FAE));
  if (sdif < 0)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_CANT_CREATE_FILE, MYF(0), fname.c_str(), my_errno,
             my_strerror(errbuf, sizeof(errbuf), my_errno()));
    return checked_return(true);
  }

  size_t bw= mysql_file_write(sdif,
                              reinterpret_cast<const uchar*>(sdi.str),
                              sdi.length, MYF(MY_FNABP));

  if (bw == MY_FILE_ERROR)
  {
#ifndef DBUG_OFF
    bool close_error=
#endif /* !DBUG_OFF */
      mysql_file_close(sdif, MYF(0));
    DBUG_ASSERT(close_error == false);
    return checked_return(true);
  }
  DBUG_ASSERT(bw == 0);
  return checked_return(mysql_file_close(sdif, MYF(MY_FAE)));
}

bool sdi_file_exists(const dd::String_type &fname, bool *res)
{
#ifndef _WIN32

  if (my_access(fname.c_str(), F_OK) == 0)
  {
    *res= true;
    return false;
  }

#else /* _WIN32 */
  // my_access cannot be used to test for the absence of a file on Windows
  WIN32_FILE_ATTRIBUTE_DATA fileinfo;
  BOOL result= GetFileAttributesEx(fname.c_str(), GetFileExInfoStandard,
                                   &fileinfo);
  if (result)
  {
    *res= true;
    return false;
  }

  my_osmaperr(GetLastError());

#endif /* _WIN32 */

  if (errno == ENOENT)
  {
    *res= false;
    return false;
  }

  char errbuf[MYSYS_STRERROR_SIZE];
  my_error(ER_CANT_GET_STAT, MYF(0), fname.c_str(), errno,
           my_strerror(errbuf, sizeof(errbuf), errno));
  return checked_return(true);
}

}

namespace dd {
namespace sdi_file {

String_type sdi_filename(const dd::Entity_object *eo,
                         const String_type &schema)
{
  typedef String_type::const_iterator CHARIT;
  const CHARIT begin= eo->name().begin();
  const CHARIT end= eo->name().end();
  CHARIT i= begin;
  size_t count= 0;

  while (i != end && count < dd::sdi_file::FILENAME_PREFIX_CHARS)
  {
    size_t charlen= my_mbcharlen(system_charset_info, static_cast<uchar>(*i));
    DBUG_ASSERT(charlen > 0);
    i += charlen;
    ++count;
  }

  Stringstream_type fnamestr;
  fnamestr << String_type(begin, i) << "_" << eo->id();

  char path[FN_REFLEN+1];
  bool was_truncated= false;
  build_table_filename(path, sizeof(path) - 1, schema.c_str(),
                       fnamestr.str().c_str(),
                       ".SDI", 0, &was_truncated);
  DBUG_ASSERT(!was_truncated);

  return String_type(path);
}

bool store(THD *thd, const MYSQL_LEX_CSTRING &sdi, const dd::Schema *schema)
{
  return checked_return(write_sdi_file(sdi_filename(schema, ""), sdi));
}

bool store(THD *thd, handlerton*, const MYSQL_LEX_CSTRING &sdi, const dd::Table *table,
           const dd::Schema *schema)
{
  return checked_return(write_sdi_file(sdi_filename(table,
                                                    schema->name()), sdi));
}

bool remove(const String_type &fname)
{
  return checked_return(mysql_file_delete(key_file_sdi, fname.c_str(),
                                          MYF(MY_FAE)));
}

static bool remove_sdi_file_if_exists(const String_type &fname)
{
  bool file_exists= false;
  if (sdi_file_exists(fname, &file_exists))
  {
    return checked_return(true);
  }

  if (!file_exists)
  {
    return false;
  }

  return checked_return(remove(fname));
}

bool remove(THD *thd, const dd::Schema *schema)
{
  String_type sdi_fname= sdi_filename(schema, "");
  return checked_return(remove_sdi_file_if_exists(sdi_fname));
}

bool remove(THD *thd, handlerton*, const dd::Table *table,
            const dd::Schema *schema)
{
  String_type sdi_fname= sdi_filename(table, schema->name());
  return checked_return(remove_sdi_file_if_exists(sdi_fname));
}
}
}
