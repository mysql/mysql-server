#
# BIT column type
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

# Valid values

let $create_definition =
  a BIT $col_opts,
  b BIT(20) $col_opts,
  c BIT(64) $col_opts,
  d BIT(1) $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = BIT types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  --let $alter_definition = DROP COLUMN d
  --source alter_table.inc
  if ($mysql_errname)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    --let $alter_definition = ADD COLUMN d BIT(0) $col_opts
    --source alter_table.inc

    --replace_column 3 # 4 # 5 #
    SHOW COLUMNS IN t1;
  }

  INSERT INTO t1 (a,b,c,d) VALUES (0,POW(2,20)-1,b'1111111111111111111111111111111111111111111111111111111111111111',1);
  --sorted_result
  SELECT BIN(a), HEX(b), c+0 FROM t1 WHERE d>0;

  INSERT INTO t1 (a,b,c,d) VALUES (1,0,-1,0);
  --sorted_result
  SELECT a+0, b+0, c+0 FROM t1 WHERE d<100;

  INSERT INTO t1 (a,b,c,d) VALUES (b'1', 'f', 0xFF, 0x0);
  --sorted_result
  SELECT a+0, b+0, c+0 FROM t1 WHERE d IN (0, 2);

  # Out of range values
  # (should produce warnings)

  INSERT INTO t1 (a,b,c,d) VALUES (0x10,0,0,1);
  --sorted_result
  SELECT a,b,c,d FROM t1;

  INSERT INTO t1 (a,b,c,d) VALUES (0x01,0,0x10000000000000000,0);
  --sorted_result
  SELECT a,b,c,d FROM t1;

  DROP TABLE t1;

  --let $error_codes = ER_TOO_BIG_DISPLAYWIDTH
  --let $create_definition = a BIT(65) $col_opts
  --source create_table.inc
}


