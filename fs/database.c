/* Copyright (C) 2000 db AB & db Finland AB & TCX DataKonsult AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Database functions
 *
 * Using these functions we emulate filesystem behaviour on top of SQL 
 * database.
 * Written by Tõnu Samuel <tonu@please.do.not.remove.this.spam.ee>
 *
 * FIXME:
 * - Direct handling of database handlers without SQL parsing overhead
 * - connection pool
 * - configurable function name/file name mappings
 */


#include "libmysqlfs.h"
#include "mysqlcorbafs.h"
#include <unistd.h>
#include <string.h>
#include <my_sys.h>

DYNAMIC_ARRAY field_array;

/*
 * ** dbConnect -- connects to the host and selects DB.
 * **        Also checks whether the tablename is a valid table name.
 * */
int db_connect(char *host, char *user,char *passwd)
{
  DBUG_ENTER("db_connect");
  DBUG_PRINT("enter",("host: '%s', user: '%s', passwd: '%s'", host, user, passwd));

  if (verbose)
  {
    fprintf(stderr, "# Connecting to %s...\n", host ? host : "localhost");
  }
  mysql_init(&connection);
  if (opt_compress)
    mysql_options(&connection,MYSQL_OPT_COMPRESS,NullS);
#ifdef HAVE_OPENSSL
    if (opt_use_ssl)
      mysql_ssl_set(&connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
        opt_ssl_capath);
#endif
  if (!(sock= mysql_real_connect(&connection,host,user,passwd,
       NULL,opt_mysql_port,opt_mysql_unix_port,0)))
  {
    DBerror(&connection, "when trying to connect");
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
} /* dbConnect */


/*
 * ** dbDisconnect -- disconnects from the host.
 * */
void db_disconnect(char *host)
{
  DBUG_ENTER("db_disconnect");
  DBUG_PRINT("enter",("host: '%s'", host));
  if (verbose)
     fprintf(stderr, "# Disconnecting from %s...\n", host ? host : "localhost");
  mysql_close(sock);
  DBUG_VOID_RETURN;
} /* dbDisconnect */

#define OUTPUT(x) strcpy(buffptr,x); buffptr+=strlen(x);
#define OUTPUT_TOP(x)    strcpy(topptr,x); topptr+=strlen(x);
#define OUTPUT_MIDDLE(x) strcpy(midptr,x); midptr+=strlen(x);
#define OUTPUT_BOTTOM(x) strcpy(botptr,x); botptr+=strlen(x);
#define OUTPUT_HEADER(x) strcpy(hdrptr,x); hdrptr+=strlen(x);

void db_show_result(MYSQL* sock, char *b, struct format *f)
{
   MYSQL_ROW  row;
   MYSQL_RES  *result;
   MYSQL_FIELD *field;
   char *buffptr;
   char topseparator[BUFLEN]="";
   char middleseparator[BUFLEN]="";
   char bottomseparator[BUFLEN]="";
   char header[BUFLEN]="";
   char *topptr=topseparator;
   char *midptr=middleseparator;
   char *botptr=bottomseparator;
   char *hdrptr=header;
   uint i,count, length;

   DBUG_ENTER("db_show_result");
   DBUG_PRINT("enter",("b: '%s', f '%x'", b, f));

   result=mysql_store_result(sock);

   buffptr=b;
   OUTPUT(f->tablestart)
   
   OUTPUT_TOP(f->leftuppercorner);
   OUTPUT_MIDDLE(f->leftcross);
   OUTPUT_BOTTOM(f->leftdowncorner);
   OUTPUT_HEADER(f->headerrowstart);


   count=mysql_num_fields(result);
//   while ((field = mysql_fetch_field(result)))
   for(i=0 ; i < count ; ++i)
   {
      field = mysql_fetch_field(result);
      length=(uint) strlen(field->name);
      OUTPUT_HEADER(f->headercellstart);

      length=max(length,field->max_length);
      if (length < 4 && !IS_NOT_NULL(field->flags))
         length=4;               // Room for "NULL"
      field->max_length=length;

      memset(topptr,'=',field->max_length); 
      memset(midptr,'-',field->max_length); 
      memset(botptr,'=',field->max_length); 

      sprintf(hdrptr,"%-*s",field->max_length,field->name);
      //num_flag[off]= IS_NUM(field->type);
      
      topptr+=field->max_length;
      midptr+=field->max_length;
      botptr+=field->max_length;
      hdrptr+=field->max_length;

      if(i<count-1) {
         OUTPUT_TOP(f->topcross);
         OUTPUT_MIDDLE(f->middlecross);
         OUTPUT_BOTTOM(f->bottomcross);
         OUTPUT_HEADER(f->headercellseparator);
      } 
   }
   OUTPUT_TOP(f->rightuppercorner);
   OUTPUT_MIDDLE(f->rightcross);
   OUTPUT_BOTTOM(f->rightdowncorner);

   OUTPUT_HEADER(f->headercellend);
   OUTPUT_HEADER(f->headerrowend);

   OUTPUT(topseparator);
   OUTPUT(header);
   OUTPUT(middleseparator);
   while(row=mysql_fetch_row(result)) {
      mysql_field_seek(result,0);
           
      OUTPUT(f->contentrowstart);
      for(i=0 ; i < mysql_field_count(sock); ++i) {
         field = mysql_fetch_field(result);
         OUTPUT(f->contentcellstart);
         sprintf(buffptr,"%-*s",field->max_length,row[i]);
         buffptr+=field->max_length;

         if(i==mysql_field_count(sock))
         {
            OUTPUT(f->contentcellend);
         } else {
            OUTPUT(f->contentcellseparator);
         }
      }
      OUTPUT(f->contentrowend);
   }
   OUTPUT(bottomseparator);
   OUTPUT(f->tableend);

   mysql_free_result(result);
   DBUG_VOID_RETURN;
}


int db_function(char *b,const char *server, const char *database,const char *table,const char *field, const char *value, const char* path, struct func_st *function)
{
   char buff[BUFLEN];
   int i;
   DBUG_ENTER("db_function");
   DBUG_PRINT("enter",("buffer: '%s', database: '%s', table: '%s', field: '%s', path: '%s'", b, database, table, field,path));

   if(*database) {
     if (mysql_select_db(sock,database))
     {
       printf("error when changing database to'%s'\n",database);
       DBUG_RETURN(-1);
     }
   }

   sprintf(buff,"%s",function->function);
   search_and_replace("$database",database,buff);
   search_and_replace("$table",table,buff);
   search_and_replace("$field",field,buff);
   search_and_replace("$value",value,buff);
   DBUG_PRINT("info",("path: '%s'",path));
   DBUG_PRINT("info",("sum: '%d'",(database[0] ? strlen(database)+1 : 0) +(table[0] ? strlen(table)+1 : 0) +(field[0] ? strlen(field)+1 : 0) +(value[0] ? strlen(value)+1 : 0) +1));
   search_and_replace("$*",path+
                   (server[0] ? strlen(server)+1 : 0) +
                   (database[0] ? strlen(database)+1 : 0) +
                   (table[0] ? strlen(table)+1 : 0) +
                   (field[0] ? strlen(field)+1 : 0) +
                   (value[0] ? strlen(value)+1 : 0) +
                   function->length +
                   1,buff);
   DBUG_PRINT("info",("Executing constructed function query: '%s'", buff));
   if (mysql_query(sock, buff))
   {
      printf("error when executing '%s'\n",buff);
      sprintf(b,"ERROR %d: %s",mysql_error(sock),mysql_error(sock));
      DBUG_VOID_RETURN;
   }

   db_show_result(sock, b, &Human);
   DBUG_PRINT("info",("Returning: %s", b));
   DBUG_RETURN(1);
}

int db_show_field(char *b,const char *database,const char *table, const char *field,const char *value, const char *param)
{
   MYSQL_RES  *result;
   MYSQL_ROW  row;
   char buff[BUFLEN];
   int i=0;
   my_string *ptr;
   DBUG_ENTER("db_show_field");
   DBUG_PRINT("enter",("buffer: '%s', database: '%s', table: '%s', field: '%s' value: '%s'", b, database, table, field, value));

   /* We cant output fields when one of these variables is missing */
   if (!(database[0] && table[0] && field[0]))
      DBUG_RETURN(-1);
   
   init_dynamic_array(&field_array, sizeof(buff), 4096, 1024);

   if (mysql_select_db(sock,database))
   {
     printf("error when changing database to'%s'\n",database);
     delete_dynamic(&field_array);
     DBUG_RETURN(-1);
   }

   if(param) {
      sprintf(buff,"%s",param);
   } else {
      sprintf(buff,"select %s from %s where %s='%s' LIMIT 1",field,table,field,value);
   }
   if (mysql_query(sock, buff))
   {
     printf("error when executing '%s'\n",buff);
     delete_dynamic(&field_array);
     DBUG_RETURN(-1);
   }


   db_show_result(sock,b,&Human);
/*   if(result=mysql_use_result(sock)) {
     while(row=mysql_fetch_row(result))
     {
       strcpy(&b[i][BUFLEN],row[0]);
       DBUG_PRINT("info",("field %s at %x", &b[i*BUFLEN],&b[i*BUFLEN]));
//       ptr = (*dynamic_element(&field_array,i,row[0]));
       i++;
     }
   }
//   fix_filenames((char *)b);
   mysql_free_result(result);
   */
   delete_dynamic(&field_array);
   DBUG_RETURN(i);

}
int db_show_fields(char *b,const char *database,const char *table)
{
   MYSQL_RES  *result;
   MYSQL_ROW  row;
   MYSQL_FIELD *field;
   char buff[BUFLEN];
   int i=0;

   DBUG_ENTER("show_fields");
   DBUG_PRINT("enter",("buffer: '%s', database: '%s', table: '%s'", b, database, table));
   if (mysql_select_db(sock,database))
   {
     printf("error when changing database to'%s'\n",database);
     DBUG_RETURN(-1);
   }
   if(result=mysql_list_fields(sock,buff,NULL)) {

   while(row=mysql_fetch_row(result))
   {
     strcpy(&b[i*BUFLEN],row[0]);
     DBUG_PRINT("info",("field %s at %x", &b[i*BUFLEN],&b[i*BUFLEN]));
     i++;
   }
   }
   mysql_free_result(result);
   DBUG_RETURN(i);
}

int db_show_primary_keys(char *b,const char *database, const char *table)
{
   MYSQL_RES  *result;
   MYSQL_ROW  row;
   char buff[BUFLEN];
   char buff2[BUFLEN];
   unsigned int i;

   DBUG_ENTER("db_show_primary_keys");
   DBUG_PRINT("enter",("buffer: '%s', database: '%s', table: '%s'", b, database, table));
   if (mysql_select_db(sock,database))
   {
     printf("error when changing database to '%s'\n",database);
     DBUG_RETURN(-1);
   }
   sprintf(buff,"show keys from %s",table);
   if (mysql_query(sock, buff))
   {
     printf("error when executing '%s'\n",buff);
     DBUG_RETURN(0);
   }
   buff2[0]='\0';
   if(result=mysql_use_result(sock)) {
     while(row=mysql_fetch_row(result)) {
       if(!strcasecmp(row[2],"PRIMARY")) {
         strcat(buff2,row[4]);
         strcat(buff2,",\"_\",");
       }
     }
     buff2[strlen(buff2)-5]='\0';
     if(!buff2[0])
       DBUG_RETURN(-1); // No PRIMARY keys in table
     DBUG_PRINT("info",("Keys: %s<- \n", buff2));
   } else
     DBUG_RETURN(-1); // No keys in table

   sprintf(buff,"SELECT CONCAT(%s) AS X FROM %s LIMIT 256",buff2,table);
   if (mysql_query(sock, buff))
   {
     printf("error when executing '%s'\n",buff);
     DBUG_RETURN(0);
   }
   i=0;
   if(result=mysql_use_result(sock)) {
     while(row=mysql_fetch_row(result))
     {
       strcpy(&b[i*BUFLEN],row[0]);
       fix_filenames(&b[i*BUFLEN]);
       DBUG_PRINT("info",("primarykey %s at %x, %i", &b[i*BUFLEN],&b[i*BUFLEN],i));
       if(i++ >= MAXDIRS)
         break;
     }
   }
   mysql_free_result(result);
   DBUG_RETURN(i);
}


int db_show_keys(char *b,const char *database, const char *table)
{
   MYSQL_RES  *result;
   MYSQL_ROW  row;
   char buff[BUFLEN];
   int i=0;

   DBUG_ENTER("show_keys");
   DBUG_PRINT("enter",("buffer: '%s', database: '%s', table: '%s'", b, database, table));
   if (mysql_select_db(sock,database))
   {
     printf("error when changing database to'%s'\n",database);
     DBUG_RETURN(-1);
   }
   sprintf(buff,"show keys from %s",table);
   if (mysql_query(sock, buff))
   {
     printf("error when executing '%s'\n",buff);
     DBUG_RETURN(0);
   }
   if(result=mysql_use_result(sock)) {
     while(row=mysql_fetch_row(result))
     {
       strcpy(&b[i*BUFLEN],row[0]);
       DBUG_PRINT("info",("Key %s at %x", &b[i*BUFLEN],&b[i*BUFLEN]));
       i++;
     }
   }
   mysql_free_result(result);
   DBUG_RETURN(i);
}


int db_show_tables(char *b,const char *database)
{
   MYSQL_RES  *result;
   MYSQL_ROW  row;
   char buff[BUFLEN];
   int i=0;

   DBUG_ENTER("db_show_tables");
   DBUG_PRINT("enter",("buffer: '%s', database: '%s'", b, database));
   if (mysql_select_db(sock,database))
   {
     printf("error when changing database to '%s'\n",database);
     DBUG_RETURN(-1);
   }

   if(result=mysql_list_tables(sock,NULL)) {
     while(row=mysql_fetch_row(result))
     {
       strcpy(&b[i*BUFLEN],row[0]);
       DBUG_PRINT("info",("table %s at %x", &b[i*BUFLEN],&b[i*BUFLEN]));
       i++;
     }
   }
   mysql_free_result(result);
   DBUG_RETURN(i);
}

/*
 * Finds all servers we are connected to
 * and stores them in array supplied.
 * returns count of servers
 */
int 
db_show_servers(char *b,int size)
{
   char* bufptr;
   char* buff[BUFLEN*2];
   DBUG_ENTER("db_show_servers");
   DBUG_PRINT("enter",("buffer: '%s', size: '%d'", b, size));
   bufptr=mysql_get_host_info(sock);
   // FIXME: Actually we need to escape prohibited symbols in filenames
   fix_filenames(bufptr);
   strcpy(b,bufptr);
   DBUG_RETURN(1);
}

/*
 * Finds all databases in server
 * and stores them in array supplied.
 * returns count of databases
 */
int 
db_show_databases(char *b,int size)
{
   MYSQL_RES  *result;
   MYSQL_ROW  row;
   char buff[BUFLEN];
   int i=0;
    
   DBUG_ENTER("db_show_databases");
   DBUG_PRINT("enter",("buffer: '%s', size: '%d'", b, size));
   result=mysql_list_dbs(sock,NULL);
   while(row=mysql_fetch_row(result))
   {
     strcpy(&b[i*BUFLEN],row[0]);
     DBUG_PRINT("info",("database %s at %x", &b[i*BUFLEN],&b[i*BUFLEN]));
     i++;
   }
   mysql_free_result(result);
   DBUG_RETURN(i);
}

void db_load_formats()
{

   /* In future we should read these variables 
    * from configuration file/database here */

   /* HTML output */
   HTML.tablestart="<table>\n";     
      
   HTML.headerrowstart="<tr>";       
   HTML.headercellstart="<th>";
   HTML.headercellseparator="</th><th>";
   HTML.headercellend="</th>";
   HTML.headerrowend="</tr>\n";
   HTML.headerformat=0;

   HTML.leftuppercorner="";
   HTML.rightuppercorner="";
   HTML.leftdowncorner="";
   HTML.rightdowncorner="";
   HTML.topcross="";
   HTML.middlecross="";
   HTML.bottomcross="";
   HTML.leftcross="";
   HTML.rightcross="";
   HTML.bottomcross="";
  
   HTML.contentrowstart="<tr>";
   HTML.contentcellstart="<td>";
   HTML.contentcellseparator="</td><td>";
   HTML.contentcellend="</td>";
   HTML.contentrowend="</tr>\n";
   HTML.headerformat=0;

   HTML.footerrowstart="";
   HTML.footercellstart="";
   HTML.footercellseparator="";
   HTML.footercellend="";
   HTML.footerrowend="\n";     
   HTML.footerformat=0;        

   HTML.tableend="</table>\n";

/* Nice to look mysql client like output */
   
   Human.tablestart="\n";
      
   Human.headerrowstart="| ";       
   Human.headercellstart="";
   Human.headercellseparator=" | ";
   Human.headercellend=" |";
   Human.headerrowend="\n";
   Human.headerformat=1;
      
   Human.leftuppercorner="/=";
   Human.rightuppercorner="=\\\n";
   Human.leftdowncorner="\\=";
   Human.rightdowncorner="=/\n";
   Human.leftcross="+-";
   Human.rightcross="-+\n";
   Human.topcross="=T=";
   Human.middlecross="-+-";
   Human.bottomcross="=`=";

   Human.contentrowstart="| ";
   Human.contentcellstart="";
   Human.contentcellseparator=" | ";
   Human.contentcellend=" |";
   Human.contentrowend="\n";
   Human.contentformat=1;

   Human.footerrowstart="";
   Human.footercellstart="";
   Human.footercellseparator="";
   Human.footercellend="";
   Human.footerrowend="\n";     
   Human.footerformat=1;        

   Human.tableend="\n";

/* Comma-separated format. For machine reading */
  
   /* XML */
   
/*
  tee_fprintf(PAGER,"<?xml version=\"1.0\"?>\n\n<resultset statement=\"%s\">", statement);
    (void) tee_fputs("\n  <row>\n", PAGER);
      data=(char*) my_malloc(lengths[i]*5+1, MYF(MY_WME));
      tee_fprintf(PAGER, "\t<%s>", (fields[i].name ?
				  (fields[i].name[0] ? fields[i].name :
				   " &nbsp; ") : "NULL"));
      xmlencode(data, cur[i]);
      tee_fprintf(PAGER, "</%s>\n", (fields[i].name ?
				     (fields[i].name[0] ? fields[i].name :
				      " &nbsp; ") : "NULL"));
  </row>\n"  </resultset>\n*/
}

gptr db_load_functions()
{
   char *functions[]={
      "database",".tables","SHOW TABLES","0",
      "table",".status","SHOW TABLE STATUS FROM $table","0",
      "table",".count","SELECT COUNT(*) FROM $table","0",
      "table",".table","SELECT * FROM $table","0",
      "table",".check","CHECK TABLE $table","0",
      "table",".repair","REPAIR TABLE $table","0",
      "key",".min","SELECT MIN($key) FROM $table","0",
      "key",".max","SELECT MAX($key) FROM $table","0",
      "key",".avg","SELECT AVG($key) FROM $table","0",
      "server",".uptime","SHOW STATUS like 'Uptime'","0",
      "server",".version","SELECT VERSION()","0",
      "server",".execute","$*","1",
      "root",".connect","CONNECT $*","0",
      NULL,NULL,NULL,NULL
   };
   char buff[BUFLEN];
   int i=0;
   struct func_st func;
   DBUG_ENTER("db_load_functions");
   init_dynamic_array(&functions_array, sizeof(struct func_st), 4096, 1024);
   while(functions[i]) {
      strcpy(func.type_s,   functions[i]);   /* Type in string: "table"`               */
      strcpy(func.filename, functions[i+1]); /* Name like it appears on FS: "count"    */
      strcpy(func.function, functions[i+2]); /* Query: "SELECT COUNT(*) FROM `%table`" */
      func.continuous= atoi(functions[i+3]); /* Query: "If command can be continued"   */

      if(!strcasecmp(func.type_s,"server")) 
         func.type=SERVER_FUNCTION;
      else if(!strcasecmp(func.type_s,"table")) 
         func.type=TABLE_FUNCTION;
      else if(!strcasecmp(func.type_s,"key")) 
         func.type=KEY_FUNCTION;
      else if(!strcasecmp(func.type_s,"database")) 
         func.type=DATABASE_FUNCTION;
      else if(!strcasecmp(func.type_s,"field")) 
         func.type=FIELD_FUNCTION;
      else if(!strcasecmp(func.type_s,"root")) 
         func.type=ROOT_FUNCTION;
      else func.type=NONE_FUNCTION;

      func.length=strlen(func.filename);      /* Filename length */
      DBUG_PRINT("info",("func.type_s: %s",func.type_s));
      DBUG_PRINT("info",("func.filename: %s",func.filename));
      DBUG_PRINT("info",("func.function: %s",func.function));
      DBUG_PRINT("info",("func.type: %d",func.type));
      DBUG_PRINT("info",("func.continuous: %d",func.continuous));
      DBUG_PRINT("info",("i: %d",i));
      insert_dynamic(&functions_array,(gptr)&func);
      i+=4;
   }
   DBUG_RETURN((gptr)&functions_array);
}

