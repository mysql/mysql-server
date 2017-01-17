# skip sort for prefix change
# pk(o1(2)) to pk(o1(3))
create table t1(o1 varchar(10), primary key(o1(2))) engine = innodb;
insert into t1 values('abd'), ('acd');
alter table t1 drop primary key, add primary key(o1(3)), lock=none;
drop table t1;

# pk(o1(2)) to pk(o1)
create table t1(o1 varchar(10), primary key(o1(2))) engine = innodb;
insert into t1 values('abd'), ('acd');
alter table t1 drop primary key, add primary key(o1), lock=none;
drop table t1;

# pk(o1(2)) to pk(o1(3),n1)
create table t1(o1 varchar(10), primary key(o1(2))) engine = innodb;
insert into t1 values('abd'), ('acd');
alter table t1 add n1 int not null, drop primary key, add primary key(o1(3), n1), lock=none;
drop table t1;

# pk(o1(2)) to pk(o1,n1)
create table t1(o1 varchar(10), primary key(o1(2))) engine = innodb;
insert into t1 values('abd'), ('acd');
alter table t1 add n1 int not null, drop primary key, add primary key(o1, n1), lock=none;
drop table t1;

# pk(o1(2)) to pk(o1(3), o2)
create table t1(o1 varchar(10), o2 int not null, primary key(o1(2))) engine = innodb;
insert into t1 values('abd', 1), ('acd', 2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1(3), o2), lock=none;
drop table t1;

# pk(o1(2)) to pk(o1, o2)
create table t1(o1 varchar(10), o2 int not null, primary key(o1(2))) engine = innodb;
insert into t1 values('abd', 1), ('acd', 2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1, o2), lock=none;
drop table t1;

# pk(o1(3)) to pk(o1(2))
create table t1(o1 varchar(10), primary key(o1(3))) engine = innodb;
insert into t1 values('abd'), ('acd');
alter table t1 drop primary key, add primary key(o1(2)), lock=none;
drop table t1;

# pk(o1) to pk(o1(2))
create table t1(o1 varchar(10), primary key(o1)) engine = innodb;
insert into t1 values('abd'), ('acd');
alter table t1 drop primary key, add primary key(o1(2)), lock=none;
drop table t1;

# pk(o1(3),o2) to pk(o1(2))
create table t1(o1 varchar(10), o2 int, primary key(o1(3), o2)) engine = innodb;
insert into t1 values('abd', 1), ('acd', 2);
alter table t1 drop primary key, add primary key(o1(2)), lock=none;
drop table t1;

# pk(o1,o2) to pk(o1(2))
create table t1(o1 varchar(10), o2 int, primary key(o1, o2)) engine = innodb;
insert into t1 values('abd', 1), ('acd', 2);
alter table t1 drop primary key, add primary key(o1(2)), lock=none;
drop table t1;

# pk(o1(3),o2) to pk(o1(2),n1)
create table t1(o1 varchar(10), o2 int, primary key(o1(3), o2)) engine = innodb;
insert into t1 values('abd', 1), ('acd', 2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1(2),n1), lock=none;
drop table t1;

# pk(o1,o2) to pk(o1(2),n1)
create table t1(o1 varchar(10), o2 int, primary key(o1, o2)) engine = innodb;
insert into t1 values('abd', 1), ('acd', 2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1(2),n1), lock=none;
drop table t1;

# pk(o1(3),o2) to pk(o1(3),n1)
create table t1(o1 varchar(10), o2 int, primary key(o1(3), o2)) engine = innodb;
insert into t1 values('abd', 1), ('acd', 2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1(3),n1), lock=none;
drop table t1;

# pk(o1,o2) to pk(o1,n1)
create table t1(o1 varchar(10), o2 int, primary key(o1, o2)) engine = innodb;
insert into t1 values('abd', 1), ('acd', 2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1,n1), lock=none;
drop table t1;

# pk(o1,o2(3)) to pk(o1,o2(2))
create table t1(o1 int, o2 varchar(10), primary key(o1,o2(3))) engine = innodb;
insert into t1 values(1,'abd'), (2,'acd');
alter table t1 drop primary key, add primary key(o1,o2(2)), lock=none;
drop table t1;

# pk(o1,o2) to pk(o1,o2(2))
create table t1(o1 int, o2 varchar(10), primary key(o1,o2)) engine = innodb;
insert into t1 values(1,'abd'), (2,'acd');
alter table t1 drop primary key, add primary key(o1,o2(2)), lock=none;
drop table t1;

