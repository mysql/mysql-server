#!/bin/sh
# Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB
# 
# This software is distributed with NO WARRANTY OF ANY KIND.  No author or
# distributor accepts any responsibility for the consequences of using it, or
# for whether it serves any particular purpose or works at all, unless he or
# she says so in writing.  Refer to the Free Public License (the "License")
# for full details.
# 
# Every copy of this file must include a copy of the License, normally in a
# plain ASCII text file named PUBLIC.  The License grants you the right to 
# copy, modify and redistribute this file, but only under certain conditions
# described in the License.  Among other things, the License requires that
# the copyright notice and this notice be preserved on all copies.

@bindir@/replace msqlConnect mysql_connect msqlListDBs  mysql_list_dbs msqlNumRows mysql_num_rows msqlFetchRow mysql_fetch_row msqlFetchField mysql_fetch_field msqlFreeResult mysql_free_result msqlListFields mysql_list_fields msqlListTables mysql_list_tables msqlErrMsg 'mysql_error(mysql)' msqlStoreResult mysql_store_result msqlQuery mysql_query msqlField mysql_field msqlSelect mysql_select msqlSelectDB mysql_select_db msqlNumFields mysql_num_fields msqlClose mysql_close msqlDataSeek mysql_data_seek m_field MYSQL_FIELD m_result MYSQL_RES m_row MYSQL_ROW msql mysql mSQL mySQL MSQL MYSQL msqlCreateDB mysql_create_db msqlDropDB mysql_drop_db msqlFieldSeek mysql_field_seek -- $*
