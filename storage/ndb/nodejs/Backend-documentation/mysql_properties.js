/*
  MySQL Connection Properties

*/ 

var MysqlDefaultConnectionProperties = {
  "implementation" : "mysql",
  "engine"         : "ndb",
  "database"       : "test",
  
  "mysql_host"     : "localhost",
  "mysql_port"     : 3306,
  "mysql_user"     : "root",
  "mysql_password" : "",
  "mysql_charset"  : "UTF8MB4",
  "mysql_sql_mode" : "STRICT_ALL_TABLES",
  "mysql_socket"   : null,
  "debug"          : true,
  "mysql_trace"    : false,
  "mysql_debug"    : false,
  "mysql_pool_size": 10
};


/* This file is valid JavaScript 
*/
module.exports = MysqlDefaultConnectionProperties;
