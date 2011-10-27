/*
   Copyright (C) 2007, 2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* DbUtil.cpp: implementation of the database utilities class.*/

#include "DbUtil.hpp"
#include <NdbSleep.h>
#include <NdbAutoPtr.hpp>

/* Constructors */

DbUtil::DbUtil(const char* _dbname,
               const char* _suffix):
  m_mysql(NULL),
  m_free_mysql(true),
  m_connected(false),
  m_user("root"),
  m_pass(""),
  m_dbname(_dbname),
  m_silent(false)
{
  const char* env= getenv("MYSQL_HOME");
  if (env && strlen(env))
  {
    m_default_file.assfmt("%s/my.cnf", env);
  }

  if (_suffix != NULL){
    m_default_group.assfmt("client%s", _suffix);
  }
  else {
    m_default_group.assign("client.1.master");
  }

  ndbout << "default_file: " << m_default_file.c_str() << endl;
  ndbout << "default_group: " << m_default_group.c_str() << endl;
}



DbUtil::DbUtil(MYSQL* mysql):
  m_mysql(mysql),
  m_free_mysql(false),
  m_connected(true)
{
}


bool
DbUtil::isConnected(){
  if (m_connected == true)
  {
    assert(m_mysql);
    return true;
  }
  return connect();
}


bool
DbUtil::waitConnected(int timeout) {
  timeout*= 10;
  while(!isConnected()){
    if (timeout-- == 0)
      return false;
    NdbSleep_MilliSleep(100);
  }
  return true;
}


void
DbUtil::disconnect(){
  if (m_mysql != NULL){
    if (m_free_mysql)
      mysql_close(m_mysql);
    m_mysql= NULL;
  }
  m_connected = false;
}


/* Destructor */

DbUtil::~DbUtil()
{
  disconnect();
}

/* Database Login */

bool
DbUtil::databaseLogin(const char* system, const char* usr,
                           const char* password, unsigned int portIn,
                           const char* sockIn, bool transactional)
{
  if (!(m_mysql = mysql_init(NULL)))
  {
    myerror("DB Login-> mysql_init() failed");
    return false;
  }
  setUser(usr);
  setHost(system);
  setPassword(password);
  setPort(portIn);
  setSocket(sockIn);
  m_dbname.assign("test");

  if (!(mysql_real_connect(m_mysql, 
                           m_host.c_str(), 
                           m_user.c_str(), 
                           m_pass.c_str(), 
                           m_dbname.c_str(),
                           m_port, 
                           m_socket.c_str(), 0)))
  {
    myerror("connection failed");
    disconnect();
    return false;
  }

  m_mysql->reconnect = TRUE;

  /* set AUTOCOMMIT */
  if(!transactional)
    mysql_autocommit(m_mysql, TRUE);
  else
    mysql_autocommit(m_mysql, FALSE);

  #ifdef DEBUG
    printf("\n\tConnected to MySQL server version: %s (%lu)\n\n", 
           mysql_get_server_info(m_mysql),
           (unsigned long) mysql_get_server_version(m_mysql));
  #endif
  selectDb();
  m_connected= true;
  return true;
}

/* Database Connect */

bool
DbUtil::connect()
{
  if (!(m_mysql = mysql_init(NULL)))
  {
    myerror("DB connect-> mysql_init() failed");
    return false;
  }

  /* Load connection parameters file and group */
  if (mysql_options(m_mysql, MYSQL_READ_DEFAULT_FILE, m_default_file.c_str()) ||
      mysql_options(m_mysql, MYSQL_READ_DEFAULT_GROUP, m_default_group.c_str()))
  {
    myerror("DB Connect -> mysql_options failed");
    disconnect();
    return false;
  }

  /*
    Connect, read settings from my.cnf
    NOTE! user and password can be stored there as well
  */
  if (mysql_real_connect(m_mysql, NULL,
                         m_user.c_str(),
                         m_pass.c_str(),
                         m_dbname.c_str(),
                         0, NULL, 0) == NULL)
  {
    myerror("connection failed");
    disconnect();
    return false;
  }
  selectDb();
  m_connected= true;
  assert(m_mysql);
  return true;
}


/* Database Logout */

void
DbUtil::databaseLogout()
{
  if (m_mysql){
    #ifdef DEBUG
      printf("\n\tClosing the MySQL database connection ...\n\n");
    #endif
    mysql_close(m_mysql);
  }
}

/* Prepare MySQL Statements Cont */

