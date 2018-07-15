create user insertsqluser@localhost identified by 'insertsqluser';
grant all on *.* to insertsqluser@localhost;
RUN DROP SCHEMA if EXISTS mysqlxplugin

0 rows affected
RUN CREATE SCHEMA mysqlxplugin

1 rows affected
RUN USE mysqlxplugin

0 rows affected
RUN CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu'

0 rows affected
RUN SELECT name,space_type,state FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
                             WHERE space_type = 'Undo'
name	space_type	state
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	active
0 rows affected
RUN ALTER UNDO TABLESPACE undo_003 SET INACTIVE

0 rows affected
RUN CREATE TABLE tab2(c1 int , c2 varchar(10)) Engine=InnoDB

0 rows affected
RUN INSERT INTO tab2 VALUES(2, 'VISH')

1 rows affected
RUN CREATE INDEX ix1 ON tab2(c2) USING BTREE

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
RUN SELECT * FROM tab2
c1	c2
2	VISH
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
SELECT name,space_type,state FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE space_type = 'Undo';
name	space_type	state
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
RUN USE mysqlxplugin

0 rows affected
RUN DROP TABLE tab2

0 rows affected
RUN DROP UNDO TABLESPACE undo_003

0 rows affected
RUN DROP SCHEMA if EXISTS mysqlxplugin

0 rows affected
RUN DROP USER insertsqluser@localhost

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
