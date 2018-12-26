select * from t1 where b = '1969';
select * from t1 where c = '2017-03-21';
select * from t1 where d = '17:50:30';
select * from t1 where e = '2014-03-19 18:41:26';
select * from t1 where f = '1999-05-23 10:45:42';
select count(*) from t1
where b between '2000' and 2010;
select count(*) from t1
where c between '2000-01-01' and '2010-01-01';
select count(*) from t1
where d between '-20:30:40' and '20:30:40';
select count(*) from t1
where e between '2000-01-01 20:30:40' and '2010-01-01 20:30:40';
select count(*) from t1
where f between '2000-01-01 20:30:40' and '2010-01-01 20:30:40';
