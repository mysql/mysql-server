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

#ifndef DD__SDI_TABLESPACE_INCLUDED
#define DD__SDI_TABLESPACE_INCLUDED

#include "dd/types/fwd.h"   // dd::Schema

class THD;
struct handlerton;

namespace dd {
namespace sdi_tablespace {
bool store(THD *thd, handlerton *hton, const sdi_t &sdi, const Schema *schema,
           const Table *table);
bool store(THD *thd, handlerton*, const sdi_t &sdi, const Table *table,
           const dd::Schema *schema);
bool store(handlerton *hton, const sdi_t &sdi, const Tablespace *tablespace);

bool remove(THD *thd, handlerton *hton, const Schema *schema,
            const Table *table);
bool remove(THD *thd, handlerton*, const Table *table, const Schema *schema);
bool remove(handlerton *hton, const Tablespace *tablespace);
}
}
#endif // !DD__SDI_TABLESPACE_INCLUDED
