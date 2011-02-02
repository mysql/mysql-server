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

DROP TABLE B1;
DROP TABLE B0;
DROP TABLE A;

CREATE TABLE A (
        id          INT                 NOT NULL,
        cint        INT                 NULL,
        clong       DECIMAL(20)         NULL,
        cfloat      FLOAT               NULL,
        cdouble     DOUBLE PRECISION    NULL,
        cstring     VARCHAR(30)         NULL,
        CONSTRAINT PK_A_0 PRIMARY KEY (id)
);

CREATE TABLE B0 (
        id          INT                 NOT NULL,
        cint        INT                 NULL,
        clong       DECIMAL(20)         NULL,
        cfloat      FLOAT               NULL,
        cdouble     DOUBLE PRECISION    NULL,
        cstring     VARCHAR(30)         NULL,
        a_id        INT                 NULL,
        CONSTRAINT PK_B0_0 PRIMARY KEY (id),
        CONSTRAINT FK_B0_1 FOREIGN KEY (a_id) REFERENCES A (id)
);

CREATE TABLE B1 (
        id int NOT NULL,
        CONSTRAINT PK_B1_0 PRIMARY KEY (id)
);

CREATE INDEX I_B0_FK ON B0 (
	a_id
);

COMMIT WORK;

EXIT;
