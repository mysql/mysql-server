SET transaction_isolation = 'READ-COMMITTED';
CREATE TABLE bug49164 (a INT, b BIGINT, c TINYINT, PRIMARY KEY (a, b))
ENGINE=InnoDB;
insert into bug49164 values (1,1,1), (2,2,2), (3,3,3);
begin;
update bug49164 set c=7;
select * from bug49164;
a	b	c
1	1	7
2	2	7
3	3	7
rollback;
select * from bug49164;
a	b	c
1	1	1
2	2	2
3	3	3
begin;
update bug49164 set c=7;
SET transaction_isolation = 'READ-COMMITTED';
begin;
select * from bug49164;
a	b	c
1	1	1
2	2	2
3	3	3
commit;
begin;
update bug49164 set c=6 where a=1 and b=1;
rollback;
select * from bug49164;
a	b	c
1	1	1
2	2	2
3	3	3
commit;
select * from bug49164;
a	b	c
1	1	6
2	2	2
3	3	3
drop table bug49164;
