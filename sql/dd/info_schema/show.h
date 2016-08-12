/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_DD_SHOW_H
#define SQL_DD_SHOW_H

#include "parse_tree_node_base.h"  // POS

class Item;
class String;
class THD;
class SELECT_LEX;
class Table_ident;

namespace dd {
  namespace info_schema {


/**
  Build a substitue query for SHOW CHARSETS.

  For command like,
  <code>
    SHOW CHARACTER SET [ LIKE 'pattern' | WHERE expr ]
  </code>

  We build following,
  <code>
    SELECT * FROM
             (SELECT CHARACTER_SET_NAME as `Charset`,
                     DESCRIPTION as `Description`,
                     DEFAULT_COLLATE_NAME as `Default collation`,
                     MAXLEN as `Maxlen`
              FROM information_schema.character_sets) character_sets
      [ WHERE Charset LIKE "<value>" | WHERE @<where_clause@> ]
      ORDER BY `Charset`;
  </code>

  @param pos  - YYLTYPE position of parsing context.
  @param thd  - Current thread.
  @param wild - The value of LIKE clause.
  @param where_cond - @<where_clause>@ clause provided by user.

  @returns pointer to SELECT_LEX on success, NULL otherwise.
*/
SELECT_LEX*
build_show_character_set_query(const POS &pos, THD *thd,
                               const String *wild, Item *where_cond);

/**
  Build a substitue query for SHOW COLLATION.

  For command like,
  <code>
    SHOW COLLATION [ LIKE 'pattern' | WHERE expr ]
  </code>

  We build following,
  <code>
    SELECT * FROM
             (SELECT COLLATION_NAME as `Collation`,
                     CHARACTER_SET_NAME as `Charset`,
                     ID as `Id`,
                     IS_COMPILED as `Compiled`,
                     SORTLEN as `Sortlen`
              FROM information_schema.collations) collations
      [ WHERE Collation LIKE "<value>" | WHERE @<where_clause@> ]
      ORDER BY `Collation`;
  </code>

  @param pos  - YYLTYPE position of parsing context.
  @param thd  - Current thread.
  @param wild - The value of LIKE clause.
  @param where_cond - @<where_clause>@ clause provided by user.

  @returns pointer to SELECT_LEX on success, NULL otherwise.
*/
SELECT_LEX*
build_show_collation_query(const POS &pos, THD *thd,
                           const String *wild, Item *where_cond);

/**
  Build a substitue query for SHOW DATABASES.

  For command like,
  <code>
    SHOW DATABASES [ LIKE 'pattern' | WHERE expr ]
  </code>

  We build following,
  <code>
    SELECT Database FROM
             (SELECT SCHEMA_NAME as `Database`,
              FROM information_schema.schemata) schemata
      [ WHERE Database LIKE "<value>" | WHERE @<where_clause@> ]
      ORDER BY `Database`;
  </code>

  @param pos  - YYLTYPE position of parsing context.
  @param thd  - Current thread.
  @param wild - The value of LIKE clause.
  @param where_cond - @<where_clause>@ clause provided by user.

  @returns pointer to SELECT_LEX on success, NULL otherwise.
*/
SELECT_LEX*
build_show_databases_query(const POS &pos,
                           THD *thd,
                           String *wild,
                           Item *where_cond);

/**
  Build a substitue query for SHOW TABLES / TABLE STATUS.

  For command like,
  <code>
    SHOW [FULL] TABLES [{FROM | IN} db_name]
                       [LIKE 'pattern' | WHERE expr]
             OR
    SHOW TABLE STATUS [{FROM | IN} db_name]
                      [LIKE 'pattern' | WHERE expr]
  </code>

  We build following,
  <code>
    SELECT `Table`,
           `Table_type`, <-- only with 'FULL'

           // For SHOW TABLE STATUS
           `Engine`,
           `Version`,
           `Row_format`,
           `Rows`,
           `Avg_row_length`,
           `Data_length`,
           `Max_data_length`,
           `Index_length`,
           `Data_free`,
           `Auto_increment`,
           `Create_time`,
           `Update_time`,
           `Check_time`,
           `Collation`,
           `Checksum`,
           `Create_options`,
           `Comment`
    FROM
      (SELECT TABLE_SCHEMA AS `Database`,
              TABLE_NAME as `Table`,
              TABLE_TYPE AS `Table_type`, <-- only with 'FULL'

              // For SHOW TABLE STATUS
              ENGINE AS `Engine`,
              VERSION AS `Version`,
              ROW_FORMAT AS `Row_format`,
              TABLE_ROWS AS `Rows`,
              AVG_ROW_LENGTH AS `Avg_row_length`,
              DATA_LENGTH AS `Data_length`,
              MAX_DATA_LENGTH AS `Max_data_length`,
              INDEX_LENGTH AS `Index_length`,
              DATA_FREE AS `Data_free`,
              AUTO_INCREMENT AS `Auto_increment`,
              CREATE_TIME AS `Create_time`,
              UPDATE_TIME AS `Update_time`,
              CHECK_TIME AS `Check_time`,
              TABLE_COLLATION AS `Collation`,
              CHECKSUM AS `Checksum`,
              CREATE_OPTIONS AS `Create_options`,
              TABLE_COMMENT AS `Comment`
         FROM information_schema.tables) tables
    WHERE Database == '<value>'           <-- Default DB or IN clause
          AND
          [ Table LIKE "<value>" | @<where_clause@> ]
    ORDER BY `Table`;
  </code>

  Note that the thd->lex->verbose == true would mean user has
  provide keyword 'FULL'.

  @param pos  - YYLTYPE position of parsing context.
  @param thd  - Current thread.
  @param wild - The value of LIKE clause.
  @param where_cond - @<where_clause>@ clause provided by user.
  @param include_status_fields - If we are handling SHOW TABLE STATUS

  @returns pointer to SELECT_LEX on success, NULL otherwise.
*/
SELECT_LEX*
build_show_tables_query(const POS &pos,
                        THD *thd,
                        String *wild,
                        Item *where_cond,
                        bool include_status_fields);

/**
  Build a substitue query for SHOW COLUMNS/FIELDS OR DESCRIBE.

  For command like,
  <code>
    SHOW [FULL] COLUMNS
        {FROM | IN} tbl_name
           [{FROM | IN} db_name]
               [LIKE 'pattern' | WHERE expr]
       OR
    DESCRIBE tbl_name
  </code>

  We build following,
  <code>
    SELECT Field,
           Type,
           Collation,  <-- only with 'FULL'
           Null,
           Key,
           Default,
           Extra,
           Privileges, <-- only with 'FULL'
           Comment     <-- only with 'FULL'
    FROM
      (SELECT TABLE_SCHEMA AS Database,
              TABLE_NAME AS Table,
              COLUMN_NAME AS Field,
              COLUMN_TYPE AS Type,
              COLLATION_NAME AS Collation,  <-- only with 'FULL'
              IS_NULLABLE AS Null,
              COLUMN_KEY AS Key,
              COLUMN_DEFAULT AS Default,
              EXTRA AS Extra,
              PRIVILEGES AS  Privileges, <-- only with 'FULL'
              COLUMN_COMMENT AS Comment, <-- only with 'FULL'
              ORDINAL_POSITION AS Oridinal_position
         FROM information_schema.columns) columns
    WHERE Database == '<value>'     <-- Default DB or db_name
          AND
          Table == 'value'          <-- tbl_name
          AND
          [ Field LIKE "<value>" | @<where_clause@> ]
    ORDER BY `Ordinal_position`;
  </code>

  Note that the thd->lex->verbose == true would mean user has
  provide keyword 'FULL'.

  @param pos  - YYLTYPE position of parsing context.
  @param thd  - Current thread.
  @param table_ident  - Database and Table name of table being used.
  @param wild - The value of LIKE clause.
  @param where_cond - @<where_clause>@ clause provided by user.

  @returns pointer to SELECT_LEX on success, NULL otherwise.
*/
SELECT_LEX*
build_show_columns_query(const POS &pos,
                         THD *thd,
                         Table_ident *table_ident,
                         const String *wild,
                         Item *where_cond);

/**
  Build a substitue query for SHOW INDEX|KEYS|INDEXES

  For command like,
  <code>
    SHOW {INDEX | INDEXES | KEYS}
        {FROM | IN} tbl_name
            [{FROM | IN} db_name]
                [WHERE expr]
  </code>

  We build following,
  <code>
    SELECT Table,
           Non_unique,
           Key_name,
           Seq_in_index,
           Column_name,
           Collation,
           Cardinality,
           Sub_part,
           Packed,
           Null,
           Index_type,
           Comment,
           Index_comment,
           Visible
     FROM
      (SELECT Database,
              Table,
              Non_unique,
              Key_name,
              Seq_in_index,
              Column_name,
              Collation,
              Cardinality,
              Sub_part,
              Packed,
              Null,
              Index_type,
              Comment,
              Index_comment,
              Visible,
              INDEX_ORDINAL_POSITION,
              COLUMN_ORDINAL_POSITION
         FROM information_schema.show_statistics) statistics
    WHERE Database == '<value>'     <-- Default DB or db_name
          AND
          Table == 'value'          <-- tbl_name
          AND
          [ @<where_clause@> ]
    ORDER BY INDEX_ORDINAL_POSITION, COLUMN_ORDINAL_POSITION
  </code>


  @param pos  - YYLTYPE position of parsing context.
  @param thd  - Current thread.
  @param table_ident  - Database and Table name of table being used.
  @param where_cond - @<where_clause>@ clause provided by user.

  @returns pointer to SELECT_LEX on success, NULL otherwise.
*/
SELECT_LEX*
build_show_keys_query(const POS &pos,
                      THD *thd,
                      Table_ident *table_ident,
                      Item *where_cond);

} // namespace info_schema
} // namespace dd

#endif /* SQL_DD_SHOW_H */
