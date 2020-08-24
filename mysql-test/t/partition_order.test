#--disable_abort_on_error
#
# Simple test for the partition storage engine
# Focuses on tests of ordered index read
# 

--disable_warnings
drop table if exists t1;
--enable_warnings

#
# Ordered index read, int type
#
CREATE TABLE t1 (
a int not null,
b int not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 order by b;

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, unsigned int type
#
CREATE TABLE t1 (
a int not null,
b int unsigned not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, tiny int type
#
CREATE TABLE t1 (
a int not null,
b tinyint not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1; 

#
# Ordered index read, unsigned tinyint type
#
CREATE TABLE t1 (
a int not null,
b tinyint unsigned not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1; 

#
# Ordered index read, smallint type
#
CREATE TABLE t1 (
a int not null,
b smallint not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1; 

#
# Ordered index read, unsigned smallint type
#
CREATE TABLE t1 (
a int not null,
b smallint unsigned not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1; 
#
# Ordered index read, mediumint type
#
CREATE TABLE t1 (
a int not null,
b mediumint not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1; 

#
# Ordered index read, unsigned int type
#
CREATE TABLE t1 (
a int not null,
b mediumint unsigned not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1; 

#
# Ordered index read, unsigned bigint type
#
CREATE TABLE t1 (
a int not null,
b bigint unsigned not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1; 

#
# Ordered index read, bigint type
#
CREATE TABLE t1 (
a int not null,
b bigint not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1; 
#
# Ordered index read, bigint type
#
CREATE TABLE t1 (
a int not null,
b bigint not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, float type
#
CREATE TABLE t1 (
a int not null,
b float not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, double type
#
CREATE TABLE t1 (
a int not null,
b double not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, unsigned double type
#
CREATE TABLE t1 (
a int not null,
b double unsigned not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, unsigned float type
#
CREATE TABLE t1 (
a int not null,
b float unsigned not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, double precision type
#
CREATE TABLE t1 (
a int not null,
b double precision not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;
#
# Ordered index read, unsigned double precision type
#
CREATE TABLE t1 (
a int not null,
b double precision unsigned not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, decimal type
#
CREATE TABLE t1 (
a int not null,
b decimal not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (2, 5);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);

select * from t1 force index (b) where b > 0 order by b;

drop table t1;
#
# Ordered index read, char type
#
CREATE TABLE t1 (
a int not null,
b char(10) not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b > 0 order by b;

drop table t1;

#
# Ordered index read, varchar type
#
CREATE TABLE t1 (
a int not null,
b varchar(10) not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b > '0' order by b;

drop table t1;
#
# Ordered index read, varchar type limited index size
#
CREATE TABLE t1 (
a int not null,
b varchar(10) not null,
primary key(a),
index (b(5)))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b > '0' order by b;

drop table t1;

#
# Ordered index read, varchar binary type
#
CREATE TABLE t1 (
a int not null,
b varchar(10) binary not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b > '0' order by b;

drop table t1;

#
# Ordered index read, tinytext type
#
CREATE TABLE t1 (
a int not null,
b tinytext not null,
primary key(a),
index (b(10)))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b > '0' order by b;

drop table t1;
#
# Ordered index read, text type
#
CREATE TABLE t1 (
a int not null,
b text not null,
primary key(a),
index (b(10)))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b > '0' order by b;

drop table t1;

#
# Ordered index read, mediumtext type
#
CREATE TABLE t1 (
a int not null,
b mediumtext not null,
primary key(a),
index (b(10)))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b > '0' order by b;

drop table t1;
#
# Ordered index read, longtext type
#
CREATE TABLE t1 (
a int not null,
b longtext not null,
primary key(a),
index (b(10)))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b > '0' order by b;

drop table t1;
#
# Ordered index read, enum type
#
CREATE TABLE t1 (
a int not null,
b enum('1','2', '4', '5') not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b >= '1' order by b;

drop table t1;
#
# Ordered index read, set type
#
CREATE TABLE t1 (
a int not null,
b set('1','2', '4', '5') not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '1');
INSERT into t1 values (2, '5');
INSERT into t1 values (30, '4');
INSERT into t1 values (35, '2');

select * from t1 force index (b) where b >= '1' order by b;

drop table t1;
#
# Ordered index read, date type
#
CREATE TABLE t1 (
a int not null,
b date not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '2001-01-01');
INSERT into t1 values (2, '2005-01-01');
INSERT into t1 values (30, '2004-01-01');
INSERT into t1 values (35, '2002-01-01');

select * from t1 force index (b) where b > '2000-01-01' order by b;

drop table t1;
#
# Ordered index read, datetime type
#
CREATE TABLE t1 (
a int not null,
b datetime not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '2001-01-01 00:00:00');
INSERT into t1 values (2, '2005-01-01 00:00:00');
INSERT into t1 values (30, '2004-01-01 00:00:00');
INSERT into t1 values (35, '2002-01-01 00:00:00');

select * from t1 force index (b) where b > '2000-01-01 00:00:00' order by b;

drop table t1;
#
# Ordered index read, timestamp type
#
CREATE TABLE t1 (
a int not null,
b timestamp not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '2001-01-01 00:00:00');
INSERT into t1 values (2, '2005-01-01 00:00:00');
INSERT into t1 values (30, '2004-01-01 00:00:00');
INSERT into t1 values (35, '2002-01-01 00:00:00');

select * from t1 force index (b) where b > '2000-01-01 00:00:00' order by b;

drop table t1;
#
# Ordered index read, time type
#
CREATE TABLE t1 (
a int not null,
b time not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, '01:00:00');
INSERT into t1 values (2, '05:00:00');
INSERT into t1 values (30, '04:00:00');
INSERT into t1 values (35, '02:00:00');

select * from t1 force index (b) where b > '00:00:00' order by b;

drop table t1;
#
# Ordered index read, year type
#
CREATE TABLE t1 (
a int not null,
b year not null,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 2001);
INSERT into t1 values (2, 2005);
INSERT into t1 values (30, 2004);
INSERT into t1 values (35, 2002);

select * from t1 force index (b) where b > 2000 order by b;

drop table t1;
#
# Ordered index read, bit(5) type
#
CREATE TABLE t1 (
a int not null,
b bit(5) not null,
c int,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, b'00001', NULL);
INSERT into t1 values (2, b'00101', 2);
INSERT into t1 values (30, b'00100', 2);
INSERT into t1 values (35, b'00010', NULL);

select a from t1 force index (b) where b > b'00000' order by b;

drop table t1;
#
# Ordered index read, bit(15) type
#
CREATE TABLE t1 (
a int not null,
b bit(15) not null,
c int,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1,  b'000000000000001', NULL);
INSERT into t1 values (2,  b'001010000000101', 2);
INSERT into t1 values (30, b'001000000000100', 2);
INSERT into t1 values (35, b'000100000000010', NULL);

select a from t1 force index (b) where b > b'000000000000000' order by b;

drop table t1;

#
# Ordered index read, NULL values
#
CREATE TABLE t1 (
a int not null,
b int,
primary key(a),
index (b))
partition by range (a)
partitions 2
(partition x1 values less than (25),
 partition x2 values less than (100));

# Insert a couple of tuples
INSERT into t1 values (1, 1);
INSERT into t1 values (5, NULL);
INSERT into t1 values (2, 4);
INSERT into t1 values (3, 3);
INSERT into t1 values (4, 5);
INSERT into t1 values (7, 1);
INSERT into t1 values (6, 6);
INSERT into t1 values (30, 4);
INSERT into t1 values (35, 2);
INSERT into t1 values (40, NULL);

--partially_sorted_result 1
select b,a from t1 force index (b) where b < 10 OR b IS NULL order by b;
--partially_sorted_result 1
select b,a from t1 force index (b) where b < 10 ORDER BY b;
--partially_sorted_result 1
select b,a from t1 force index (b) where b < 10 ORDER BY b DESC;
drop table t1;

create table t1 (a int not null, b int, c varchar(20), key (a,b,c))
partition by range (b)
(partition p0 values less than (5),
 partition p1 values less than (10));
INSERT into t1 values (1,1,'1'),(2,2,'2'),(1,3,'3'),(2,4,'4'),(1,5,'5');
INSERT into t1 values (2,6,'6'),(1,7,'7'),(2,8,'8'),(1,9,'9');
INSERT into t1 values (1, NULL, NULL), (2, NULL, '10');
select * from t1 where a = 1 order by a desc, b desc;
select * from t1 where a = 1 order by b desc;
drop table t1;
