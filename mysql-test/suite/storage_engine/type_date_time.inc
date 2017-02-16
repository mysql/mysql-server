#
# Date and time column types
# (DATE, DATETIME, TIMESTAMP, TIME, YEAR)
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $create_definition =
  d DATE $col_opts,
  dt DATETIME $col_opts,
  ts TIMESTAMP $col_opts,
  t TIME $col_opts,
  y YEAR $col_opts,
  y4 YEAR(4) $col_opts,
  y2 YEAR(2) $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = Date and time types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  SET @tm = '2012-04-09 05:27:00';

  # Valid values 
  # '1970-01-01 00:00:01'
  INSERT INTO t1 (d,dt,ts,t,y,y4,y2) VALUES
  ('1000-01-01', '1000-01-01 00:00:00', FROM_UNIXTIME(1), '-838:59:59', '1901', '1901', '00'),
  ('9999-12-31', '9999-12-31 23:59:59', FROM_UNIXTIME(2147483647), '838:59:59', '2155', '2155', '99'),
  ('0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00', '00:00:00', '0', '0', '0'),
  (DATE(@tm),@tm,TIMESTAMP(@tm),TIME(@tm),YEAR(@tm),YEAR(@tm),YEAR(@tm)); 

  --sorted_result
  SELECT d,dt,ts,t,y,y4,y2 FROM t1;

  # Invalid values

  INSERT INTO t1 (d,dt,ts,t,y,y4,y2) VALUES
  ('999-13-32', '999-11-31 00:00:00', '0', '-839:00:00', '1900', '1900', '-1' );

  --sorted_result
  SELECT d,dt,ts,t,y,y4,y2 FROM t1;

  DROP TABLE t1;
}


