#
# Fixed point types
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $create_definition =
  d DECIMAL $col_opts,
  d0 DECIMAL(0) $col_opts,
  d1_1 DECIMAL(1,1) $col_opts,
  d10_2 DECIMAL(10,2) $col_opts,
  d60_10 DECIMAL(60,10) $col_opts,
  n NUMERIC $col_opts,
  n0_0 NUMERIC(0,0) $col_opts,
  n1 NUMERIC(1) $col_opts,
  n20_4 NUMERIC(20,4) $col_opts,
  n65_4 NUMERIC(65,4) $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = Fixed point types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  # Always valid values 

  INSERT INTO t1 (d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4) VALUES (100,123456,0.3,40000.25,123456789123456789.10001,1024,7000.0,8.0,999999.9,9223372036854775807);
  INSERT INTO t1 (d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4) VALUES (0,0,0,0,0,0,0,0,0,0);
  INSERT INTO t1 (d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4) VALUES (9999999999.0,9999999999.0,0.9,99999999.99,99999999999999999999999999999999999999999999999999.9999999999,9999999999.0,9999999999.0,9.0,9999999999999999.9999,9999999999999999999999999999999999999999999999999999999999999.9999);

  --sorted_result
  SELECT d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4 FROM t1;

  # Values which can be valid or not, 
  # depending on whether columns are SIGNED or UNSIGNED
  # (if not valid should produce warnings)

  INSERT INTO t1 (d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4) VALUES (-100,-123456,-0.3,-40000.25,-123456789123456789.10001,-1024,-7000.0,-8.0,-999999.9,-9223372036854775807);
  INSERT INTO t1 (d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4) VALUES (-9999999999.0,-9999999999.0,-0.9,-99999999.99,-99999999999999999999999999999999999999999999999999.9999999999,-9999999999.0,-9999999999.0,-9.0,-9999999999999999.9999,-9999999999999999999999999999999999999999999999999999999999999.9999);

  --sorted_result
  SELECT d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4 FROM t1;

  --sorted_result
  SELECT d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4 FROM t1 WHERE n20_4 = 9999999999999999.9999 OR d < 100;

  # Invalid values

  INSERT INTO t1 (d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4) SELECT n65_4, n65_4, n65_4, n65_4, n65_4, n65_4, n65_4, n65_4, n65_4, n65_4 FROM t1 WHERE n65_4 = ( SELECT MAX(n65_4) FROM t1 );

  --sorted_result
  SELECT d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4 FROM t1;

  INSERT INTO t1 (d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4) VALUES (10000000000.0,10000000000.0,1.1,100000000.99,100000000000000000000000000000000000000000000000000.0,10000000000.0,10000000000.0,10.0,10000000000000000.9999,10000000000000000000000000000000000000000000000000000000000000.9999);
  --sorted_result
  SELECT d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4 FROM t1;

  INSERT INTO t1 (d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4) VALUES (9999999999.1,9999999999.1,1.9,99999999.001,99999999999999999999999999999999999999999999999999.99999999991,9999999999.1,9999999999.1,9.1,9999999999999999.00001,9999999999999999999999999999999999999999999999999999999999999.11111);
  --sorted_result
  SELECT d,d0,d1_1,d10_2,d60_10,n,n0_0,n1,n20_4,n65_4 FROM t1;

  --let $error_codes = ER_TOO_BIG_PRECISION
  --let $alter_definition = ADD COLUMN n66 NUMERIC(66)
  --source alter_table.inc
  if ($mysql_errname != ER_TOO_BIG_PRECISION)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }

  --let $error_codes = ER_TOO_BIG_PRECISION
  --let $alter_definition = ADD COLUMN n66_6 DECIMAL(66,6)
  --source alter_table.inc
  --let $error_codes = ER_TOO_BIG_SCALE
  --let $alter_definition = ADD COLUMN n66_66 DECIMAL(66,66)
  --source alter_table.inc

  DROP TABLE t1;
}

