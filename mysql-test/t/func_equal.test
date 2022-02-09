
# Initialise
--disable_warnings
drop table if exists t1,t2;
--enable_warnings

#
# Testing of the <=> operator
#

#
# First some simple tests
#

select 0<=>0,0.0<=>0.0,0E0=0E0,"A"<=>"A",NULL<=>NULL;
select 1<=>0,0<=>NULL,NULL<=>0;
select 1.0<=>0.0,0.0<=>NULL,NULL<=>0.0;
select "A"<=>"B","A"<=>NULL,NULL<=>"A";
select 0<=>0.0, 0.0<=>0E0, 0E0<=>"0", 10.0<=>1E1, 10<=>10.0, 10<=>1E1;
select 1.0<=>0E1,10<=>NULL,NULL<=>0.0, NULL<=>0E0;

#
# Test with tables
#

create table t1 (id int, value int);
create table t2 (id int, value int);

insert into t1 values (1,null);
insert into t2 values (1,null);

select t1.*, t2.*, t1.value<=>t2.value from t1, t2 where t1.id=t2.id and t1.id=1;
select * from t1 where id <=>id;
select * from t1 where value <=> value;
select * from t1 where id <=> value or value<=>id;
drop table t1,t2;

#
# Bug #12612: quoted bigint unsigned value and the use of "in" in where clause
#
create table t1 (a bigint unsigned);
insert into t1 values (4828532208463511553);
select * from t1 where a = '4828532208463511553';
select * from t1 where a in ('4828532208463511553');
drop table t1;

# End of 4.1 tests

--echo # 33790919: Null-safe compare works incorrectly with TIMESTAMP in trigger

CREATE TABLE t1(id INT, ts TIMESTAMP NULL DEFAULT NULL);
INSERT INTO t1 VALUES (1, "2022-01-01"), (2, NULL), (NULL, "2022-01-01");

delimiter //;
CREATE TRIGGER tr_bi_check_uniqueness_with_nulls
BEFORE INSERT ON t1 FOR EACH ROW
BEGIN
  IF EXISTS (SELECT * FROM t1 WHERE t1.id <=> NEW.id AND t1.ts <=> NEW.ts) THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Duplicated values not allowed.';
  END IF;
END
//
delimiter ;//
--error ER_SIGNAL_EXCEPTION
INSERT INTO t1 VALUES (1,'2022-01-01');
--error ER_SIGNAL_EXCEPTION
INSERT INTO t1 VALUES (2, NULL);
--error ER_SIGNAL_EXCEPTION
INSERT INTO t1 VALUES (NULL,'2022-01-01');

SELECT * FROM t1;

DROP TABLE t1;
