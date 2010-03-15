################################################################################
# t/partition_blocked_sql_funcs_main.inc                                       #
#                                                                              #
# Purpose:                                                                     #
#  Tests around sql functions                                                  #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: HH                                                          #
# Original Date: 2006-11-22                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--echo -------------------------------------------------------------------------
--echo ---   All SQL functions should be rejected, otherwise BUG (see 18198)
--echo -------------------------------------------------------------------------

let $sqlfunc = ascii(col1);
let $valsqlfunc = ascii('a');
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = ord(col1);
let $valsqlfunc = ord('a');
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = greatest(col1,15);
let $valsqlfunc = greatest(1,15);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = isnull(col1);
let $valsqlfunc = isnull(15);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = least(col1,15);
let $valsqlfunc = least(15,30);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = case when col1>15 then 20 else 10 end;
let $valsqlfunc = case when 1>30 then 20 else 15 end;
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = ifnull(col1,30);
let $valsqlfunc = ifnull(1,30);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = nullif(col1,30);
let $valsqlfunc = nullif(1,30);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = bit_length(col1);
let $valsqlfunc = bit_length(255);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = char_length(col1);
let $valsqlfunc = char_length('a');
#let $coltype = int;
#--source suite/parts/inc/partition_blocked_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = character_length(col1);
let $valsqlfunc = character_length('a');
let $coltype = char(30)
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = find_in_set(col1,'1,2,3,4,5,6,7,8,9');
let $valsqlfunc = find_in_set('i','a,b,c,d,e,f,g,h,i');
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = instr(col1,'acb');
let $valsqlfunc = instr('i','a,b,c,d,e,f,g,h,i');
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = length(col1);
let $valsqlfunc = length('a,b,c,d,e,f,g,h,i');
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = locate('a',col1);
let $valsqlfunc = locate('i','a,b,c,d,e,f,g,h,i');
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = octet_length(col1);
let $valsqlfunc = octet_length('a,b,c,d,e,f,g,h,i');
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = position('a' in col1);
let $valsqlfunc = position('i' in 'a,b,c,d,e,f,g,h,i');
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = strcmp(col1,'acb');
let $valsqlfunc = strcmp('i','a,b,c,d,e,f,g,h,i');
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = crc32(col1);
let $valsqlfunc = crc32('a,b,c,d,e,f,g,h,i');
let $coltype = char(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = round(col1);
let $valsqlfunc = round(15);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = sign(col1);
let $valsqlfunc = sign(123);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = period_add(col1,5);
let $valsqlfunc = period_add(9804,5);
let $coltype = datetime;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = period_diff(col1,col2);
let $valsqlfunc = period_diff(9809,199907);
let $coltype = datetime,col2 datetime;
--source suite/parts/inc/partition_blocked_sql_funcs.inc
let $coltype = int,col2 int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = timestampdiff(day,5,col1);
let $valsqlfunc = timestampdiff(YEAR,'2002-05-01','2001-01-01');
let $coltype = datetime;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

################################################################################
# After the fix for bug #42849 the server behavior does not fit into this test's
# architecture: for UNIX_TIMESTAMP() some of the queries in 
# suite/parts/inc/partition_blocked_sql_funcs.inc will fail with a different
# error (ER_WRONG_EXPR_IN_PARTITION_FUNC_ERROR) and some will succeed where
################################################################################
#let $sqlfunc = unix_timestamp(col1);
#let $valsqlfunc = unix_timestamp ('2002-05-01');
#let $coltype = date;
#--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = week(col1);
let $valsqlfunc = week('2002-05-01');
let $coltype = datetime;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = weekofyear(col1);
let $valsqlfunc = weekofyear('2002-05-01');
let $coltype = datetime;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = cast(col1 as signed);
let $valsqlfunc = cast(123 as signed);
let $coltype = varchar(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = convert(col1,unsigned);
let $valsqlfunc = convert(123,unsigned);
let $coltype = varchar(30);
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = col1 | 20;
let $valsqlfunc = 10 | 20;
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = col1 & 20;
let $valsqlfunc = 10 & 20;
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = col1 ^ 20;
let $valsqlfunc = 10 ^ 20;
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = col1 << 20;
let $valsqlfunc = 10 << 20;
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = col1 >> 20;
let $valsqlfunc = 10 >> 20;
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = ~col1;
let $valsqlfunc = ~20;
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = bit_count(col1);
let $valsqlfunc = bit_count(20);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

let $sqlfunc = inet_aton(col1);
let $valsqlfunc = inet_aton('192.168.1.1');
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc

set @var =20;
let $sqlfunc = bit_length(col1)+@var-@var;
let $valsqlfunc = bit_length(20)+@var-@var;
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc


delimiter //;
create function getmaxsigned_t1(col int) returns int
begin
  declare done int default 0;
  declare v4 int;
  declare max int;
  declare cur1 cursor for 
     select col from t1; 
  declare continue handler for sqlstate '01000' set done = 1;
  declare continue handler for sqlstate '02000' set done = 1;
  open cur1;
  set max = 0;
  fetch cur1 into v4;
  wl_loop: WHILE NOT done DO
     fetch cur1 into v4;
     IF v4 > max
     then set max = v4;
     END IF;
  END WHILE wl_loop;
  close cur1;
  return max;
 end//
delimiter ;//

let $sqlfunc = getmaxsigned_t1(col1);
let $valsqlfunc = getmaxsigned(10);
let $coltype = int;
--source suite/parts/inc/partition_blocked_sql_funcs.inc
drop function if exists getmaxsigned_t1;
