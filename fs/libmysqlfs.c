/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
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
#include "libmysqlfs.h"

int search_and_replace(char *search, char* replace, char* string)
{
   char buff[1024];
   int found=0;
   char *ptr1;
   const char *ptr2=buff;
   char *strptr=string;

   DBUG_ENTER("search_and_replace");
   DBUG_PRINT("enter",("search: '%s'  replace:'%s'  string:'%s'",search,replace,string));
   strcpy(buff,string);
   while(ptr1=strstr(ptr2,search))
   {
      strncpy(strptr,ptr2,ptr1-buff);
      strptr+=ptr1-buff;
      ptr2+=ptr1-buff+strlen(search);
      strcpy(strptr,replace);
      strptr+=strlen(replace);
      found++;
   }
   DBUG_RETURN(found);
}

int show_functions(char *b, function_type type)
{
   int i=0,j=0; 
   struct func_st func;
   DBUG_ENTER("show_functions");
   get_dynamic(&functions_array,(gptr)&func,i);
   while(func.length) {
      if (func.type == type) 
        strcpy(&b[j++*BUFLEN],func.filename);
      get_dynamic(&functions_array,(gptr)&func,++i);
   }
   DBUG_RETURN(j);
}

struct func_st * check_if_function(char *name, function_type type)
{
   int pathlen;
   int j,i=0, len;
   static struct func_st function;
   char buffer[BUFLEN];

   DBUG_ENTER("check_if_function");
   DBUG_PRINT("enter",("name: '%s' type: '%d'", name, type));
   pathlen=strlen(name);

   /* We try to compare last element in path to function names */
   get_dynamic(&functions_array,(gptr)&function,i);
   while(function.length) {
      function.continuous ? 
                      (j=!strncasecmp(function.filename, name, function.length))
                      : (j=!strcasecmp(function.filename,name));
      if(j) { /* This happens when function was matched */
        DBUG_PRINT("info",("Function %s detected!",function.filename));
        break;
      }
      get_dynamic(&functions_array,(gptr)&function,++i);
   }

   /* Copy path to buffer and trip function name (if found) from it */   
   if(function.length != 0)
   {
      DBUG_RETURN(&function);
   } else {
      DBUG_RETURN(0);
   }
}

/*
 * parse - splits "path" into different variables
 * in way "/server/database/table/(field|key)/(value|function)". If path is shorter, 
 * then other fields will be NULL. If path is longer than four levels or
 * shorter than one level, FS_NOTEXIST is returned.
 */
int parse(const char * path, char *server, char * database, char *table, 
                char* field, char* value, struct func_st **funce)
{
   char buffer[BUFLEN];
   char *p=buffer;
   char *x;
   int len;

   DBUG_ENTER("parse");
   DBUG_PRINT("enter",("path: '%s'", path));

   *server=*database=*table=*field=*value='\0';

   /* Search for first slash and drop it */
   strcpy(buffer,path);
   x=strtok_r(p,"/",&p);
   if(x)
   {
     strcpy(server,x); /* First argument is server name */
     if(*p)
        strcpy(database,strtok_r(p,"/",&p)); /* Second is database */
     if(p && *p) 
        strcpy(table   ,strtok_r(p,"/",&p)); /* Third is table name */
     if(p && *p) 
        strcpy(field   ,strtok_r(p,"/",&p)); /* Fourth is field or key name */
     if(p && *p) 
        strcpy(value   ,strtok_r(p,"/",&p)); /* Fifth is field/key value or function */
   }

   /* We have to find if last argument is function, 
    * In which case we clear it
    */
   if(*value) {
      *funce=check_if_function(value,VALUE_FUNCTION);
      if(*funce) *value='\0';
   } else if (*field) {
      *funce=check_if_function(field,FIELD_FUNCTION);
      if(*funce) *field='\0';
   } else if (*table) {
      *funce=check_if_function(table,TABLE_FUNCTION);
      if(*funce) *table='\0';
   } else if (*database) {
      *funce=check_if_function(database,DATABASE_FUNCTION);
      if(*funce) *database='\0';
   } else if (*server) {
      *funce=check_if_function(server,SERVER_FUNCTION);
      if(*funce) *server='\0';
   } else
      *funce=NULL;
   
   DBUG_PRINT("info",("path: '%s', server: '%s', db: '%s', table: '%s', field: '%s', value: '%s', function: '%x'",
                   buffer, server, database, table, field, value, funce )); 
   if(p && *p) /* Something is in buffer - too deep in levels */
     DBUG_RETURN(-1)
   else
     DBUG_RETURN(0)
}


