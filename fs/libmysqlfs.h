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
#include "CorbaFS.h"

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"

#define BUFLEN 1024
#define MAXDIRS 1024

typedef enum { 
        FUNC_NONE, 
        FUNC_SERVER_UPTIME, 
        FUNC_SERVER_THREADS, 
        FUNC_SERVER_VERSION,
        FUNC_DATABASE_CREATED,
        FUNC_TABLE_COUNT, 
        FUNC_TABLE_CREATED,
        FUNC_FIELD_LENGTH,
        FUNC_KEY_AVG, 
        FUNC_KEY_SUM, 
        FUNC_KEY_MAX, 
        FUNC_KEY_MIN
} func_enum;


typedef enum {
        NONE_FUNCTION,
        ROOT_FUNCTION,
        SERVER_FUNCTION, 
        DATABASE_FUNCTION, 
        TABLE_FUNCTION, 
        KEY_FUNCTION,
        FIELD_FUNCTION,
        VALUE_FUNCTION
} function_type;

struct func_st {
   char type_s[20];
   char filename[20];
   char function[80];
   function_type type;
   int length;
   my_bool continuous;
} ;


int parse(const char* path, 
                char* root,
                char* database, 
                char* table, 
                char* key, 
                char* field, 
                struct func_st **func
);

gptr db_load_functions();
int db_function(char *b,const char *server, const char *database,const char *table,const char *field, 
                const char *value, const char *path, struct func_st *function);
int fix_filenames(char *buf);

DYNAMIC_ARRAY functions_array;


