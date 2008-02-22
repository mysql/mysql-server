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

#include <NDBT.hpp>
#include <BaseString.hpp>
#include <Properties.hpp>
#include <Vector.hpp>
#include <mysql.h>

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


class SqlResultSet : public Properties {
public:

  // Get row with number
  bool get_row(int row_num);
  // Load next row
  bool next(void);
  // Reset iterator
  void reset(void);
  // Remove current row from resultset
  void remove();

  SqlResultSet();
  ~SqlResultSet();

  const char* column(const char* col_name);
  uint columnAsInt(const char* col_name);

  uint insertId();
  uint affectedRows();
  uint numRows(void);
  uint mysqlErrno();
  const char* mysqlError();
  const char* mysqlSqlstate();

private:
  uint get_int(const char* name);
  const char* get_string(const char* name);

  const Properties* m_curr_row;
  uint m_curr_row_num;
};


#define DBU_FAILED 1
#define DBU_OK 0

class DbUtil
{
public:

  DbUtil(MYSQL* mysql);
  DbUtil(const char* dbname = "mysql",
         const char* user = "root",
         const char* pass = "",
         const char* suffix = NULL);
  ~DbUtil();

  bool doQuery(const char* query);
  bool doQuery(const char* query, SqlResultSet& result);
  bool doQuery(const char* query, const Properties& args, SqlResultSet& result);

  bool doQuery(BaseString& str);
  bool doQuery(BaseString& str, SqlResultSet& result);
  bool doQuery(BaseString& str, const Properties& args, SqlResultSet& result);

  bool waitConnected(int timeout);

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
  const char * getServerType(){return mysql_get_server_info(m_mysql);};
  const char * getError();

  MYSQL * getMysql(){return m_mysql;};
  MYSQL_STMT * STDCALL mysqlSimplePrepare(const char *query);

  void databaseLogout();
  void mysqlCloseStmHandle(MYSQL_STMT *my_stmt);

  int connect();
  void disconnect();
  int selectDb();
  int selectDb(const char *);
  int createDb(BaseString&);
  int getErrorNumber();

  unsigned long selectCountTable(const char * table);

protected:

  bool runQuery(const char* query,
               const Properties& args,
               SqlResultSet& rows);

  bool isConnected();

  MYSQL * m_mysql;
  bool m_free_mysql; /* Don't free mysql* if allocated elsewhere */

private:

  bool m_connected;

  BaseString m_host;       // Computer to connect to
  BaseString m_user;       // MySQL User
  BaseString m_pass;       // MySQL User Password
  BaseString m_dbname;     // Database to use
  BaseString m_socket;     // MySQL Server Unix Socket
  BaseString m_default_file;
  BaseString m_default_group;

  unsigned int m_port;     // MySQL Server port

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

