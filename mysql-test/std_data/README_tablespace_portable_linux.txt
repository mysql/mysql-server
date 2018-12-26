wl#8619 : This readme file is prepared for testing the functionality (portability) of the wl#8619 .
Test linux --datadir and tablespaces runs on Windows platform.
Note 1: The --datadir hierarchy and schema name and its hierarchy (Ex: test/tab1.ibd) should be followed

Note 2: For older version of --datadir (Ex: 8.0), it is users responsibility to run upgrade script against the --datadir, once the
upgrade is successful, you can create a zip file (tablespace_portable_linux.zip). You can directly run portability functionality
with old --datadir (Ex: mysql-8.0). Hence intentionally upgrade step is not added in this testcase.

Note 3: The folder names and tablspace names and my.cnf config file should same as follows, do not change anything, as testcase
might fail.

1) Logon on to any Linux machine (you can also use laptop with ubuntu)

2) Build latest version of trunk

3) InitDB/bootstrap the datadir using the following my.cnf config file, here is the command

./bin/mysqld --defaults-file=./my.cnf --datadir=../Linx-DB/ --basedir=. -u root --initialize-insecure

log_error_verbosity=3
lower_case_table_names=1
innodb_log_files_in_group=4
innodb_log_group_home_dir=../data_home
innodb_data_home_dir=../data_home
innodb_undo_directory=../undo_files
innodb_undo_tablespaces=5
innodb_data_file_path=data01:20M;data02:20M:autoextend

4) start the server with the same my.cnf file and connect with mysql client
./bin/mysqld --defaults-file=./my.cnf --datadir=../Linx-DB/ --basedir=. -u root

5) Run following DDLs

Note : Do replace 'xxx' in DDL statements with target location of that machine

CREATE DATABASE test;
use test;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' Engine=InnoDB;
CREATE TABLE tab1(c1 int, c2 varchar(10)) TABLESPACE=ts1;
INSERT INTO tab1 VALUES(1, 'VISH');
CREATE TABLE tab2(c1 int , c2 varchar(10)) Engine=InnoDB;
INSERT INTO tab2 VALUES(2, 'VISH');
CREATE INDEX ix1 ON tab1(c2) USING BTREE;
CREATE INDEX ix2 ON tab2(c2) ;

create table tab3 (
empno int,
ename varchar(30),
sal   numeric(3))
engine=InnoDB row_format=compressed
partition by hash(empno) (
partition P0  DATA DIRECTORY = '/xxx/xxx/datadir1',
partition P1 DATA DIRECTORY = '/xxx/xxx/datadir1');

CREATE INDEX ix1 ON tab3(ename) USING BTREE;
INSERT INTO tab3 VALUES (100,'VISWANATH',100);
INSERT INTO tab3 VALUES (300,'VISWANATH',100);

CREATE TABLE purchase (
  `id` int(11) DEFAULT NULL,
  `purchased` date DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1
PARTITION BY RANGE ( YEAR(purchased))
SUBPARTITION BY HASH ( TO_DAYS(purchased))
(PARTITION p0 VALUES LESS THAN (1990)
 (SUBPARTITION s0 DATA DIRECTORY = '/xxx/xxx/part0' ENGINE = InnoDB,
  SUBPARTITION s1 DATA DIRECTORY = '/xxx/xxx/part1' ENGINE = InnoDB),
 PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION s2 DATA DIRECTORY = '/xxx/xxx/part2' ENGINE = InnoDB,
  SUBPARTITION s3 DATA DIRECTORY = '/xxx/xxx/part3' ENGINE = InnoDB));

INSERT INTO purchase VALUES(1,'1980-05-31');
INSERT INTO purchase VALUES(2,'1999-05-31');
INSERT INTO purchase VALUES(3,'1998-05-31');
INSERT INTO purchase VALUES(4,'1979-05-31');
INSERT INTO purchase VALUES(5,'1978-05-31');
INSERT INTO purchase VALUES(6,'1997-05-31');

CREATE TABLESPACE ts2 ADD DATAFILE '/xxx/xxx/undo_files/ts2.ibd' Engine=InnoDB;
CREATE TABLE tab4(c1 int, c2 varchar(10)) TABLESPACE=ts2;
INSERT INTO tab4 VALUES(1, 'VISH');

6) Shutdown the server (no crash or KILL -9)

7) zip entire --datadir files (following list of folders)

--datadir (Linx-DB)
--innodb_undo_directory (undo_files)
--innodb_data_home_dir (data_home)
--innodb_log_group_home_dir (data_home)
part0,part1,part2,part3,datadir1

zip -r -9 tablespace_portable_linux.zip datadir1/ Linx-DB/ undo_files/ part?

8) Copy that zip file into mysql-test/std_data location of MTR home DIR

