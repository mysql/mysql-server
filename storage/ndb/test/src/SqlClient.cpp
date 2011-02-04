/*
   Copyright (C) 2008 MySQL AB
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

#include <SqlClient.hpp>
#include <NDBT_Output.hpp>
#include <NdbSleep.h>

SqlClient::SqlClient(const char* _user,
                       const char* _password,
                       const char* _group_suffix):
  connected(false),
  mysql(NULL),
  free_mysql(false)
{

  const char* env= getenv("MYSQL_HOME");
  if (env && strlen(env))
  {
    default_file.assfmt("%s/my.cnf", env);
  }

  if (_group_suffix != NULL){
    default_group.assfmt("client%s", _group_suffix);
  }
  else {
    default_group.assign("client.1.atrt");
  }

  g_info << "default_file: " << default_file.c_str() << endl;
  g_info << "default_group: " << default_group.c_str() << endl;

  user.assign(_user);
  password.assign(_password);
}


SqlClient::SqlClient(MYSQL* mysql):
  connected(true),
  mysql(mysql),
  free_mysql(false)
{
}


SqlClient::~SqlClient(){
  disconnect();
}


bool
SqlClient::isConnected(){
  if (connected == true)
  {
    assert(mysql);
    return true;
  }
  return connect() == 0;
}


int
SqlClient::connect(){
  disconnect();

//  mysql_debug("d:t:O,/tmp/client.trace");

  if ((mysql= mysql_init(NULL)) == NULL){
    g_err << "mysql_init failed" << endl;
    return -1;
  }

  /* Load connection parameters file and group */
  if (mysql_options(mysql, MYSQL_READ_DEFAULT_FILE, default_file.c_str()) ||
      mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, default_group.c_str()))
  {
    g_err << "mysql_options failed" << endl;
    disconnect();
    return 1;
  }

  /*
    Connect, read settings from my.cnf
    NOTE! user and password can be stored there as well
   */
  if (mysql_real_connect(mysql, NULL, user.c_str(),
                         password.c_str(), "atrt", 0, NULL, 0) == NULL)
  {
    g_err  << "Connection to atrt server failed: "<< mysql_error(mysql) << endl;
    disconnect();
    return -1;
  }

  g_err << "Connected to MySQL " << mysql_get_server_info(mysql)<< endl;

  connected = true;
  return 0;
}


bool
SqlClient::waitConnected(int timeout) {
  timeout*= 10;
  while(!isConnected()){
    if (timeout-- == 0)
      return false;
    NdbSleep_MilliSleep(100);
  }
  return true;
}


void
SqlClient::disconnect(){
  if (mysql != NULL){
    if (free_mysql)
      mysql_close(mysql);
    mysql= NULL;
  }
  connected = false;
}


static bool is_int_type(enum_field_types type){
  switch(type){
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_ENUM:
    return true;
  default:
    return false;
  }
  return false;
}


bool
SqlClient::runQuery(const char* sql,
                    const Properties& args,
                    SqlResultSet& rows){

  rows.clear();
  if (!isConnected())
    return false;

  g_debug << "runQuery: " << endl
          << " sql: '" << sql << "'" << endl;


  MYSQL_STMT *stmt= mysql_stmt_init(mysql);
  if (mysql_stmt_prepare(stmt, sql, strlen(sql)))
  {
    g_err << "Failed to prepare: " << mysql_error(mysql) << endl;
    return false;
  }

  uint params= mysql_stmt_param_count(stmt);
  MYSQL_BIND bind_param[params];
  bzero(bind_param, sizeof(bind_param));

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
      bind_param[i].buffer_length= strlen(val_s);
      g_debug << " param" << name.c_str() << ": " << val_s << endl;
      break;
    default:
      assert(false);
      break;
    }
  }
  if (mysql_stmt_bind_param(stmt, bind_param))
  {
    g_err << "Failed to bind param: " << mysql_error(mysql) << endl;
    mysql_stmt_close(stmt);
    return false;
  }

  if (mysql_stmt_execute(stmt))
  {
    g_err << "Failed to execute: " << mysql_error(mysql) << endl;
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
    g_err << "Failed to store result: " << mysql_error(mysql) << endl;
    mysql_stmt_close(stmt);
    return false;
  }

  uint row= 0;
  MYSQL_RES* res= mysql_stmt_result_metadata(stmt);
  if (res != NULL)
  {
    MYSQL_FIELD *fields= mysql_fetch_fields(res);
    uint num_fields= mysql_num_fields(res);
    MYSQL_BIND bind_result[num_fields];
    bzero(bind_result, sizeof(bind_result));

    for (uint i= 0; i < num_fields; i++)
    {
      if (is_int_type(fields[i].type)){
        bind_result[i].buffer_type= MYSQL_TYPE_LONG;
        bind_result[i].buffer= malloc(sizeof(int));
      }
      else
      {
        uint max_length= fields[i].max_length + 1;
        bind_result[i].buffer_type= MYSQL_TYPE_STRING;
        bind_result[i].buffer= malloc(max_length);
        bind_result[i].buffer_length= max_length;
      }
    }

    if (mysql_stmt_bind_result(stmt, bind_result)){
      g_err << "Failed to bind result: " << mysql_error(mysql) << endl;
      mysql_stmt_close(stmt);
      return false;
    }

    while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
    {
      Properties curr(true);
      for (uint i= 0; i < num_fields; i++){
        if (is_int_type(fields[i].type))
          curr.put(fields[i].name, *(int*)bind_result[i].buffer);
        else
          curr.put(fields[i].name, (char*)bind_result[i].buffer);
      }
      rows.put("row", row++, &curr);
    }

    mysql_free_result(res);

    for (uint i= 0; i < num_fields; i++)
      free(bind_result[i].buffer);

  }

  // Save stats in result set
  rows.put("rows", row);
  rows.put("affected_rows", mysql_affected_rows(mysql));
  rows.put("mysql_errno", mysql_errno(mysql));
  rows.put("mysql_error", mysql_error(mysql));
  rows.put("mysql_sqlstate", mysql_sqlstate(mysql));
  rows.put("insert_id", mysql_insert_id(mysql));

  mysql_stmt_close(stmt);
  return true;
}


bool
SqlClient::doQuery(const char* query){
  const Properties args;
  SqlResultSet result;
  return doQuery(query, args, result);
}


bool
SqlClient::doQuery(const char* query, SqlResultSet& result){
  Properties args;
  return doQuery(query, args, result);
}


bool
SqlClient::doQuery(const char* query, const Properties& args,
                   SqlResultSet& result){
  if (!runQuery(query, args, result))
    return false;
  result.get_row(0); // Load first row
  return true;
}


bool
SqlClient::doQuery(BaseString& str){
  return doQuery(str.c_str());
}


bool
SqlClient::doQuery(BaseString& str, SqlResultSet& result){
  return doQuery(str.c_str(), result);
}


bool
SqlClient::doQuery(BaseString& str, const Properties& args,
                   SqlResultSet& result){
  return doQuery(str.c_str(), args, result);
}




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

uint SqlResultSet::insertId(){
  return get_int("insert_id");
}

uint SqlResultSet::affectedRows(){
  return get_int("affected_rows");
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
  assert(get(name, &value));
  return value;
}

const char* SqlResultSet::get_string(const char* name){
  const char* value;
  assert(get(name, &value));
  return value;
}
