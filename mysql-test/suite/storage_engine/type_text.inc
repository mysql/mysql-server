#
# TEXT column types
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $create_definition =
  t TEXT $col_opts,
  t0 TEXT(0) $col_opts,
  t1 TEXT(1) $col_opts,
  t300 TEXT(300) $col_opts,
  tm TEXT(65535) $col_opts,
  t70k TEXT(70000) $col_opts,
  t17m TEXT(17000000) $col_opts,
  tt TINYTEXT $col_opts,
  m MEDIUMTEXT $col_opts,
  l LONGTEXT $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = TEXT types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  # Valid values 
  # (cannot get MAX for all columns due to max_allowed_packet limitations)

  INSERT INTO t1 (t,t0,t1,t300,tm,t70k,t17m,tt,m,l) VALUES
  ('','','','','','','','','',''),
  ('a','b','c','d','e','f','g','h','i','j'),
  ('test1','test2','test3','test4','test5','test6','test7','test8','test9','test10'),
  ( REPEAT('a',65535), REPEAT('b',65535), REPEAT('c',255), REPEAT('d',65535), REPEAT('e',65535), REPEAT('f',1048576), REPEAT('g',1048576), REPEAT('h',255), REPEAT('i',1048576), REPEAT('j',1048576) );

  --sorted_result
  SELECT LENGTH(t), LENGTH(t0), LENGTH(t1), LENGTH(t300), LENGTH(tm), LENGTH(t70k), LENGTH(t17m), LENGTH(tt), LENGTH(m), LENGTH(l) FROM t1;

  # Invalid values (produce warnings, except for mediumtext and longtext columns for which the values are within limits)

  INSERT INTO t1 (t,t0,t1,t300,tm,t70k,t17m,tt,m,l) VALUES
  ( REPEAT('a',65536), REPEAT('b',65536), REPEAT('c',256), REPEAT('d',65536), REPEAT('e',65536), REPEAT('f',1048576), REPEAT('g',1048576), REPEAT('h',256), REPEAT('i',1048576), REPEAT('j',1048576) );

  --sorted_result
  SELECT LENGTH(t), LENGTH(t0), LENGTH(t1), LENGTH(t300), LENGTH(tm), LENGTH(t70k), LENGTH(t17m), LENGTH(tt), LENGTH(m), LENGTH(l) FROM t1;

  --let $error_codes = ER_TOO_BIG_DISPLAYWIDTH
  --let $alter_definition = ADD COLUMN ttt TEXT(4294967296)
  --source alter_table.inc
  if ($mysql_errname != ER_TOO_BIG_DISPLAYWIDTH)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }
  DROP TABLE t1;
}


