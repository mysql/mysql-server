-- Copyright 2010 Sun Microsystems, Inc.
--  All rights reserved. Use is subject to license terms.
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
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

CONNECT 'jdbc:derby:crunddb;create=true';

-- DROP TABLE IF EXISTS not supported yet by derby
DROP TABLE b1;
DROP TABLE b0;
DROP TABLE a;

CREATE TABLE a (
        id          INT                 NOT NULL,
        cint        INT,
        clong       DECIMAL(20),
        cfloat      FLOAT,
        cdouble     DOUBLE PRECISION,
        CONSTRAINT  PK_A_0 PRIMARY KEY (id)
);

CREATE TABLE b0 (
        id          INT                 NOT NULL,
        cint        INT,
        clong       DECIMAL(20),
        cfloat      FLOAT,
        cdouble     DOUBLE PRECISION,
        cvarbinary_def  BLOB(202),
        cvarchar_def    VARCHAR(202),
        cblob_def       BLOB(202),
        ctext_def       VARCHAR(202),
        a_id        INT,
        CONSTRAINT  PK_B0_0 PRIMARY KEY (id),
        CONSTRAINT  FK_B0_1 FOREIGN KEY (a_id) REFERENCES a (id)
);

CREATE TABLE b1 (
        id int NOT NULL,
        CONSTRAINT PK_B1_0 PRIMARY KEY (id)
);

-- derby already creates an index on B0.a_id

-- seems that these datatypes are not supported (parse error with ij):
--        cvarbinary_def  VARBINARY(202),
--        ctext_def       TEXT(202),

EXIT;
