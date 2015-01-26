drop table if exists t1;
create table t1(City VARCHAR(30),Location geometry);
insert into t1 values("Paris",ST_GeomFromText('POINT(2.33 48.87)'));
select City from t1 where (select
MBRintersects(ST_GeomFromText(ST_AsText(Location)),ST_GeomFromText('Polygon((2 50, 2.5
50, 2.5 47, 2 47, 2 50))'))=0);
City
drop table t1;