MYSQL_STMT *STDCALL 
DbUtil::mysqlSimplePrepare(const char *query)
{
  #ifdef DEBUG
    printf("Inside DbUtil::mysqlSimplePrepare\n");
  #endif
  MYSQL_STMT *my_stmt= mysql_stmt_init(this->getMysql());
  if (my_stmt && mysql_stmt_prepare(my_stmt, query, (unsigned long)strlen(query))){
    this->printStError(my_stmt,"Prepare Statement Failed");
    mysql_stmt_close(my_stmt);
    return NULL;
  }
  return my_stmt;
}

/* Close MySQL Statements Handle */

void 
DbUtil::mysqlCloseStmHandle(MYSQL_STMT *my_stmt)
{
  mysql_stmt_close(my_stmt);
}
 
/* Error Printing */

void
DbUtil::printError(const char *msg)
{
  if (m_mysql && mysql_errno(m_mysql))
  {
    if (m_mysql->server_version)
      printf("\n [MySQL-%s]", m_mysql->server_version);
    else
      printf("\n [MySQL]");
      printf("[%d] %s\n", getErrorNumber(), getError());
  }
  else if (msg)
    printf(" [MySQL] %s\n", msg);
}

void
DbUtil::printStError(MYSQL_STMT *stmt, const char *msg)
{
  if (stmt && mysql_stmt_errno(stmt))
  {
    if (m_mysql && m_mysql->server_version)
      printf("\n [MySQL-%s]", m_mysql->server_version);
    else
      printf("\n [MySQL]");

    printf("[%d] %s\n", mysql_stmt_errno(stmt),
    mysql_stmt_error(stmt));
  }
  else if (msg)
    printf("[MySQL] %s\n", msg);
}

/* Select which database to use */

bool
DbUtil::selectDb()
{
  if ((getDbName()) != NULL)
  {
    if(mysql_select_db(m_mysql, this->getDbName()))
    {
      //printError("mysql_select_db failed");
      return false;
    }
    return true;   
  }
  printError("getDbName() == NULL");
  return false;
}

bool
DbUtil::selectDb(const char * m_db)
{
  {
    if(mysql_select_db(m_mysql, m_db))
    {
      printError("mysql_select_db failed");
      return false;
    }
    return true;
  }
}

bool
DbUtil::createDb(BaseString& m_db)
{
  BaseString stm;
  setDbName(m_db.c_str());
  
  {
    if(selectDb())
    {
      stm.assfmt("DROP DATABASE %s", m_db.c_str());
      if(!doQuery(m_db.c_str()))
        return false;
    }
    stm.assfmt("CREATE DATABASE %s", m_db.c_str());
    if(!doQuery(m_db.c_str()))
      return false;
    return true;
  }
}


/* Count Table Rows */

unsigned long long
DbUtil::selectCountTable(const char * table)
{
  BaseString query;
  SqlResultSet result;

  query.assfmt("select count(*) as count from %s", table);
  if (!doQuery(query, result)) {
    printError("select count(*) failed");
    return -1;
  }
   return result.columnAsLong("count");
}


/* Run Simple Queries */


