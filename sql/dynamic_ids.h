/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DYNAMIC_ID_H

#define DYNAMIC_ID_H

#include <my_sys.h>
#include <sql_string.h>

class Dynamic_ids
{
public:
    DYNAMIC_ARRAY dynamic_ids;

    Dynamic_ids(size_t param_size);
    virtual ~Dynamic_ids();

    bool pack_dynamic_ids(String *buffer)
    {
      return(do_pack_dynamic_ids(buffer));
    }

    bool unpack_dynamic_ids(char *param_dynamic_ids)
    {
      return(do_unpack_dynamic_ids(param_dynamic_ids));
    }

    bool search_id(const void *id)
    {
      return (do_search_id(id));
    }

protected:
    size_t size;

private:
    virtual bool do_pack_dynamic_ids(String *buffer)= 0;
    virtual bool do_unpack_dynamic_ids(char *param_dynamic_ids)= 0;
    virtual bool do_search_id(const void *id)= 0;
};

class Server_ids : public Dynamic_ids
{
public:
    Server_ids(size_t size): Dynamic_ids(size) { };
    virtual ~Server_ids() { };

private:
    bool do_pack_dynamic_ids(String *buffer);
    bool do_unpack_dynamic_ids(char *param_dynamic_ids);
    bool do_search_id(const void *id);
};

class Database_ids : public Dynamic_ids
{
public:
    Database_ids(size_t size): Dynamic_ids(size) { };
    virtual ~Database_ids() { };

private:
    bool do_pack_dynamic_ids(String *buffer);
    bool do_unpack_dynamic_ids(char *param_dynamic_ids);
    bool do_search_id(const void *id);
};
#endif
