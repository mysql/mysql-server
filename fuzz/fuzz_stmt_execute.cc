#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <mysql.h>
#include <mysql/client_plugin.h>
#include <mysqld_error.h>
#include "violite.h"

using namespace std;

#define STRING_SIZE 50

#define INSERT_SAMPLE "INSERT INTO \
test_table(col1,col2,col3) \
VALUES(?,?,?)"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    MYSQL mysql;
    MYSQL_BIND    bind[3];
    my_ulonglong  affected_rows;
    int           param_count;
    short         small_data;
    int           int_data;
    char          str_data[STRING_SIZE];
    unsigned long str_length;
    bool          is_null;

    mysql_init(&mysql);
    mysql.options.protocol = MYSQL_PROTOCOL_FUZZ;
    // The fuzzing takes place on network data received from server
    sock_initfuzz(Data,Size);
    if (!mysql_real_connect(&mysql,"localhost","root","root","",0,NULL,0))
    {
        return 0;
    }

     MYSQL_STMT *stmt = mysql_stmt_init(&mysql);
     if (!stmt)
     {
         mysql_stmt_close(stmt);
         mysql_close(&mysql);
         return 0;
     }
     if (mysql_stmt_prepare(stmt, INSERT_SAMPLE, strlen(INSERT_SAMPLE)))
     {
         mysql_stmt_close(stmt);
         mysql_close(&mysql);
         return 0;
     }

     param_count= mysql_stmt_param_count(stmt);

     if (param_count != 3) /* validate parameter count */
     {
          mysql_stmt_close(stmt);
          mysql_close(&mysql);
          return 0;
     }

     memset(bind, 0, sizeof(bind));

     /* INTEGER PARAM */
     bind[0].buffer_type= MYSQL_TYPE_LONG;
     bind[0].buffer= (char *)&int_data;
     bind[0].is_null= 0;
     bind[0].length= 0;

     /* STRING PARAM */
     bind[1].buffer_type= MYSQL_TYPE_STRING;
     bind[1].buffer= (char *)str_data;
     bind[1].buffer_length= STRING_SIZE;
     bind[1].is_null= 0;
     bind[1].length= &str_length;

     /* SMALLINT PARAM */
     bind[2].buffer_type= MYSQL_TYPE_SHORT;
     bind[2].buffer= (char *)&small_data;
     bind[2].is_null= &is_null;
     bind[2].length= 0;

     /* Bind the buffers */
     if (mysql_stmt_bind_param(stmt, bind))
     {
          mysql_stmt_close(stmt);
          mysql_close(&mysql);
          return 0;
     }

     /* Specify the data values for the first row */
     int_data= 10;             /* integer */
     strncpy(str_data, "MySQL", STRING_SIZE); /* string  */
     str_length= strlen(str_data);

     /* INSERT SMALLINT data as NULL */
     is_null= 1;

     /* Execute the INSERT statement - 1*/
     if (mysql_stmt_execute(stmt))
     {
          mysql_stmt_close(stmt);
          mysql_close(&mysql);
          return 0;
     }

     /* Get the number of affected rows */
     affected_rows= mysql_stmt_affected_rows(stmt);

     if (affected_rows != 1) /* validate affected rows */
     {
          mysql_stmt_close(stmt);
          mysql_close(&mysql);
          return 0;
     }

     /* Specify data values for second row,
        then re-execute the statement */
     int_data= 1000;
     strncpy(str_data, "The most popular Open Source database", STRING_SIZE);
     str_length= strlen(str_data);
     small_data= 1000;         /* smallint */
     is_null= 0;               /* reset */

     /* Execute the INSERT statement - 2*/
     if (mysql_stmt_execute(stmt))
     {
          mysql_stmt_close(stmt);
          mysql_close(&mysql);
          return 0;
     }

     /* Get the total rows affected */
     affected_rows= mysql_stmt_affected_rows(stmt);

     if (affected_rows != 1) /* validate affected rows */
     {
          mysql_stmt_close(stmt);
          mysql_close(&mysql);
          return 0;
     }

      mysql_stmt_close(stmt);
      mysql_close(&mysql);
      return 0;
}
