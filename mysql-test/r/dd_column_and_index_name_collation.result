set names latin1;
CREATE TABLE t1 (`a` INT, `b` INT, `C` INT,
`D` INT, `á` INT, `é` INT,
`Á` INT, `Å` INT, `ê` INT,
KEY `a` ( `a` ), KEY `b` ( `b` ), KEY `C` ( `C` ),
KEY `D` ( `D` ), KEY `á` ( `á` ), KEY `é` ( `é` ),
KEY `Á` ( `Á` ), KEY `Å` ( `Å` ), KEY `ê` ( `ê` ));
CREATE TABLE t2 (`ê` INT, `Å` INT, `Á` INT,
`é` INT, `á` INT, `D` INT,
`C` INT, `b` INT, `a` INT,
KEY `ê` ( `ê` ), KEY `Á` ( `Á` ), KEY `Å` ( `Å` ),
KEY `é` ( `é` ), KEY `á` ( `á` ), KEY `D` ( `D` ),
KEY `C` ( `C` ), KEY `b` ( `b` ), KEY `a` ( `a` ));
SELECT table_name, column_name FROM INFORMATION_SCHEMA.COLUMNS WHERE table_name= 't1' ORDER BY column_name;
TABLE_NAME	COLUMN_NAME
t1	a
t1	b
t1	C
t1	D
t1	Á
t1	á
t1	é
t1	ê
t1	Å
SELECT table_name, column_name FROM INFORMATION_SCHEMA.COLUMNS WHERE table_name= 't2' ORDER BY column_name;
TABLE_NAME	COLUMN_NAME
t2	a
t2	b
t2	C
t2	D
t2	Á
t2	á
t2	é
t2	ê
t2	Å
SELECT table_name, index_name, column_name FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_NAME= 't1' ORDER BY index_name;
TABLE_NAME	INDEX_NAME	COLUMN_NAME
t1	a	a
t1	b	b
t1	C	C
t1	D	D
t1	Á	Á
t1	á	á
t1	é	é
t1	ê	ê
t1	Å	Å
SELECT table_name, index_name, column_name FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_NAME= 't2' ORDER BY index_name;
TABLE_NAME	INDEX_NAME	COLUMN_NAME
t2	a	a
t2	b	b
t2	C	C
t2	D	D
t2	Á	Á
t2	á	á
t2	é	é
t2	ê	ê
t2	Å	Å
DROP TABLE t2, t1;
