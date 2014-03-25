#!/bin/sh
# Copyright (C) 1979-2007 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING. If not, write to the
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston
# MA  02110-1301  USA.

@bindir@/replace msqlConnect mysql_connect msqlListDBs  mysql_list_dbs msqlNumRows mysql_num_rows msqlFetchRow mysql_fetch_row msqlFetchField mysql_fetch_field msqlFreeResult mysql_free_result msqlListFields mysql_list_fields msqlListTables mysql_list_tables msqlErrMsg 'mysql_error(mysql)' msqlStoreResult mysql_store_result msqlQuery mysql_query msqlField mysql_field msqlSelect mysql_select msqlSelectDB mysql_select_db msqlNumFields mysql_num_fields msqlClose mysql_close msqlDataSeek mysql_data_seek m_field MYSQL_FIELD m_result MYSQL_RES m_row MYSQL_ROW msql mysql mSQL mySQL MSQL MYSQL msqlCreateDB mysql_create_db msqlDropDB mysql_drop_db msqlFieldSeek mysql_field_seek -- $*
