use test;
drop function if exists a;
drop function if exists st_x;
drop function if exists st_y;
create function a() returns int
return 1;
create function st_x() returns int
return 2;
Warnings:
Note	1585	This function 'st_x' has the same name as a native function
create function st_y() returns int
return 3;
Warnings:
Note	1585	This function 'st_y' has the same name as a native function
select a();
a()
1
select st_x();
ERROR 42000: Incorrect parameter count in the call to native function 'st_x'
select st_y();
ERROR 42000: Incorrect parameter count in the call to native function 'st_y'
select st_x(ST_PointFromText("POINT(10 20)")), st_y(ST_PointFromText("POINT(10 20)"));
st_x(ST_PointFromText("POINT(10 20)"))	st_y(ST_PointFromText("POINT(10 20)"))
10	20
select test.a(), test.st_x(), test.st_y();
test.a()	test.st_x()	test.st_y()
1	2	3
drop function a;
drop function st_x;
drop function st_y;
