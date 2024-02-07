-- Copyright (c) 2010, 2024, Oracle and/or its affiliates.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License, version 2.0,
-- as published by the Free Software Foundation.
--
-- This program is designed to work with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have either included with
-- the program or referenced in the documentation.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License, version 2.0, for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
SET default_storage_engine=NDB;

DROP DATABASE IF EXISTS testdb;
CREATE DATABASE testdb;
-- USE testdb;

DROP TABLE IF EXISTS testdb.mytable;

CREATE  TABLE IF NOT EXISTS testdb.mytable (
        c0 VARCHAR(9)   NOT NULL,
        c1 VARCHAR(10)  NOT NULL,
        c2 INT          NOT NULL,
        c3 INT          NOT NULL,
        c4 INT          NULL,
        c5 VARCHAR(100)  NULL,
        c6 VARCHAR(120)  NULL,
        c7 VARCHAR(120)  NOT NULL,
        c8 VARCHAR(120)  NOT NULL,
        c9 CHAR         NULL,
        c10 CHAR        NULL,
        c11 VARCHAR(120)  NULL,
        c12 VARCHAR(120)  NULL,
        c13 CHAR        NULL,
        c14 VARCHAR(120) NULL,
        PRIMARY KEY (c0),
        UNIQUE INDEX c0_UNIQUE USING BTREE (c0 ASC)
-- "Job buffer congestion" crashes with node failure with unique indexes
-- http://bugs.mysql.com/bug.php?id=56552
--        UNIQUE INDEX c1_UNIQUE USING BTREE (c1 ASC),
--        UNIQUE INDEX c2_UNIQUE (c2 ASC),
--        UNIQUE INDEX c7_UNIQUE (c7 ASC),
--        UNIQUE INDEX c8_UNIQUE (c8 ASC)
) ENGINE=NDB;
