################################################################################
# inc/vcol_trigger_sp.inc                                                      #
#                                                                              #
# Purpose:                                                                     #
#  Testing triggers, stored procedures and functions                           #
#  defined on tables with virtual columns.                                     #
#                                                                              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-04                                                    #
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

create table t1 (a int, 
                 b int as (a/10),
		 c int as (a/10) persistent);

create table t2 (a timestamp);

delimiter |;

create trigger trg1 before insert on t1 for each row
begin
  if (new.b < 10) then
    set new.a:= 100;
    set new.b:= 9;
    set new.c:= 9;
  end if;

  if (new.c > 50) then
    set new.a:= 500; 
  end if;
end|

create trigger trg2 after insert on t1 for each row
begin
  if (new.b >= 60) then
    insert into t2 values (now()); 
  end if;
end|

create function f1()
returns int
begin
  declare sum1 int default '0';
  declare cur1 cursor for select sum(b) from t1;
  open cur1;
  fetch cur1 into sum1;
  close cur1;
  return sum1;
end|

delimiter ;|

set sql_warnings = 1;

insert into t1 (a) values (200);
select * from t1;
select * from t2;

insert into t1 (a) values (10);
select * from t1;
select * from t2;

insert into t1 (a) values (600);
select * from t1;
--replace_column 1 <timestamp>
select * from t2;

select f1();

set sql_warnings = 0;

drop trigger trg1;
drop trigger trg2;
drop table t2;

delimiter |;

create procedure p1()
begin
  declare i int default '0';
  create table t2 like t1;
  insert into t2 (a) values (100), (200);
  begin
    declare cur1 cursor for select sum(c) from t2;
    open cur1;
    fetch cur1 into i;
    close cur1;
    if (i=30) then
      insert into t1 values (300,default,default);
    end if;
  end;
end|

delimiter ;|

delete from t1;

call p1();

select * from t2;
select * from t1;

drop table t1,t2;
drop procedure p1;

--echo #
--echo # Bug mdev-3845: values of virtual columns are not computed for triggers
--echo #

CREATE TABLE t1 (
  a INTEGER UNSIGNED NULL DEFAULT NULL,
  b INTEGER UNSIGNED GENERATED ALWAYS AS (a) VIRTUAL
);

CREATE TABLE t2 (c INTEGER UNSIGNED NOT NULL);

DELIMITER |;

CREATE TRIGGER t1_ins_aft
  AFTER INSERT
  ON t1
  FOR EACH ROW
BEGIN
  INSERT INTO t2 (c) VALUES (NEW.b);
END |

CREATE TRIGGER t1_del_bef
  BEFORE DELETE
  ON t1
  FOR EACH ROW
BEGIN
  INSERT INTO t2 (c) VALUES (OLD.b);
END |

DELIMITER ;|

INSERT INTO t1 (a) VALUES (1), (2), (3);
SELECT * FROM t2;
DELETE FROM t1;
SELECT * FROM t2;

DROP TRIGGER t1_ins_aft;
DROP TRIGGER t1_del_bef;
DROP TABLE t1,t2;

