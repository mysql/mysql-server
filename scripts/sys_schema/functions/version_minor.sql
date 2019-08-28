-- Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
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

DROP FUNCTION IF EXISTS version_minor;

DELIMITER $$

CREATE DEFINER='mysql.sys'@'localhost' FUNCTION version_minor ()
    RETURNS TINYINT UNSIGNED
    COMMENT '
Description
-----------

Returns the minor (release series) version of MySQL Server.

Returns
-----------

TINYINT UNSIGNED

Example
-----------

mysql> SELECT VERSION(), sys.server_minor();
+--------------------------------------+---------------------+
| VERSION()                            | sys.version_minor() |
+--------------------------------------+---------------------+
| 5.7.9-enterprise-commercial-advanced | 7                   |
+--------------------------------------+---------------------+
1 row in set (0.00 sec)
'
    SQL SECURITY INVOKER
    NOT DETERMINISTIC
    NO SQL
BEGIN
    RETURN SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING_INDEX(VERSION(), '-', 1), '.', 2), '.', -1);
END$$

DELIMITER ;
