connection master;
--disable_warnings
DROP DATABASE IF EXISTS mysqltest;
--enable_warnings

CREATE DATABASE mysqltest CHARSET utf8mb3;
USE mysqltest;
CREATE TABLE t (cl varchar(100)) CHARSET utf8mb3;

if (!$LOAD_LOCAL)
{
  LOAD DATA INFILE '../../std_data/loaddata_utf8.dat' INTO TABLE t
  FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n';
}
if ($LOAD_LOCAL)
{
  LOAD DATA LOCAL INFILE './std_data/loaddata_utf8.dat' INTO TABLE t
  FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n';
}

echo ----------content on master----------;
SELECT hex(cl) FROM t;

--source include/rpl/sync_to_replica.inc

echo ----------content on slave----------;
USE mysqltest;
SELECT hex(cl) FROM t;

connection master;
DROP DATABASE mysqltest;
--source include/rpl/sync_to_replica.inc
