use test;
drop table if exists temporaltypes;
create table if not exists temporaltypes (
  id int not null PRIMARY KEY,
  cYear year,
  cTimestamp timestamp NOT NULL,
  cNullableTimestamp timestamp NULL,
  cTime time,
  cDate date,
  cDatetime datetime,
  cDatetimeDefault datetime NOT NULL DEFAULT "1989-11-09 17:00:00"
);
