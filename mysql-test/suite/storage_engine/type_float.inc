#
# Float types
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $create_definition =
  f FLOAT $col_opts,
  f0 FLOAT(0) $col_opts,
  r1_1 REAL(1,1) $col_opts,
  f23_0 FLOAT(23) $col_opts,
  f20_3 FLOAT(20,3) $col_opts,
  d DOUBLE $col_opts,
  d1_0 DOUBLE(1,0) $col_opts,
  d10_10 DOUBLE PRECISION (10,10) $col_opts,
  d53 DOUBLE(53,0) $col_opts,
  d53_10 DOUBLE(53,10) $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = Float point types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  # Always valid values 

  INSERT INTO t1 (f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10) VALUES (12345.12345,12345.12345,0.9,123456789.123,56789.987,11111111.111,8.0,0.0123456789,1234566789123456789,99999999999999999.99999999);

  --sorted_result
  --query_vertical SELECT f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10 FROM t1

  INSERT INTO t1 (f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10) VALUES (0,0,0,0,0,0,0,0,0,0);
  INSERT INTO t1 (f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10) VALUES (
    99999999999999999999999999999999999999,
    99999999999999999999999999999999999999.9999999999999999,
    0.9,
    99999999999999999999999999999999999999.9,
    99999999999999999.999,
    999999999999999999999999999999999999999999999999999999999999999999999999999999999,
    9,
    0.9999999999, 
    1999999999999999999999999999999999999999999999999999999,
    19999999999999999999999999999999999999999999.9999999999
  );

  --sorted_result
  --query_vertical SELECT f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10 FROM t1

  # Values which can be valid or not, 
  # depending on whether columns are SIGNED or UNSIGNED
  # (if not valid should produce warnings)

  INSERT INTO t1 (f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10) VALUES (-999999999999999999999999,-99999999999.999999999999,-0.9,-999.99999999999999999999,-99999999999999999.999,-999999999999999999999999999999999999999999999999999999999999-0.999,-9,-.9999999999,-999999999999999999999999999999.99999999999999999999999,-9999999999999999999999999999999999999999999.9999999999);

  --sorted_result
  --query_vertical SELECT f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10 FROM t1

  --sorted_result
  --query_vertical SELECT MAX(f), MAX(f0), MAX(r1_1), MAX(f23_0), MAX(f20_3), MAX(d), MAX(d1_0), MAX(d10_10), MAX(d53), MAX(d53_10) FROM t1

  # Invalid values

  INSERT INTO t1 (f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10) SELECT d53_10, d53_10, d53_10, d53_10, d53_10, d53_10, d53_10, d53_10, d53_10, d53_10 FROM t1 ORDER BY d53_10 DESC LIMIT 1;

  --sorted_result
  --query_vertical SELECT f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10 FROM t1

  INSERT INTO t1 (f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10) VALUES (
    999999999999999999999999999999999999999,
    999999999999999999999999999999999999999.9999999999999999,
    1.9,
    999999999999999999999999999999999999999.9,
    999999999999999999.999,
    9999999999999999999999999999999999999999999999999999999999999999999999999999999999,
    99,
    1.9999999999,
    1999999999999999999999999999999999999999999999999999999,
    19999999999999999999999999999999999999999999.9999999999
  );

  --sorted_result
  --query_vertical SELECT f,f0,r1_1,f23_0,f20_3,d,d1_0,d10_10,d53,d53_10 FROM t1

  --let $error_codes = ER_TOO_BIG_DISPLAYWIDTH
  --let $alter_definition = ADD COLUMN d0_0 DOUBLE(0,0)
  --source alter_table.inc
  if ($mysql_errname != ER_TOO_BIG_DISPLAYWIDTH)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }

  --let $error_codes = ER_TOO_BIG_PRECISION
  --let $alter_definition = ADD COLUMN n66_6 DECIMAL(256,1)
  --source alter_table.inc
  --let $error_codes = ER_TOO_BIG_SCALE
  --let $alter_definition = ADD COLUMN n66_66 DECIMAL(40,35)
  --source alter_table.inc

  DROP TABLE t1;
}

