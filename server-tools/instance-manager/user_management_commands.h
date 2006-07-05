#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MANAGEMENT_CMD_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MANAGEMENT_CMD_H

/*
   Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
  This header contains declarations of classes inteded to support
  user-management commands (such as add user, get list of users, etc).

  The general idea is to have one interface (pure abstract class) for such a
  command. Each concrete user-management command is implemented in concrete
  class, derived from the common interface.
*/

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

/*************************************************************************
  User_management_cmd -- base class for User-management commands.
*************************************************************************/

class User_management_cmd
{
public:
  User_management_cmd()
  { }

  virtual ~User_management_cmd()
  { }

public:
  /*
    Executes user-management command.

    SYNOPSYS
      execute()

    RETURN
      See exit_codes.h for possible values.
  */

  virtual int execute() = 0;
};


/*************************************************************************
  Print_password_line_cmd: support for --print-password-line command-line
  option.
*************************************************************************/

class Print_password_line_cmd : public User_management_cmd
{
public:
  Print_password_line_cmd()
  { }

public:
  virtual int execute();
};


/*************************************************************************
  Add_user_cmd: support for --add-user command-line option.
*************************************************************************/

class Add_user_cmd : public User_management_cmd
{
public:
  Add_user_cmd()
  { }

public:
  virtual int execute();
};


/*************************************************************************
  Drop_user_cmd: support for --drop-user command-line option.
*************************************************************************/

class Drop_user_cmd : public User_management_cmd
{
public:
  Drop_user_cmd()
  { }

public:
  virtual int execute();
};


/*************************************************************************
  Edit_user_cmd: support for --edit-user command-line option.
*************************************************************************/

class Edit_user_cmd : public User_management_cmd
{
public:
  Edit_user_cmd()
  { }

public:
  virtual int execute();
};


/*************************************************************************
  Clean_db_cmd: support for --clean-db command-line option.
*************************************************************************/

class Clean_db_cmd : public User_management_cmd
{
public:
  Clean_db_cmd()
  { }

public:
  virtual int execute();
};


/*************************************************************************
  Check_db_cmd: support for --check-db command-line option.
*************************************************************************/

class Check_db_cmd : public User_management_cmd
{
public:
  Check_db_cmd()
  { }

public:
  virtual int execute();
};


/*************************************************************************
  List_users_cmd: support for --list-users command-line option.
*************************************************************************/

class List_users_cmd : public User_management_cmd
{
public:
  List_users_cmd()
  { }

public:
  virtual int execute();
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MANAGEMENT_CMD_H
