-- Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.
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
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET storage_engine=NDB;
-- SET storage_engine=INNODB;

DROP DATABASE IF EXISTS crunddb;
CREATE DATABASE crunddb;
USE crunddb;

-- DROP TABLE IF EXISTS b1;
-- DROP TABLE IF EXISTS b0;
-- DROP TABLE IF EXISTS a;

CREATE TABLE a (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        CONSTRAINT PK_A_0 PRIMARY KEY (id)
);

CREATE TABLE b0 (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        a_id            INT,
 	-- crund code currently does not support VARBINARY/CHAR > 202
        cvarbinary_def  VARBINARY(202),
        cvarchar_def    VARCHAR(202),
        cblob_def       BLOB(1000004),
        ctext_def       TEXT(1000004),
        CONSTRAINT PK_B0_0 PRIMARY KEY (id),
        CONSTRAINT FK_B0_1 FOREIGN KEY (a_id) REFERENCES a (id)
);
--        cvarchar_ascii  VARCHAR(202) CHARACTER SET ASCII,
--        ctext_ascii     TEXT(202) CHARACTER SET ASCII,
--        cvarchar_ucs2   VARCHAR(202) CHARACTER SET UCS2,
--        ctext_ucs2      TEXT(202) CHARACTER SET UCS2,
--        cvarchar_utf8   VARCHAR(202) CHARACTER SET UTF8,
--        ctext_utf8      TEXT(202) CHARACTER SET UTF8,

CREATE TABLE b1 (
        id              INT NOT NULL,
        CONSTRAINT PK_B1_0 PRIMARY KEY (id)
);

CREATE INDEX I_B0_FK ON b0 (
        a_id
);
