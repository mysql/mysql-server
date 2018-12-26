create table bug47777(c2 linestring not null) engine=innodb;
insert into bug47777 values (ST_geomfromtext('linestring(1 2,3 4,5 6,7 8,9 10)'));
select count(*) from bug47777 where c2 =ST_geomfromtext('linestring(1 2,3 4,5 6,7 8,9 10)');
count(*)
1
update bug47777 set c2=ST_GeomFromText('POINT(1 1)');
ERROR 22003: Cannot get geometry object from data you send to the GEOMETRY field
select count(*) from bug47777 where c2 =ST_geomfromtext('linestring(1 2,3 4,5 6,7 8,9 10)');
count(*)
1
select count(*) from bug47777 where c2 = ST_GeomFromText('POINT(1 1)');
count(*)
0
drop table bug47777;
