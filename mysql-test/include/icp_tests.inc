--echo #
--echo # Bug#42580 - Innodb's ORDER BY ..LIMIT returns no rows for
--echo #             null-safe operator <=> NULL
--echo #

CREATE TABLE t1(
  c1 DATE NOT NULL,
  c2 DATE NULL,
  c3 DATETIME,
  c4 TIMESTAMP,
  PRIMARY KEY(c1),
  UNIQUE(c2)
);

--echo
INSERT INTO t1 VALUES('0000-00-00', '0000-00-00', '2008-01-04', '2008-01-05');
INSERT INTO t1 VALUES('2007-05-25', '2007-05-25', '2007-05-26', '2007-05-26');
INSERT INTO t1 VALUES('2008-01-01', NULL        , '2008-01-02', '2008-01-03');
INSERT INTO t1 VALUES('2008-01-17', NULL        , NULL        , '2009-01-29');
INSERT INTO t1 VALUES('2009-01-29', '2009-01-29', '2009-01-29', '2009-01-29');

--echo
SELECT * FROM t1 WHERE c2 <=> NULL ORDER BY c1,c2;
--echo
SELECT * FROM t1 WHERE c2 <=> NULL ORDER BY c1,c2 LIMIT 2;

--echo
DROP TABLE t1;

