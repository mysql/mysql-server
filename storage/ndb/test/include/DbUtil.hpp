/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// dbutil.h: interface for the database utilities class.
// Supplies a database to the test application

#ifndef DBUTIL_HPP
#define DBUTIL_HPP

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <mysql.h>
//include "rand.h"
#include <stdlib.h>
#include "BaseString.hpp"
#include "NDBT.hpp"

//#define DEBUG
#define  DIE_UNLESS(expr) \
           ((void) ((expr) ? 0 : (Die(__FILE__, __LINE__, #expr), 0)))
#define DIE(expr) \
          Die(__FILE__, __LINE__, #expr)
#define myerror(msg) printError(msg)
#define mysterror(stmt, msg) printStError(stmt, msg)
#define  CheckStmt(stmt) \
{ \
if ( stmt == 0) \
  myerror(NULL); \
DIE_UNLESS(stmt != 0); \
}

#define  check_execute(stmt, r) \
{ \
if (r) \
  mysterror(stmt, NULL); \
DIE_UNLESS(r == 0);\
}

#define DBU_FAILED 1
#define DBU_OK 0

class DbUtil
{
public:

  /* Deprecated, see DbUtil(dbname, suffix) */
  DbUtil(const char * databaseName);
  DbUtil(const char* dbname, const char* suffix = NULL);
  ~DbUtil();

  /* Deprecated, see connect() */
  void  databaseLogin(const char * system,
                      const char * usr,
                      const char * password,
                      unsigned int portIn,
                      const char * sockIn,
                      bool transactional);

  const char * getDbName()  {return m_dbname.c_str();};
  const char * getUser()    {return m_user.c_str();};
  const char * getPassword(){return m_pass.c_str();};
  const char * getHost()    {return m_host.c_str();};
  const char * getSocket()  {return m_socket.c_str();};
  const char * getServerType(){return mysql_get_server_info(mysql);};
  const char * getError();

  MYSQL * getMysql(){return mysql;};
  MYSQL_STMT * STDCALL mysqlSimplePrepare(const char *query);

  void databaseLogout();
  void mysqlCloseStmHandle(MYSQL_STMT *my_stmt);

  int connect();
  int selectDb();
  int selectDb(const char *);
  int createDb(BaseString&);
  int doQuery(BaseString&);
  int doQuery(const char *);
  int getErrorNumber();

  unsigned long selectCountTable(const char * table);

private:

  bool m_connected;

  BaseString m_host;       // Computer to connect to
  BaseString m_user;       // MySQL User
  BaseString m_pass;       // MySQL User Password
  BaseString m_dbname;     // Database to use
  BaseString m_socket;     // MySQL Server Unix Socket
  BaseString default_file;
  BaseString default_group;

  unsigned int m_port;     // MySQL Server port

  MYSQL * mysql;
  MYSQL_RES * m_result;
  MYSQL_ROW m_row;

  void setDbName(const char * name){m_dbname.assign(name);};
  void setUser(const char * user_name){m_user.assign(user_name);};
  void setPassword(const char * password){m_pass.assign(password);};
  void setHost(const char * system){m_host.assign(system);};
  void setPort(unsigned int portIn){m_port=portIn;};
  void setSocket(const char * sockIn){m_socket.assign(sockIn);};
  void printError(const char *msg);
  void printStError(MYSQL_STMT *stmt, const char *msg);
  void die(const char *file, int line, const char *expr); // stop program
  
};
#endif

