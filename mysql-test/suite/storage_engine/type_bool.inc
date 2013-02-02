#
# BOOLEAN column type
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $create_definition =
  b1 BOOL $col_opts,
  b2 BOOLEAN $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = BOOLEAN types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{

  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  # Valid values 

  INSERT INTO t1 (b1,b2) VALUES (1,TRUE);
  --sorted_result
  SELECT b1,b2 FROM t1;

  INSERT INTO t1 (b1,b2) VALUES (FALSE,0);
  --sorted_result
  SELECT b1,b2 FROM t1;

  INSERT INTO t1 (b1,b2) VALUES (2,3);
  --sorted_result
  SELECT b1,b2 FROM t1;

  INSERT INTO t1 (b1,b2) VALUES (-1,-2);
  --sorted_result
  SELECT b1,b2 FROM t1;

  --sorted_result
  SELECT IF(b1,'true','false') AS a, IF(b2,'true','false') AS b FROM t1;

  --sorted_result
  SELECT b1,b2 FROM t1 WHERE b1 = TRUE;

  --sorted_result
  SELECT b1,b2 FROM t1 WHERE b2 = FALSE;

  # Invalid values

  INSERT INTO t1 (b1,b2) VALUES ('a','b');
  --sorted_result
  SELECT b1,b2 FROM t1;

  INSERT INTO t1 (b1,b2) VALUES (128,-129);
  --sorted_result
  SELECT b1,b2 FROM t1;

  # This is why we don't have zerofill and unsigned tests
  # for boolean columns:
  --let $error_codes = ER_PARSE_ERROR
  --let $alter_definition = ADD COLUMN b3 BOOLEAN UNSIGNED
  --source alter_table.inc
  if ($mysql_errname != ER_PARSE_ERROR)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }
  --let $error_codes = ER_PARSE_ERROR
  --let $alter_definition = ADD COLUMN b3 BOOL ZEROFILL
  --source alter_table.inc

  DROP TABLE t1;
}


