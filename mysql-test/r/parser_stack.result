use test;
SELECT
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
1
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
;
1
1
prepare stmt from
"
SELECT
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
1
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
"
;
execute stmt;
1
1
drop view if exists view_overflow;
CREATE VIEW view_overflow AS
SELECT
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
1
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
;
SELECT * from view_overflow;
1
1
drop view view_overflow;
drop procedure if exists proc_overflow;
CREATE PROCEDURE proc_overflow()
BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
select 1;
select 2;
select 3;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END $$
call proc_overflow();
1
1
2
2
3
3
drop procedure proc_overflow;
drop function if exists func_overflow;
create function func_overflow() returns int
BEGIN
DECLARE x int default 0;
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
SET x=x+1;
SET x=x+2;
SET x=x+3;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
return x;
END $$
select func_overflow();
func_overflow()
6
drop function func_overflow;
drop table if exists table_overflow;
create table table_overflow(a int, b int);
create trigger trigger_overflow before insert on table_overflow
for each row
BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN BEGIN
SET NEW.b := NEW.a;
SET NEW.b := NEW.b + 1;
SET NEW.b := NEW.b + 2;
SET NEW.b := NEW.b + 3;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;  END;
END $$
insert into table_overflow set a=10;
insert into table_overflow set a=20;
select * from table_overflow;
a	b
10	16
20	26
drop table table_overflow;
drop procedure if exists proc_35577;
CREATE PROCEDURE proc_35577()
BEGIN
DECLARE z_done INT DEFAULT 0;
DECLARE t_done VARCHAR(5000);
outer_loop: LOOP
IF t_done=1  THEN
LEAVE outer_loop;
END IF;
inner_block:BEGIN
DECLARE z_done INT DEFAULT  0;
SET z_done = 0;
inner_loop: LOOP
IF z_done=1  THEN
LEAVE inner_loop;
END IF;
IF (t_done = 'a') THEN
IF (t_done <> 0) THEN
IF ( t_done > 0) THEN
IF (t_done = 'a') THEN
SET t_done = 'a';
ELSEIF (t_done = 'a') THEN
SET t_done = 'a';
ELSEIF(t_done = 'a') THEN
SET t_done = 'a';
ELSEIF(t_done = 'a') THEN
SET t_done = 'a';
ELSEIF(t_done = 'a') THEN
SET t_done = 'a';
ELSEIF(t_done = 'a') THEN
SET t_done = 'a';
ELSEIF(t_done = 'a') THEN
SET t_done = 'a';
ELSEIF(t_done = 'a') THEN
SET t_done = 'a';
END IF;
END IF;
END IF;
END IF;
END LOOP inner_loop;
END inner_block;
END LOOP outer_loop;
END $$
drop procedure proc_35577;
drop procedure if exists p_37269;
create procedure p_37269()
begin
declare done int default 0;
declare varb int default 0;
declare vara int default 0;
repeat
select now();
until done end repeat;
while varb do
select now();
begin
select now();
repeat
select now();
until done end repeat;
if vara then 
select now();
repeat
select now();
loop
select now();
end loop;
repeat
select now();
label1: while varb do
select now();
end while label1;
if vara then 
select now();
repeat
select now();
until done end repeat;
begin
select now();
while varb do
select now();
label1: while varb do
select now();
end while label1;
if vara then 
select now();
while varb do
select now();
loop
select now();
end loop;
repeat
select now();
loop
select now();
while varb do
select now();
end while;
repeat
select now();
label1: loop
select now();
if vara then 
select now();
end if;
end loop label1;
until done end repeat;
end loop;
until done end repeat;
end while;
end if;
end while;
end;
end if;
until done end repeat;
until done end repeat;
end if;
end;
end while;
end $$
drop procedure p_37269;
drop procedure if exists p_37228;
create procedure p_37228 ()
BEGIN
DECLARE v INT DEFAULT 123;
IF (v > 1) THEN SET v = 1; 
ELSEIF (v < 10) THEN SET v = 10;
ELSEIF (v < 11) THEN SET v = 11;
ELSEIF (v < 12) THEN SET v = 12;
ELSEIF (v < 13) THEN SET v = 13;
ELSEIF (v < 14) THEN SET v = 14;
ELSEIF (v < 15) THEN SET v = 15;
ELSEIF (v < 16) THEN SET v = 16;
ELSEIF (v < 17) THEN SET v = 17;
ELSEIF (v < 18) THEN SET v = 18;
ELSEIF (v < 19) THEN SET v = 19;
ELSEIF (v < 20) THEN SET v = 20;
ELSEIF (v < 21) THEN SET v = 21;
ELSEIF (v < 22) THEN SET v = 22;
ELSEIF (v < 23) THEN SET v = 23;
ELSEIF (v < 24) THEN SET v = 24;
ELSEIF (v < 25) THEN SET v = 25;
ELSEIF (v < 26) THEN SET v = 26;
ELSEIF (v < 27) THEN SET v = 27;
ELSEIF (v < 28) THEN SET v = 28;
ELSEIF (v < 29) THEN SET v = 29;
ELSEIF (v < 30) THEN SET v = 30;
ELSEIF (v < 31) THEN SET v = 31;
ELSEIF (v < 32) THEN SET v = 32;
ELSEIF (v < 33) THEN SET v = 33;
ELSEIF (v < 34) THEN SET v = 34;
ELSEIF (v < 35) THEN SET v = 35;
ELSEIF (v < 36) THEN SET v = 36;
ELSEIF (v < 37) THEN SET v = 37;
ELSEIF (v < 38) THEN SET v = 38;
ELSEIF (v < 39) THEN SET v = 39;
END IF;
END $$
drop procedure p_37228;
