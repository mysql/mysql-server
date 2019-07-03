-- Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

DROP FUNCTION IF EXISTS ps_is_instrument_default_timed;

DELIMITER $$

CREATE DEFINER='mysql.sys'@'localhost' FUNCTION ps_is_instrument_default_timed (
        in_instrument VARCHAR(128)
    ) 
    RETURNS ENUM('YES', 'NO')
    COMMENT '
Description
-----------

Returns whether an instrument is timed by default in this version of MySQL.

Parameters
-----------

in_instrument VARCHAR(128): 
  The instrument to check.

Returns
-----------

ENUM(\'YES\', \'NO\')

Example
-----------

mysql> SELECT sys.ps_is_instrument_default_timed(\'statement/sql/select\');
+------------------------------------------------------------+
| sys.ps_is_instrument_default_timed(\'statement/sql/select\') |
+------------------------------------------------------------+
| YES                                                        |
+------------------------------------------------------------+
1 row in set (0.00 sec)
'
    SQL SECURITY INVOKER
    DETERMINISTIC 
    READS SQL DATA 
BEGIN
    DECLARE v_timed ENUM('YES', 'NO');

    -- Currently the same in all versions
    SET v_timed = IF(in_instrument LIKE 'wait/io/file/%'
                        OR in_instrument LIKE 'wait/io/table/%'
                        OR in_instrument LIKE 'statement/%'
                        OR in_instrument IN ('wait/lock/table/sql/handler', 'idle')
               /*!50707
                        OR in_instrument LIKE 'stage/innodb/%'
                        OR in_instrument = 'stage/sql/copy to tmp table'
               */
                      ,
                       'YES',
                       'NO'
                    );

    RETURN v_timed;
END$$

DELIMITER ;
