create table bug40565(value decimal(4,2)) engine=innodb;
insert into bug40565 values (1), (null);
update bug40565 set value=NULL;
affected rows: 1
info: Rows matched: 2  Changed: 1  Warnings: 0
update bug40565 set value=NULL;
affected rows: 0
info: Rows matched: 2  Changed: 0  Warnings: 0
drop table bug40565;
