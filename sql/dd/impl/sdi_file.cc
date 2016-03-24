/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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


#include "mysql/psi/mysql_file.h" // mysql_file_create
#include "sql_table.h"            // build_table_filename
#include "sql_const.h"            // CREATE_MODE
#include "sql_class.h"            // THD

#include "dd/impl/sdi_utils.h"    // dd::sdi_util::checked_return
#include "dd/types/schema.h"      // dd::Schema
#include "dd/types/table.h"       // dd::Table

#include <string>
#include <sstream>

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

bool write_sdi_file(const std::string &fname, const dd::sdi_t &sdi)
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
                              reinterpret_cast<const uchar*>(sdi.c_str()),
                              sdi.size(), MYF(MY_FNABP));

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

bool sdi_file_exists(const std::string &fname, bool *res)
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

std::string sdi_filename(const dd::Entity_object *eo,
                         const std::string &schema)
{
  typedef std::string::const_iterator CHARIT;
  const CHARIT begin= eo->name().begin();
  const CHARIT end= eo->name().end();
  CHARIT i= begin;
  int count= 0;

  while (i != end && count < 16)
  {
    i += my_mbcharlen(system_charset_info, *i);
    ++count;
  }

  std::ostringstream fnamestr;
  fnamestr << std::string(begin, i) << "_" << eo->id();

  char path[FN_REFLEN+1];
  bool was_truncated= false;
  build_table_filename(path, sizeof(path) - 1, schema.c_str(),
                       fnamestr.str().c_str(),
                       ".SDI", 0, &was_truncated);
  DBUG_ASSERT(!was_truncated);

  return std::string(path);
}

bool store(THD *thd, const dd::sdi_t &sdi, const dd::Schema *schema)
{
  return checked_return(write_sdi_file(sdi_filename(schema, ""), sdi));
}

bool store(THD *thd, handlerton*, const dd::sdi_t &sdi, const dd::Table *table,
           const dd::Schema *schema)
{
  return checked_return(write_sdi_file(sdi_filename(table,
                                                    schema->name()), sdi));
}

bool remove(const std::string &fname)
{
  return checked_return(mysql_file_delete(key_file_sdi, fname.c_str(),
                                          MYF(MY_FAE)));
}

static bool remove_sdi_file_if_exists(const std::string &fname)
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
  std::string sdi_fname= sdi_filename(schema, "");
  return checked_return(remove_sdi_file_if_exists(sdi_fname));
}

bool remove(THD *thd, handlerton*, const dd::Table *table,
            const dd::Schema *schema)
{
  std::string sdi_fname= sdi_filename(table, schema->name());
  return checked_return(remove_sdi_file_if_exists(sdi_fname));
}
}
}