bool
DbUtil::runQuery(const char* sql,
                 const Properties& args,
                 SqlResultSet& rows){

  clear_error();
  rows.clear();
  if (!isConnected())
    return false;
  assert(m_mysql);

  g_debug << "runQuery: " << endl
          << " sql: '" << sql << "'" << endl;


  MYSQL_STMT *stmt= mysql_stmt_init(m_mysql);
  if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)))
  {
    report_error("Failed to prepare: ", m_mysql);
    return false;
  }

  uint params= mysql_stmt_param_count(stmt);
  MYSQL_BIND *bind_param = new MYSQL_BIND[params];
  NdbAutoObjArrayPtr<MYSQL_BIND> _guard(bind_param);

  memset(bind_param, 0, params * sizeof(MYSQL_BIND));

  for(uint i= 0; i < mysql_stmt_param_count(stmt); i++)
  {
    BaseString name;
    name.assfmt("%d", i);
    // Parameters are named 0, 1, 2...
    if (!args.contains(name.c_str()))
    {
      g_err << "param " << i << " missing" << endl;
      assert(false);
    }
    PropertiesType t;
    Uint32 val_i;
    const char* val_s;
    args.getTypeOf(name.c_str(), &t);
    switch(t) {
    case PropertiesType_Uint32:
      args.get(name.c_str(), &val_i);
      bind_param[i].buffer_type= MYSQL_TYPE_LONG;
      bind_param[i].buffer= (char*)&val_i;
      g_debug << " param" << name.c_str() << ": " << val_i << endl;
      break;
    case PropertiesType_char:
      args.get(name.c_str(), &val_s);
      bind_param[i].buffer_type= MYSQL_TYPE_STRING;
      bind_param[i].buffer= (char*)val_s;
      bind_param[i].buffer_length= (unsigned long)strlen(val_s);
      g_debug << " param" << name.c_str() << ": " << val_s << endl;
      break;
    default:
      assert(false);
      break;
    }
  }
  if (mysql_stmt_bind_param(stmt, bind_param))
  {
    report_error("Failed to bind param: ", m_mysql);
    mysql_stmt_close(stmt);
    return false;
  }

  if (mysql_stmt_execute(stmt))
  {
    report_error("Failed to execute: ", m_mysql);
    mysql_stmt_close(stmt);
    return false;
  }

  /*
    Update max_length, making it possible to know how big
    buffers to allocate
  */
  my_bool one= 1;
  mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (void*) &one);

  if (mysql_stmt_store_result(stmt))
  {
    report_error("Failed to store result: ", m_mysql);
    mysql_stmt_close(stmt);
    return false;
  }

  uint row= 0;
  MYSQL_RES* res= mysql_stmt_result_metadata(stmt);
  if (res != NULL)
  {
    MYSQL_FIELD *fields= mysql_fetch_fields(res);
    uint num_fields= mysql_num_fields(res);
    MYSQL_BIND *bind_result = new MYSQL_BIND[num_fields];
    NdbAutoObjArrayPtr<MYSQL_BIND> _guard1(bind_result);
    memset(bind_result, 0, num_fields * sizeof(MYSQL_BIND));

    for (uint i= 0; i < num_fields; i++)
    {
      unsigned long buf_len= sizeof(int);

      switch(fields[i].type){
      case MYSQL_TYPE_STRING:
        buf_len = fields[i].length + 1;
        break;
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_VAR_STRING:
        buf_len= fields[i].max_length + 1;
        break;
      case MYSQL_TYPE_LONGLONG:
        buf_len= sizeof(long long);
        break;
      case MYSQL_TYPE_LONG:
        buf_len = sizeof(long);
        break;
      default:
        break;
      }
      
      bind_result[i].buffer_type= fields[i].type;
      bind_result[i].buffer= malloc(buf_len);
      bind_result[i].buffer_length= buf_len;
      bind_result[i].is_null = (my_bool*)malloc(sizeof(my_bool));
      * bind_result[i].is_null = 0;
    }

    if (mysql_stmt_bind_result(stmt, bind_result)){
      report_error("Failed to bind result: ", m_mysql);
      mysql_stmt_close(stmt);
      return false;
    }

    while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
    {
      Properties curr(true);
      for (uint i= 0; i < num_fields; i++){
        if (* bind_result[i].is_null)
          continue;
        switch(fields[i].type){
        case MYSQL_TYPE_STRING:
	  ((char*)bind_result[i].buffer)[fields[i].max_length] = 0;
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
          curr.put(fields[i].name, (char*)bind_result[i].buffer);
          break;

        case MYSQL_TYPE_LONGLONG:
          curr.put64(fields[i].name,
                     *(unsigned long long*)bind_result[i].buffer);
          break;

        default:
          curr.put(fields[i].name, *(int*)bind_result[i].buffer);
          break;
        }
      }
      rows.put("row", row++, &curr);
    }

    mysql_free_result(res);

    for (uint i= 0; i < num_fields; i++)
    {
      free(bind_result[i].buffer);
      free(bind_result[i].is_null);
    }
  }

  // Save stats in result set
  rows.put("rows", row);
  rows.put64("affected_rows", mysql_affected_rows(m_mysql));
  rows.put("mysql_errno", mysql_errno(m_mysql));
  rows.put("mysql_error", mysql_error(m_mysql));
  rows.put("mysql_sqlstate", mysql_sqlstate(m_mysql));
  rows.put64("insert_id", mysql_insert_id(m_mysql));

  mysql_stmt_close(stmt);
  return true;
}


bool
DbUtil::doQuery(const char* query){
  const Properties args;
  SqlResultSet result;
  return doQuery(query, args, result);
}


bool
DbUtil::doQuery(const char* query, SqlResultSet& result){
  Properties args;
  return doQuery(query, args, result);
}


