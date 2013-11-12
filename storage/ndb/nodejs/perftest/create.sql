# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET storage_engine=ndbcluster;

CREATE DATABASE IF NOT EXISTS jscrund;
USE jscrund;

CREATE TABLE IF NOT EXISTS a (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        CONSTRAINT PK_A_0 PRIMARY KEY (id)
);

CREATE TABLE IF NOT EXISTS b0 (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        a_id            INT,
        cvarbinary_def  VARBINARY(202),
        cvarchar_def    VARCHAR(202),
#         cblob_def       BLOB(1000004),
#         ctext_def       TEXT(1000004),
        CONSTRAINT PK_B0_0 PRIMARY KEY (id),
        CONSTRAINT FK_B0_1 FOREIGN KEY (a_id) REFERENCES a (id)
);


CREATE  TABLE IF NOT EXISTS tws (
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
        PRIMARY KEY (c0)
#        , UNIQUE INDEX c0_UNIQUE USING BTREE (c0 ASC)
);

DELETE FROM a;
DELETE FROM b0;
DELETE FROM tws;
