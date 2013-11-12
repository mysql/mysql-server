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

DROP DATABASE IF EXISTS crunddb;
CREATE DATABASE crunddb;
USE crunddb;

-- DROP TABLE IF EXISTS B;
-- DROP TABLE IF EXISTS A;

CREATE TABLE A (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        CONSTRAINT PK_A_0 PRIMARY KEY (id)
) ENGINE=NDB;

CREATE TABLE B (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        a_id            INT,
        -- XXX crund c++ code currently does not support VARBINARY/CHAR > 255
        cvarbinary_def  VARBINARY(202),
        -- default charset: latin1 (~ISO 8859-1)
        cvarchar_def    VARCHAR(202),
        -- cvarchar_ascii  VARCHAR(202) CHARACTER SET ASCII,
        -- cvarchar_ucs2   VARCHAR(202) CHARACTER SET UCS2,
        -- cvarchar_utf8   VARCHAR(202) CHARACTER SET UTF8,
        cblob_def       BLOB(1000004),
        ctext_def       TEXT(1000004),
        -- ctext_utf8      TEXT(202) CHARACTER SET UTF8,
        CONSTRAINT PK_B_0 PRIMARY KEY (id),
        CONSTRAINT FK_B_1 FOREIGN KEY (a_id) REFERENCES a (id),
        INDEX I_B_FK (a_id)
) ENGINE=NDB;


-- DROP TABLE IF EXISTS S;

CREATE TABLE S (
        c0 VARCHAR(10)  NOT NULL,
        c1 VARCHAR(10)  NOT NULL,
        c2 INT          NOT NULL,
        c3 INT          NOT NULL,
        c4 INT          NULL,
        c5 VARCHAR(50)  NULL,
        c6 VARCHAR(50)  NULL,
        c7 VARCHAR(10)  NOT NULL,
        c8 VARCHAR(10)  NOT NULL,
        c9 CHAR         NULL,
        c10 CHAR        NULL,
        c11 VARCHAR(10) NULL,
        c12 VARCHAR(10) NULL,
        c13 CHAR        NULL,
        c14 VARCHAR(50) NULL,
        PRIMARY KEY (c0)
        -- not clear why these additional hash+ordered indexes:
        -- UNIQUE INDEX c0_UNIQUE USING BTREE (c0 ASC)
        -- @10k rows, bulk insert: "job buffer congestion" node failures
        -- with these additional hash+ordered indexes:
        -- UNIQUE INDEX c1_UNIQUE USING BTREE (c1 ASC),
        -- @1k rows, bulk insert: "job buffer congestion" node failures
        -- with these additional hash+ordered indexes:
        -- UNIQUE INDEX c2_UNIQUE (c2 ASC),
        -- UNIQUE INDEX c7_UNIQUE (c7 ASC),
        -- UNIQUE INDEX c8_UNIQUE (c8 ASC)
) ENGINE=NDB;

-- see details: $ ndb_desc -c localhost <table lower case> -d <database>
