SET storage_engine=NDB;

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
        c5 VARCHAR(39)  NULL,
        c6 VARCHAR(39)  NULL,
        c7 VARCHAR(9)   NOT NULL,
        c8 VARCHAR(11)  NOT NULL,
        c9 CHAR         NULL,
        c10 CHAR        NULL,
        c11 VARCHAR(7)  NULL,
        c12 VARCHAR(7)  NULL,
        c13 CHAR        NULL,
        c14 VARCHAR(34) NULL,
        PRIMARY KEY (c0),
        UNIQUE INDEX c0_UNIQUE USING BTREE (c0 ASC)
-- "Job buffer congestion" crashes with node failure with unique indexes
-- http://bugs.mysql.com/bug.php?id=56552
--        UNIQUE INDEX c1_UNIQUE USING BTREE (c1 ASC),
--        UNIQUE INDEX c2_UNIQUE (c2 ASC),
--        UNIQUE INDEX c7_UNIQUE (c7 ASC),
--        UNIQUE INDEX c8_UNIQUE (c8 ASC)
);
