/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_ALTER_INSTANCE_INCLUDED
#define SQL_ALTER_INSTANCE_INCLUDED

class THD;
/*
  Base class for execution control for ALTER INSTANCE ... statement
*/
class Alter_instance
{
protected:
  THD *m_thd;
public:
  explicit Alter_instance(THD *thd)
    : m_thd(thd)
  {}
  virtual bool execute()= 0;
  bool log_to_binlog(bool is_transactional);
  virtual ~Alter_instance() {};
};

class Rotate_innodb_master_key : public Alter_instance
{
public:
  explicit Rotate_innodb_master_key(THD *thd)
    : Alter_instance(thd)
  {}

  bool execute();
  ~Rotate_innodb_master_key() {};
};

#endif /* SQL_ALTER_INSTANCE_INCLUDED */
