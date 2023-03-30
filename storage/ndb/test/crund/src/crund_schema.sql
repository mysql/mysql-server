-- Copyright (c) 2010, 2023, Oracle and/or its affiliates.
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
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET default_storage_engine=NDB;

DROP DATABASE IF EXISTS crunddb;
CREATE DATABASE crunddb;
USE crunddb;

-- schema details: $ ndb_desc -c localhost <table lower case> -d <database>

CREATE TABLE a (
        id              INT     PRIMARY KEY,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE
);

CREATE TABLE b (
        id              INT     PRIMARY KEY,
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
        -- cvarchar_utf8   VARCHAR(202) CHARACTER SET UTF8MB3,
        cblob_def       BLOB(1000004),
        ctext_def       TEXT(1000004),
        -- ctext_utf8      TEXT(202) CHARACTER SET UTF8MB3,
        CONSTRAINT FK_B_1 FOREIGN KEY (a_id) REFERENCES a (id),
        INDEX I_B_FK (a_id)
);

CREATE TABLE s (
        c0      VARCHAR(10)     NOT NULL,
        c1      VARCHAR(10)     NOT NULL,
        c2      INT             NOT NULL,
        c3      INT             NOT NULL,
        c4      INT             NULL,
        c5      VARCHAR(50)     NULL,
        c6      VARCHAR(50)     NULL,
        c7      VARCHAR(10)     NOT NULL,
        c8      VARCHAR(10)     NOT NULL,
        c9      CHAR            NULL,
        c10     CHAR            NULL,
        c11     VARCHAR(10)     NULL,
        c12     VARCHAR(10)     NULL,
        c13     CHAR            NULL,
        c14     VARCHAR(50)     NULL,
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
);

-- ----------------------------------------------------------------------
-- Results Schema
-- ----------------------------------------------------------------------

-- aggregated results from crund runs
-- example queries:
-- load data infile '/Users/mz/mysql/crundcharting/log_results.csv' ignore into table results fields terminated by ',' ignore 1 lines;
CREATE TABLE results (
        metric          VARCHAR(16)     NOT NULL,
        cload           VARCHAR(16)     NOT NULL,
        nrows           INT             NOT NULL,
        nruns           INT             NOT NULL,
        op              VARCHAR(32)     NOT NULL,
        xmode           ENUM('indy','each','bulk')      NOT NULL,
        nrows_metric    DECIMAL(8,2)    NOT NULL,
        metric_nrows    DECIMAL(8,2)    NOT NULL,
        rsdev           DECIMAL(8,2)    NOT NULL,
        UNIQUE KEY (metric, cload, nrows, op, xmode)
) ENGINE=MEMORY;

-- base view with rtime as metric
CREATE VIEW results_rtime
       AS (SELECT cload, op, xmode, nrows, nrows_metric
           FROM results
           WHERE metric = 'rtime[ms]');

-- base views for fixed xMode values
CREATE VIEW results_rtime_indy AS
        (SELECT cload, op, nrows_metric AS 'indy', nrows
         FROM results_rtime
         WHERE xmode='indy');
CREATE VIEW results_rtime_each AS
        (SELECT cload, op, nrows_metric AS 'each', nrows
         FROM results_rtime
         WHERE xmode='each');
CREATE VIEW results_rtime_bulk AS
        (SELECT cload, op, nrows_metric AS 'bulk', nrows
         FROM results_rtime
         WHERE xmode='bulk');

-- joined view with xmode values in columns
-- using left outer joines (full outer joins not supported, emulateable by union of left+right)
-- i.e., ok for bulk-only results but have to deal with indy- and each-only results separately
-- example queries:
-- select * from results_rtime_xmode where op like '%getAttr%' order by nrows, cload, op;
-- select * from results_rtime_xmode where op like '%getAttr%' and nrows=1000 order by cload, op;
CREATE VIEW results_rtime_xmode AS
        (SELECT nrows,
                cload,
                op,
                IFNULL(i.indy,0) AS 'indy',
                IFNULL(e.each,0) AS 'each',
                IFNULL(b.bulk,0) AS 'bulk'
         FROM results_rtime_bulk AS b
                NATURAL LEFT OUTER JOIN results_rtime_each AS e
                NATURAL LEFT OUTER JOIN results_rtime_indy AS i
         ORDER BY nrows, cload, op);

-- base views for fixed nrows values (10^2, 10^3, 10^4)
CREATE VIEW results_rtime_nrows2 AS
        (SELECT cload, op, nrows_metric AS 'nrows2', xmode
         FROM results_rtime
         WHERE nrows=100);
CREATE VIEW results_rtime_nrows3 AS
        (SELECT cload, op, nrows_metric AS 'nrows3', xmode
         FROM results_rtime
         WHERE nrows=1000);
CREATE VIEW results_rtime_nrows4 AS
        (SELECT cload, op, nrows_metric AS 'nrows4', xmode
         FROM results_rtime
         WHERE nrows=10000);

-- joined view with nrows values in columns
-- using inner joines (full outer joins not supported, emulateable by union of left+right)
-- i.e., have to deal with r2/r3/r4-only results separately
-- example queries:
-- select * from results_rtime_nrows where op like '%getAttr%' order by xmode, cload, op;
-- select * from results_rtime_nrows where op like '%getAttr%' and xmode='bulk' order by cload, op;
-- select * from results_rtime_nrows where op like '%delAll%' and xmode='bulk' order by cload, op;
CREATE VIEW results_rtime_nrows AS
        (SELECT xmode,
                cload,
                op,
                IFNULL(r2.nrows2,0) AS 'nrows2',
                IFNULL(r3.nrows3,0) AS 'nrows3',
                IFNULL(r4.nrows4,0) AS 'nrows4'
         FROM results_rtime_nrows2 AS r2
                NATURAL JOIN results_rtime_nrows3 AS r3
                NATURAL JOIN results_rtime_nrows4 AS r4
         ORDER BY xmode, cload, op);
