#
# BLOB column types
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $create_definition =
  b BLOB $col_opts,
  b0 BLOB(0) $col_opts,
  b1 BLOB(1) $col_opts,
  b300 BLOB(300) $col_opts,
  bm BLOB(65535) $col_opts,
  b70k BLOB(70000) $col_opts,
  b17m BLOB(17000000) $col_opts,
  t TINYBLOB $col_opts,
  m MEDIUMBLOB $col_opts,
  l LONGBLOB $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = BLOB types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  # Valid values 
  # (cannot get MAX for all columns due to max_allowed_packet limitations)

  INSERT INTO t1 (b,b0,b1,b300,bm,b70k,b17m,t,m,l) VALUES
  ('','','','','','','','','',''),
  ('a','b','c','d','e','f','g','h','i','j'),
  ('test1','test2','test3','test4','test5','test6','test7','test8','test9','test10'),
  ( REPEAT('a',65535), REPEAT('b',65535), REPEAT('c',255), REPEAT('d',65535), REPEAT('e',65535), REPEAT('f',1048576), HEX(REPEAT('g',1048576)), REPEAT('h',255), REPEAT('i',1048576), HEX(REPEAT('j',1048576)) );

  --sorted_result
  SELECT LENGTH(b), LENGTH(b0), LENGTH(b1), LENGTH(b300), LENGTH(bm), LENGTH(b70k), LENGTH(b17m), LENGTH(t), LENGTH(m), LENGTH(l) FROM t1;

  # Invalid values (produce warnings, except for mediumblob and longblob columns for which the values are within limits)

  INSERT INTO t1 (b,b0,b1,b300,bm,b70k,b17m,t,m,l) VALUES
  ( REPEAT('a',65536), REPEAT('b',65536), REPEAT('c',256), REPEAT('d',65536), REPEAT('e',65536), REPEAT('f',1048576), REPEAT('g',1048576), REPEAT('h',256), REPEAT('i',1048576), REPEAT('j',1048576) );

  --sorted_result
  SELECT LENGTH(b), LENGTH(b0), LENGTH(b1), LENGTH(b300), LENGTH(bm), LENGTH(b70k), LENGTH(b17m), LENGTH(t), LENGTH(m), LENGTH(l) FROM t1;

  --let $error_codes = ER_TOO_BIG_DISPLAYWIDTH
  --let $alter_definition = ADD COLUMN bbb BLOB(4294967296)
  --source alter_table.inc
  if ($mysql_errname != ER_TOO_BIG_DISPLAYWIDTH)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }

  DROP TABLE t1;
}

