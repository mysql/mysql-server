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
#         cblob_def       BLOB(1000004),
#         ctext_def       TEXT(1000004),
        CONSTRAINT PK_B_0 PRIMARY KEY (id),
        CONSTRAINT FK_B_1 FOREIGN KEY (a_id) REFERENCES a (id)
);

DELETE FROM a;
DELETE FROM b;
