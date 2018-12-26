--echo #
--echo # Bug #24714857 FOUND_ROWS() RETURNS 1 WHEN NO ROWS FOUND
--echo #

CREATE TABLE tbl (
  col_text TEXT
);

SELECT SQL_CALC_FOUND_ROWS col_text FROM tbl AS tbl1
UNION ALL
SELECT col_text FROM tbl AS tbl2
ORDER BY col_text
LIMIT 0, 2;

SELECT FOUND_ROWS();

DROP TABLE tbl;
