-- Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License, version 2.0,
-- as published by the Free Software Foundation.
--
-- This program is also distributed with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have included with MySQL.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License, version 2.0, for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

DROP FUNCTION IF EXISTS list_add;

DELIMITER $$

CREATE DEFINER='mysql.sys'@'localhost' FUNCTION list_add (
        in_list TEXT,
        in_add_value TEXT
    )
    RETURNS TEXT
    COMMENT '
Description
-----------

Takes a list, and a value to add to the list, and returns the resulting list.

Useful for altering certain session variables, like sql_mode or optimizer_switch for instance.

Parameters
-----------

in_list (TEXT):
  The comma separated list to add a value to

in_add_value (TEXT):
  The value to add to the input list

Returns
-----------

TEXT

Example
--------

mysql> select @@sql_mode;
+-----------------------------------------------------------------------------------+
| @@sql_mode                                                                        |
+-----------------------------------------------------------------------------------+
| ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_AUTO_CREATE_USER,NO_ENGINE_SUBSTITUTION |
+-----------------------------------------------------------------------------------+
1 row in set (0.00 sec)

mysql> set sql_mode = sys.list_add(@@sql_mode, ''ANSI_QUOTES'');
Query OK, 0 rows affected (0.06 sec)

mysql> select @@sql_mode;
+-----------------------------------------------------------------------------------------------+
| @@sql_mode                                                                                    |
+-----------------------------------------------------------------------------------------------+
| ANSI_QUOTES,ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_AUTO_CREATE_USER,NO_ENGINE_SUBSTITUTION |
+-----------------------------------------------------------------------------------------------+
1 row in set (0.00 sec)

'
    SQL SECURITY INVOKER
    DETERMINISTIC
    CONTAINS SQL
BEGIN

    IF (in_add_value IS NULL) THEN
        SIGNAL SQLSTATE '02200'
           SET MESSAGE_TEXT = 'Function sys.list_add: in_add_value input variable should not be NULL',
               MYSQL_ERRNO = 1138;
    END IF;

    IF (in_list IS NULL OR LENGTH(in_list) = 0) THEN
        -- return the new value as a single value list
        RETURN in_add_value;
    END IF;

    RETURN (SELECT CONCAT(TRIM(BOTH ',' FROM TRIM(in_list)), ',', in_add_value));

END$$

DELIMITER ;
