// dbutil.h: interface for the database utilities class.
//////////////////////////////////////////////////////////////////////
// Supplies a database to the test application
//////////////////////////////////////////////////////////////////////

#ifndef DBUTIL_HPP
#define DBUTIL_HPP

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <mysql.h>
//include "rand.h"
#include <stdlib.h>

//#define DEBUG
#define  DIE_UNLESS(expr) \
           ((void) ((expr) ? 0 : (Die(__FILE__, __LINE__, #expr), 0)))
#define DIE(expr) \
          Die(__FILE__, __LINE__, #expr)
#define myerror(msg) PrintError(msg)
#define mysterror(stmt, msg) PrintStError(stmt, msg)
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

#define TRUE 1
#define FALSE 0


class dbutil
{
public:

  dbutil(const char * databaseName);
  ~dbutil();

  void  DatabaseLogin(const char * system,
                      const char * usr,
                      const char * password,
                      unsigned int portIn,
                      const char * sockIn,
                      bool transactional);
  char * GetDbName(){return dbs;};
  char * GetUser(){return user;};
  char * GetPassword(){return pass;};
  char * GetHost(){return host;};
  char * GetSocket(){return socket;};
  const char * GetServerType(){return mysql_get_server_info(myDbHandel);};
  MYSQL* GetDbHandel(){return myDbHandel;};
  MYSQL_STMT *STDCALL MysqlSimplePrepare(const char *query);
  int Select_DB();
  int Do_Query(char * stm);
  const char * GetError();
  int GetErrorNumber();
  unsigned long SelectCountTable(const char * table);

private:

  //Connect variables
  char * databaseName; //hold results file name
  char host[256];                   // Computer to connect to
  char user[256];                   // MySQL User
  char pass[256];                   // MySQL User Password
  char dbs[256];                    // Database to use (TPCB)
  unsigned int port;               // MySQL Server port
  char socket[256];             // MySQL Server Unix Socket
  MYSQL  *myDbHandel;

  void DatabaseLogout();

  void SetDbName(const char * name){strcpy((char *)dbs, name);};
  void SetUser(const char * userName){strcpy((char *)user, userName);};
  void SetPassword(const char * password){strcpy((char *)pass,password);};
  void SetHost(const char * system){strcpy((char*)host, system);};
  void SetPort(unsigned int portIn){port=portIn;};
  void SetSocket(const char * sockIn){strcpy((char *)socket, sockIn);};
  void PrintError(const char *msg);
  void PrintStError(MYSQL_STMT *stmt, const char *msg);
  void Die(const char *file, int line, const char *expr); // stop program
  
};
#endif

