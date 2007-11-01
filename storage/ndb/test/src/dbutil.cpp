// dbutil.cpp: implementation of the database utilities class.
//
//////////////////////////////////////////////////////////////////////

#include "dbutil.hpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
dbutil::dbutil(const char * dbname)
{
  memset(host,' ',sizeof(host));
  memset(user,' ',sizeof(pass));
  memset(dbs,' ',sizeof(dbs));
  port = 0;
  memset(socket,' ',sizeof(socket));
  this->SetDbName(dbname);
}

dbutil::~dbutil()
{
  this->DatabaseLogout();
}

//////////////////////////////////////////////////////////////////////
// Database Login
//////////////////////////////////////////////////////////////////////
void dbutil::DatabaseLogin(const char* system,
                           const char* usr,
                           const char* password,
                           unsigned int portIn,
                           const char* sockIn,
                           bool transactional
                           ){
  if (!(myDbHandel = mysql_init(NULL))){
    myerror("mysql_init() failed");
    exit(1);
  }
  this->SetUser(usr);
  this->SetHost(system);
  this->SetPassword(password);
  this->SetPort(portIn);
  this->SetSocket(sockIn);

  if (!(mysql_real_connect(myDbHandel, host, user, pass, "test", port, socket, 0))){
    myerror("connection failed");
    mysql_close(myDbHandel);
    fprintf(stdout, "\n Check the connection options using --help or -?\n");
    exit(1);
  }

  myDbHandel->reconnect= 1;

  /* set AUTOCOMMIT */
  if(!transactional){
    mysql_autocommit(myDbHandel, TRUE);
  }
  else{
    mysql_autocommit(myDbHandel, FALSE);
  }

  fprintf(stdout, "\n\tConnected to MySQL server version: %s (%lu)\n\n", 
          mysql_get_server_info(myDbHandel),
    (unsigned long) mysql_get_server_version(myDbHandel));
}

//////////////////////////////////////////////////////////////////////
// Database Logout
//////////////////////////////////////////////////////////////////////
void dbutil::DatabaseLogout(){
  if (myDbHandel){
    fprintf(stdout, "\n\tClosing the MySQL database connection ...\n\n");
    mysql_close(myDbHandel);
  }
}

//////////////////////////////////////////////////////////////////////
// Prepare MySQL Statements Cont
//////////////////////////////////////////////////////////////////////
MYSQL_STMT *STDCALL dbutil::MysqlSimplePrepare(const char *query){
#ifdef DEBUG
printf("Inside dbutil::MysqlSimplePrepare\n");
#endif
int result = 0;
  MYSQL_STMT *my_stmt= mysql_stmt_init(this->GetDbHandel());
  if (my_stmt && (result = mysql_stmt_prepare(my_stmt, query, strlen(query)))){
    printf("res = %s\n",mysql_stmt_error(my_stmt));
    mysql_stmt_close(my_stmt);
    return 0;
  }
  return my_stmt;
}
//////////////////////////////////////////////////////////////////////
// Error Printing
//////////////////////////////////////////////////////////////////////
void dbutil::PrintError(const char *msg){
  if (this->GetDbHandel()
      && mysql_errno(this->GetDbHandel())){
      if (this->GetDbHandel()->server_version){
        fprintf(stdout, "\n [MySQL-%s]",
                this->GetDbHandel()->server_version);
      }
      else
        fprintf(stdout, "\n [MySQL]");
      fprintf(stdout, "[%d] %s\n",
              mysql_errno(this->GetDbHandel()),
              mysql_error(this->GetDbHandel()));
  }
  else if (msg)
    fprintf(stderr, " [MySQL] %s\n", msg);
}

void dbutil::PrintStError(MYSQL_STMT *stmt, const char *msg)
{
  if (stmt && mysql_stmt_errno(stmt))
  {
    if (this->GetDbHandel()
        && this->GetDbHandel()->server_version)
      fprintf(stdout, "\n [MySQL-%s]",
              this->GetDbHandel()->server_version);
    else
      fprintf(stdout, "\n [MySQL]");

    fprintf(stdout, "[%d] %s\n", mysql_stmt_errno(stmt),
    mysql_stmt_error(stmt));
  }
  else if (msg)
   fprintf(stderr, " [MySQL] %s\n", msg);
}
/////////////////////////////////////////////////////
int dbutil::Select_DB()
{
  return mysql_select_db(this->GetDbHandel(),
                         this->GetDbName());
}
////////////////////////////////////////////////////
int dbutil::Do_Query(char * stm)
{
  return mysql_query(this->GetDbHandel(), stm);
}
////////////////////////////////////////////////////
const char * dbutil::GetError()
{
  return mysql_error(this->GetDbHandel());
}
////////////////////////////////////////////////////
int dbutil::GetErrorNumber()
{
  return mysql_errno(this->GetDbHandel());
}
////////////////////////////////////////////////////
unsigned long dbutil::SelectCountTable(const char * table)
{
	unsigned long count = 0;
    MYSQL_RES *result;
    char query[1024];
    MYSQL_ROW row;
        
    sprintf(query,"select count(*) from `%s`", table);
    if (mysql_query(this->GetDbHandel(),query) || !(result=mysql_store_result(this->GetDbHandel())))
    {
      printf("error\n");
      return 1;
    }
    row= mysql_fetch_row(result);
    count= (ulong) strtoull(row[0], (char**) 0, 10);
    mysql_free_result(result);
    
    return count;
}
void dbutil::Die(const char *file, int line, const char *expr){
  fprintf(stderr, "%s:%d: check failed: '%s'\n", file, line, expr);
  abort();
}