bool
DbUtil::doQuery(const char* query, const Properties& args,
                   SqlResultSet& result){
  if (!runQuery(query, args, result))
    return false;
  result.get_row(0); // Load first row
  return true;
}

bool
DbUtil::doQuery(const char* query, const Properties& args){
  SqlResultSet result;
  return doQuery(query, args, result);
}


bool
DbUtil::doQuery(BaseString& str){
  return doQuery(str.c_str());
}


bool
DbUtil::doQuery(BaseString& str, SqlResultSet& result){
  return doQuery(str.c_str(), result);
}


bool
DbUtil::doQuery(BaseString& str, const Properties& args,
                   SqlResultSet& result){
  return doQuery(str.c_str(), args, result);
}


bool
DbUtil::doQuery(BaseString& str, const Properties& args){
  return doQuery(str.c_str(), args);
}


/* Return MySQL Error String */

const char *
DbUtil::getError()
{
  return mysql_error(this->getMysql());
}

/* Return MySQL Error Number */

int
DbUtil::getErrorNumber()
{
  return mysql_errno(this->getMysql());
}

void
DbUtil::report_error(const char* message, MYSQL* mysql)
{
  m_last_errno= mysql_errno(mysql);
  m_last_error.assfmt("%d: %s", m_last_errno, mysql_error(mysql));

  if (!m_silent)
    g_err << message << m_last_error << endl;
}


/* DIE */

void
DbUtil::die(const char *file, int line, const char *expr)
{
  printf("%s:%d: check failed: '%s'\n", file, line, expr);
  abort();
}


/* SqlResultSet */

bool
SqlResultSet::get_row(int row_num){
  if(!get("row", row_num, &m_curr_row)){
    return false;
  }
  return true;
}


bool
SqlResultSet::next(void){
  return get_row(++m_curr_row_num);
}


// Reset iterator
void SqlResultSet::reset(void){
  m_curr_row_num= -1;
  m_curr_row= 0;
}


// Remove row from resultset
void SqlResultSet::remove(){
  BaseString row_name;
  row_name.assfmt("row_%d", m_curr_row_num);
  Properties::remove(row_name.c_str());
}


// Clear all rows and reset iterator
void SqlResultSet::clear(){
  reset();
  Properties::clear();
}


SqlResultSet::SqlResultSet(): m_curr_row(0), m_curr_row_num(-1){
}


SqlResultSet::~SqlResultSet(){
}


const char* SqlResultSet::column(const char* col_name){
  const char* value;
  if (!m_curr_row){
    g_err << "ERROR: SqlResultSet::column("<< col_name << ")" << endl
          << "There is no row loaded, call next() before "
          << "acessing the column values" << endl;
    assert(m_curr_row);
  }
  if (!m_curr_row->get(col_name, &value))
    return NULL;
  return value;
}


uint SqlResultSet::columnAsInt(const char* col_name){
  uint value;
  if (!m_curr_row){
    g_err << "ERROR: SqlResultSet::columnAsInt("<< col_name << ")" << endl
          << "There is no row loaded, call next() before "
          << "acessing the column values" << endl;
    assert(m_curr_row);
  }
  if (!m_curr_row->get(col_name, &value))
    return (uint)-1;
  return value;
}

unsigned long long SqlResultSet::columnAsLong(const char* col_name){
  unsigned long long value;
  if (!m_curr_row){
    g_err << "ERROR: SqlResultSet::columnAsLong("<< col_name << ")" << endl
          << "There is no row loaded, call next() before "
          << "acessing the column values" << endl;
    assert(m_curr_row);
  }
  if (!m_curr_row->get(col_name, &value))
    return (uint)-1;
  return value;
}


unsigned long long SqlResultSet::insertId(){
  return get_long("insert_id");
}


unsigned long long SqlResultSet::affectedRows(){
  return get_long("affected_rows");
}

uint SqlResultSet::numRows(void){
  return get_int("rows");
}


uint SqlResultSet::mysqlErrno(void){
  return get_int("mysql_errno");
}


const char* SqlResultSet::mysqlError(void){
  return get_string("mysql_error");
}


const char* SqlResultSet::mysqlSqlstate(void){
  return get_string("mysql_sqlstate");
}


uint SqlResultSet::get_int(const char* name){
  uint value;
  get(name, &value);
  return value;
}

unsigned long long SqlResultSet::get_long(const char* name){
  unsigned long long value;
  get(name, &value);
  return value;
}


const char* SqlResultSet::get_string(const char* name){
  const char* value;
  get(name, &value);
  return value;
}

/* EOF */