# pk(o1,o2(2)) to pk(o1,o2(3))
create table t1(o1 int, o2 varchar(10), primary key(o1,o2(2))) engine = innodb;
insert into t1 values(1, 'abd'), (2, 'acd');
alter table t1 drop primary key, add primary key(o1,o2(3)), lock=none;
drop table t1;

# pk(o1,o2(2)) to pk(o1,o2)
create table t1(o1 int, o2 varchar(10), primary key(o1,o2(2))) engine = innodb;
insert into t1 values(1, 'abd'), (2, 'acd');
alter table t1 drop primary key, add primary key(o1,o2), lock=none;
drop table t1;

# pk(o1,o2(3),o3) to pk(o1,o2(2))
create table t1(o1 int, o2 varchar(10), o3 int, primary key(o1,o2(3),o3)) engine = innodb;
insert into t1 values(1, 'abd', 1), (2, 'acd', 2);
alter table t1 drop primary key, add primary key(o1,o2(2)), lock=none;
drop table t1;

# pk(o1,o2,o3) to pk(o1,o2(2))
create table t1(o1 int, o2 varchar(10), o3 int, primary key(o1,o2,o3)) engine = innodb;
insert into t1 values(1, 'abd', 1), (2, 'acd', 2);
alter table t1 drop primary key, add primary key(o1,o2(2)), lock=none;
drop table t1;

# pk(o1(3),o2(3)) to pk(o1(3),o2(2))
create table t1(o1 varchar(10), o2 varchar(10), primary key(o1(3),o2(3))) engine = innodb;
insert into t1 values('abd', 'acd'), ('acd', 'abd');
alter table t1 drop primary key, add primary key(o1(3),o2(2)), lock=none;
drop table t1;

# pk(o1,o2) to pk(o1,o2(2))
create table t1(o1 varchar(10), o2 varchar(10), primary key(o1,o2)) engine = innodb;
insert into t1 values('abd', 'acd'), ('acd', 'abd');
alter table t1 drop primary key, add primary key(o1,o2(2)), lock=none;
drop table t1;

# pk(o1(3),o2(2)) to pk(o1(3),o2(3))
create table t1(o1 varchar(10), o2 varchar(10), primary key(o1(3),o2(2))) engine = innodb;
insert into t1 values('abd', 'acd'), ('acd', 'abd');
alter table t1 drop primary key, add primary key(o1(3),o2(3)), lock=none;
drop table t1;

# pk(o1,o2(2)) to pk(o1,o2)
create table t1(o1 varchar(10), o2 varchar(10), primary key(o1,o2(2))) engine = innodb;
insert into t1 values('abd', 'acd'), ('acd', 'abd');
alter table t1 drop primary key, add primary key(o1,o2), lock=none;
drop table t1;

# pk(o1(3),o2,o3(2)) to pk(o1(3),o2,o3(3))
create table t1(o1 varchar(10), o2 int, o3 varchar(10), primary key(o1(3),o2,o3(2))) engine = innodb;
insert into t1 values('abd', 1, 'acd'), ('acd', 2, 'abd');
alter table t1 drop primary key, add primary key(o1(3),o2,o3(3)), lock=none;
drop table t1;

# pk(o1,o2,o3(2)) to pk(o1,o2,o3)
create table t1(o1 varchar(10), o2 int, o3 varchar(10), primary key(o1,o2,o3(2))) engine = innodb;
insert into t1 values('abd', 1, 'acd'), ('acd', 2, 'abd');
alter table t1 drop primary key, add primary key(o1,o2,o3), lock=none;
drop table t1;

# pk(o1(3),o2,o3(3)) to pk(o1(3),o2,o3(2))
create table t1(o1 varchar(10), o2 int, o3 varchar(10), primary key(o1(3),o2,o3(3))) engine = innodb;
insert into t1 values('abd', 1, 'acd'), ('acd', 2, 'abd');
alter table t1 drop primary key, add primary key(o1(3),o2,o3(2)), lock=none;
drop table t1;

# pk(o1,o2,o3) to pk(o1,o2,o3(2))
create table t1(o1 varchar(10), o2 int, o3 varchar(10), primary key(o1,o2,o3(3))) engine = innodb;
insert into t1 values('abd', 1, 'acd'), ('acd', 2, 'abd');
alter table t1 drop primary key, add primary key(o1,o2,o3(2)), lock=none;
drop table t1;

# skip sort for adding existing columns/newly added columns, dropping pk columns at the end.
# pk(o1) to pk(o1,o2)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 drop primary key, add primary key(o1,o2), lock=none;
drop table t1;

