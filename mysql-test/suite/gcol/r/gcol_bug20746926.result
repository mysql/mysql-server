#Bug #20746926: GENERATED COLUMNS: INVALID READ OF THD WHEN WARNINGS
#
# Testing cmp_item_datetime
set sql_mode='';
create table t1 (
a date not null,
b mediumtext generated always as ((a not in (a,a))) virtual,
c timestamp generated always as ((a not in (b,b))) stored not null
);
insert t1(a) values(7777777777);
Warnings:
Warning	1265	Data truncated for column 'a' at row 1
Warning	1292	Incorrect date value: '0' for column 'a' at row 1
show warnings;
Level	Code	Message
Warning	1265	Data truncated for column 'a' at row 1
Warning	1292	Incorrect date value: '0' for column 'a' at row 1
set sql_mode='';
insert t1(a) values(6666666666);
Warnings:
Warning	1265	Data truncated for column 'a' at row 1
Warning	1292	Incorrect date value: '0' for column 'a' at row 1
show warnings;
Level	Code	Message
Warning	1265	Data truncated for column 'a' at row 1
Warning	1292	Incorrect date value: '0' for column 'a' at row 1
drop table t1;
