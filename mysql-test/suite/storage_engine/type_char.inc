#
# CHAR column types
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $create_definition =
  c CHAR $col_opts,
  c0 CHAR(0) $col_opts,
  c1 CHAR(1) $col_opts,
  c20 CHAR(20) $col_opts,
  c255 CHAR(255) $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = CHAR types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  # Valid values 

  INSERT INTO t1 (c,c0,c1,c20,c255) VALUES ('','','','','');
  INSERT INTO t1 (c,c0,c1,c20,c255) VALUES ('a','','b','abcdefghi klmnopqrst', 'Creating an article for the Knowledgebase is similar to asking questions. First, navigate to the category where you feel the article should be. Once there, double check that an article doesn\'t already exist which would work.');

  --sorted_result
  SELECT c,c0,c1,c20,c255 FROM t1;

  # Invalid values

  INSERT INTO t1 (c,c0,c1,c20,c255) VALUES ('abc', 'a', 'abc', REPEAT('a',21), REPEAT('x',256));
  INSERT INTO t1 (c,c0,c1,c20,c255) SELECT c255, c255, c255, c255, CONCAT(c255,c1) FROM t1;

  --sorted_result
  SELECT c,c0,c1,c20,c255 FROM t1;

  --sorted_result
  SELECT DISTINCT c20, REPEAT('a',LENGTH(c20)), COUNT(*) FROM t1 GROUP BY c1, c20;

  --let $error_codes = ER_TOO_BIG_FIELDLENGTH
  --let $alter_definition = ADD COLUMN c257 CHAR(257) $col_opts
  --source alter_table.inc
  if ($mysql_errname != ER_TOO_BIG_FIELDLENGTH)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }
  DROP TABLE t1;
}


