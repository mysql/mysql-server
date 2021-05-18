# Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET default_storage_engine=ndbcluster;

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

CREATE TABLE IF NOT EXISTS b (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        a_id            INT,
        cvarbinary_def  VARBINARY(202),
        cvarchar_def    VARCHAR(202),
        CONSTRAINT PK_B_0 PRIMARY KEY (id),
        CONSTRAINT FK_B_1 FOREIGN KEY (a_id) REFERENCES a (id)
);

DELETE FROM b;
DELETE FROM a;
