/* Copyright (C) 2006-2008 MySQL AB

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

#include "../maria_def.h"

static LOG_DESC INIT_LOGREC_FIXED_RECORD_0LSN_EXAMPLE=
{LOGRECTYPE_FIXEDLENGTH, 6, 6, NULL, NULL, NULL, 0,
 "fixed0example", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 9, NULL, NULL, NULL, 0,
"variable0example", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_FIXED_RECORD_1LSN_EXAMPLE=
{LOGRECTYPE_PSEUDOFIXEDLENGTH, 7, 7, NULL, NULL, NULL, 1,
"fixed1example", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 12, NULL, NULL, NULL, 1,
"variable1example", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_FIXED_RECORD_2LSN_EXAMPLE=
{LOGRECTYPE_PSEUDOFIXEDLENGTH, 23, 23, NULL, NULL, NULL, 2,
"fixed2example", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 19, NULL, NULL, NULL, 2,
"variable2example", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};


void translog_example_table_init()
{
  int i;
  log_record_type_descriptor[LOGREC_FIXED_RECORD_0LSN_EXAMPLE]=
    INIT_LOGREC_FIXED_RECORD_0LSN_EXAMPLE;
  log_record_type_descriptor[LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE]=
    INIT_LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE;
  log_record_type_descriptor[LOGREC_FIXED_RECORD_1LSN_EXAMPLE]=
    INIT_LOGREC_FIXED_RECORD_1LSN_EXAMPLE;
  log_record_type_descriptor[LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE]=
    INIT_LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE;
  log_record_type_descriptor[LOGREC_FIXED_RECORD_2LSN_EXAMPLE]=
    INIT_LOGREC_FIXED_RECORD_2LSN_EXAMPLE;
  log_record_type_descriptor[LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE]=
    INIT_LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE;
  for (i= LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE + 1;
       i < LOGREC_NUMBER_OF_TYPES;
       i++)
    log_record_type_descriptor[i].rclass= LOGRECTYPE_NOT_ALLOWED;
}