# pk(o1) to pk(o1,n1)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1,n1), lock=none;
drop table t1;

# pk(o1) to pk(n1,o1)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 add n1 int not null, drop primary key, add primary key(n1,o1), lock=none;
drop table t1;

# pk(o1) to pk(n1,o1,n2)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 add n1 int not null, add n2 int not null, drop primary key, add primary key(n1,o1,n2), lock=none;
drop table t1;

# pk(o1) to pk(n1,n2,o1)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 add n1 int not null, add n2 int not null, drop primary key, add primary key(n1,n2,o1), lock=none;
drop table t1;

# pk(o1) to pk(o1,n1,n2)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 add n1 int not null, add n2 int not null, drop primary key, add primary key(o1,n1,n2), lock=none;
drop table t1;

# pk(o1) to pk(o1,o2,n1)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1,o2,n1), lock=none;
drop table t1;

# pk(o1) to pk(o1,n1,o2)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 add n1 int not null, drop primary key, add primary key(o1,n1,o2), lock=none;
drop table t1;

# pk(o1) to pk(n1,o1,o2)
create table t1(o1 int, o2 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1),(2,2);
alter table t1 add n1 int not null, drop primary key, add primary key(n1,o1,o2), lock=none;
drop table t1;

# pk(o1) to pk(o1,o2,o3)
create table t1(o1 int, o2 int not null, o3 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1,2),(2,2,1);
alter table t1 drop primary key, add primary key(o1,o2,o3), lock=none;
drop table t1;

# pk(o1) to pk(o1,o3,o2)
create table t1(o1 int, o2 int not null, o3 int not null, primary key(o1)) engine = innodb;
insert into t1 values(1,1,2),(2,2,1);
alter table t1 drop primary key, add primary key(o1,o3,o2), lock=none;
drop table t1;

# pk(o1,o2,o3) to pk(o1,o2)
create table t1(o1 int, o2 int, o3 int, primary key(o1,o2,o3)) engine = innodb;
insert into t1 values(1,1,2),(2,2,1);
alter table t1 drop primary key, add primary key(o1,o2), lock=none;
drop table t1;

# pk(o1,o2,o3) to pk(o1,o2,o3,o4)
create table t1(o1 int, o2 int, o3 int, o4 int not null, primary key(o1,o2,o3)) engine = innodb;
insert into t1 values(1,1,2,2),(2,2,1,1);
alter table t1 add n1 int not null, drop primary key, add primary key(o1,o2,o3,o4), lock=none;
drop table t1;

# pk(o1,o2,o3) to pk(o1,o2,n1)
create table t1(o1 int, o2 int, o3 int, primary key(o1,o2,o3)) engine = innodb;
insert into t1 values(1,1,2),(2,2,1);
alter table t1 add n1 int not null, drop primary key, add primary key(o1,o2,n1), lock=none;
drop table t1;

# pk(o1,o2,o3) to pk(o1,n1,o2)
create table t1(o1 int, o2 int, o3 int, primary key(o1,o2,o3)) engine = innodb;
insert into t1 values(1,1,2),(2,2,1);
alter table t1 add n1 int not null, drop primary key, add primary key(o1,n1,o2), lock=none;
drop table t1;

# pk(o1,o2,o3) to pk(o1)
create table t1(o1 int, o2 int, o3 int, primary key(o1,o2,o3)) engine = innodb;
insert into t1 values(1,1,2),(2,2,1);
alter table t1 drop primary key, add primary key(o1), lock=none;
drop table t1;

# pk(o1) to pk(o1,o2)
CREATE TABLE t1(o1 int, o2 int, o3 int, PRIMARY KEY(o1), KEY(o2)) ENGINE=INNODB;
INSERT INTO t1 VALUES(1,1,2),(2,2,1);

EXPLAIN SELECT * FROM t1 ORDER BY o2;
SELECT * FROM t1 ORDER BY o2;

ALTER TABLE t1 DROP INDEX o2, ADD INDEX (o2 DESC);

CHECK TABLE t1;
SELECT * FROM t1;
EXPLAIN SELECT * FROM t1 ORDER BY o2;
SELECT * FROM t1 ORDER BY o2;

ALTER TABLE t1 DROP INDEX o2, ADD INDEX (o2 ASC);

CHECK TABLE t1;
SELECT * FROM t1;
EXPLAIN SELECT * FROM t1 ORDER BY o2;
SELECT * FROM t1 ORDER BY o2;

DROP TABLE t1;
