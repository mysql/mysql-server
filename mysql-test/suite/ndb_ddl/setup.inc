--source include/have_ndb.inc


# No need to include the setup commands in .result file
--disable_query_log

# Create one connection to each mysqld
let $i = $NUM_MYSQLDS;
while($i)
{
  let $_port= \$MYSQLD_PORT_$i;
  if (!$_port)
  {
    die Not all $MYSQLD_PORT_* env. variables are configured;
  }
  --connect(mysqld$i,127.0.0.1,root,,test,$_port)

  dec $i;
}

# Create special ndb_ddl_test databases
CREATE DATABASE ndb_ddl_test;
CREATE DATABASE ndb_ddl_test2;

# Always use the ndb_ddl_test database
USE ndb_ddl_test;

eval
CREATE TABLE t1(
  a INT,
  b INT,
  PRIMARY KEY(a,b),
  KEY(b)
)
$t1_table_options
ENGINE=ndb;

INSERT INTO t1 VALUES (1,1), (2,2), (3,3), (4,4), (5,5);

eval
CREATE TABLE t2(
  a INT PRIMARY KEY,
  b VARCHAR(255),
  c DATETIME,
  KEY(b),
  KEY(b,c)
)
$t2_table_options
ENGINE=ndb;

INSERT INTO t2 VALUES
  (1,'1','2015-03-01 00:00:01'),
  (2,'1','2015-03-02 00:00:02'),
  (3,'1','2015-03-03 00:00:03'),
  (4,'1','2015-03-04 00:00:04'),
  (5,'1','2015-03-05 00:00:05');

eval
CREATE TABLE t3(
  a INT PRIMARY KEY,
  b VARCHAR(255),
  c DATETIME,
  KEY(b),
  INDEX(a,b,c)
)
$t3_table_options
ENGINE=ndb;

INSERT INTO t3 VALUES
  (1,'1','2015-03-01 00:00:01'),
  (2,'1','2015-03-02 00:00:02'),
  (3,'1','2015-03-03 00:00:03'),
  (4,'1','2015-03-04 00:00:04'),
  (5,'1','2015-03-05 00:00:05');

eval
CREATE TABLE t4 (
  a INT UNSIGNED NOT NULL PRIMARY KEY,
  b INT UNSIGNED,
  c INT UNSIGNED,
  UNIQUE bc_unieuq_index(b,c),
  UNIQUE b_unique(c)
)
$t4_table_options
ENGINE=ndb;

INSERT INTO t4 VALUES (1,1,1), (2,2,2), (3,3,3), (4,4,4), (5,5,5);

eval
CREATE TABLE t5 (
  a int NOT NULL,
  b int NOT NULL,
  c int NOT NULL,
  d int NOT NULL,
  e int,
  PRIMARY KEY (c, b, a, d),
  INDEX(d),
  INDEX(c)
)
$t5_table_options
ENGINE=ndb;

INSERT INTO t5 VALUES
  (1,1,1,1,1), (2,2,2,2,2), (3,3,3,3,3), (4,4,4,4,4), (5,5,5,5,5);

eval
CREATE TABLE t6 (
  a varchar(37)
)
$t6_table_options
ENGINE=ndb;

INSERT INTO t6 VALUES
  ('value1'),
  ('value2'),
  ('value3'),
  ('value4'),
  ('value5');

# blob table
eval
CREATE TABLE t7 (
  a INT NOT NULL PRIMARY KEY,
  b TEXT NOT NULL,
  c INT NOT NULL,
  d LONGBLOB,
  KEY index_c(c)
)
$t7_table_options
engine=ndbcluster;

INSERT INTO t7 VALUES
  (1,REPEAT('1b', 371),1,REPEAT('1d', 65531)),
  (2,REPEAT('2b', 372),2,REPEAT('2d', 65532)),
  (3,REPEAT('3b', 373),3,REPEAT('3d', 65533)),
  (4,REPEAT('4b', 374),4,REPEAT('4d', 65534)),
  (5,REPEAT('5b', 375),5,REPEAT('5d', 65535));

# hidden pk table
eval
CREATE TABLE t8 (
  a INT,
  b VARCHAR(255),
  c DATETIME NOT NULL,
  d INT UNSIGNED,
  e INT NOT NULL
)
$t8_table_options
engine=ndbcluster;

INSERT INTO t8 VALUES
  (1, "1-t8", '2017-10-20 09:13:01', NULL, 1),
  (2, "2-t8", '2017-10-20 09:13:02', NULL, 2),
  (3, "3-t8", '2017-10-20 09:13:03', NULL, 3),
  (4, "4-t8", '2017-10-20 09:13:04', NULL, 4),
  (5, "5-t8", '2017-10-20 09:13:05', NULL, 5);

# hidden pk and blobs

# ignore the warning about binlogging not supported on this table
--disable_warnings ONCE
eval
CREATE TABLE t9 (
  a INT,
  b VARCHAR(255),
  c TEXT,
  d MEDIUMBLOB,
  e INT NOT NULL
)
$t9_table_options
engine=ndbcluster;

INSERT INTO t9 VALUES
  (1, "1-t9", REPEAT('1f', 41), REPEAT('1g', 31), 1),
  (2, "2-t9", REPEAT('1f', 42), REPEAT('1g', 32), 2),
  (3, "3-t9", REPEAT('1f', 43), REPEAT('1g', 33), 3),
  (4, "4-t9", REPEAT('1f', 44), REPEAT('1g', 34), 4),
  (5, "5-t9", REPEAT('1f', 45), REPEAT('1g', 35), 5);

# Create a view on top of  ndbinfo.ndb$dict_obj_info (as otherwise
# the dollar sign is evaluated as a dollar-variable and there seem
# to be no way to escape it)
CREATE VIEW test.ndbinfo_dict_obj_info as
  SELECT id, type, version, state, parent_obj_type, parent_obj_id, fq_name
    FROM ndbinfo.ndb$dict_obj_info;

--enable_query_log

#
# List all objects for the created tables and store them
# in temporary tables in the test database
let $num_tables = `select count(*) from information_schema.tables
                     where TABLE_SCHEMA = 'ndb_ddl_test'`;
if (!$num_tables)
{
  die Could not figure out number of tables in ndb_ddl_test database;
}

let $counter = 1;
while ($counter <= $num_tables)
{

  let $create_table_name = test.pre_t$counter;
  let $list_table_name = ndb_ddl_test.t$counter;
  --source list_objects.inc

  inc $counter;
}
