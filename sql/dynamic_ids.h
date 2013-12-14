/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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
