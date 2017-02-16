#
# INT column types
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $create_definition =
  i INT $col_opts,
  i0 INT(0) $col_opts,
  i1 INT(1) $col_opts,
  i20 INT(20) $col_opts,
  t TINYINT $col_opts,
  t0 TINYINT(0) $col_opts,
  t1 TINYINT(1) $col_opts,
  t20 TINYINT(20) $col_opts,
  s SMALLINT $col_opts,
  s0 SMALLINT(0) $col_opts,
  s1 SMALLINT(1) $col_opts,
  s20 SMALLINT(20) $col_opts,
  m MEDIUMINT $col_opts,
  m0 MEDIUMINT(0) $col_opts,
  m1 MEDIUMINT(1) $col_opts,
  m20 MEDIUMINT(20) $col_opts,
  b BIGINT $col_opts,
  b0 BIGINT(0) $col_opts,
  b1 BIGINT(1) $col_opts,
  b20 BIGINT(20) $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = INT types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  # Always valid values 

  INSERT INTO t1 (i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20) VALUES (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20);
  INSERT INTO t1 (i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20) VALUES (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
  INSERT INTO t1 (i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20) VALUES (2147483647,2147483647,2147483647,2147483647,127,127,127,127,32767,32767,32767,32767,8388607,8388607,8388607,8388607,9223372036854775807,9223372036854775807,9223372036854775807,9223372036854775807);
  --sorted_result
  SELECT i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20 FROM t1;

  # Values which can be valid or not, 
  # depending on whether columns are SIGNED or UNSIGNED
  # (if not valid should produce warnings)

  INSERT INTO t1 (i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20) VALUES (-2147483648,-2147483648,-2147483648,-2147483648,-128,-128,-128,-128,-32768,-32768,-32768,-32768,-8388608,-8388608,-8388608,-8388608,-9223372036854775808,-9223372036854775808,-9223372036854775808,-9223372036854775808);
  INSERT INTO t1 (i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20) VALUES (4294967295,4294967295,4294967295,4294967295,255,255,255,255,65535,65535,65535,65535,16777215,16777215,16777215,16777215,18446744073709551615,18446744073709551615,18446744073709551615,18446744073709551615);

  --sorted_result
  SELECT i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20 FROM t1;

  # Invalid values

  INSERT INTO t1 (i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20) VALUES (-2147483649,-2147483649,-2147483649,-2147483649,-129,-129,-129,-129,-32769,-32769,-32769,-32769,-8388609,-8388609,-8388609,-8388609,-9223372036854775809,-9223372036854775809,-9223372036854775809,-9223372036854775809);

  INSERT INTO t1 (i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20) VALUES (4294967296,4294967296,4294967296,4294967296,256,256,256,256,65536,65536,65536,65536,16777216,16777216,16777216,16777216,18446744073709551616,18446744073709551616,18446744073709551616,18446744073709551616);

  INSERT INTO t1 (i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20) SELECT b,b,b,b,b,b,b,b,b,b,b,b,b,b,b,b,b,b,b,b FROM t1 WHERE b IN (-9223372036854775808,9223372036854775807,18446744073709551615);

  --sorted_result
  SELECT i,i0,i1,i20,t,t0,t1,t20,s,s0,s1,s20,m,m0,m1,m20,b,b0,b1,b20 FROM t1;

  --let $error_codes = ER_TOO_BIG_DISPLAYWIDTH
  --let $alter_definition = ADD COLUMN i257 INT(257)
  --source alter_table.inc
  if ($mysql_errname != ER_TOO_BIG_DISPLAYWIDTH)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }

  DROP TABLE t1;
}

