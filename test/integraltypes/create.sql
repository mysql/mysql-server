use test;
drop table if exists integraltypes;
create table if not exists integraltypes (
  id int not null,
  ttinyint tinyint not null default 0,
  tsmallint smallint not null default 0,
  tmediumint mediumint not null default 0,
  tint int not null default 0,
  tbigint bigint not null default 0,
  
  unique key(tint),
  primary key(id)

) ENGINE=ndbcluster;

insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(0, 0, 0, 0, 0, 0);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(1, 1, 1, 1, 1, 1);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(2, 2, 2, 2, 2, 2);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(3, 3, 3, 3, 3, 3);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(4, 4, 4, 4, 4, 4);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(5, 5, 5, 5, 5, 5);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(6, 6, 6, 6, 6, 6);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(7, 7, 7, 7, 7, 7);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(8, 8, 8, 8, 8, 8);
insert into integraltypes (id, ttinyint, tsmallint, tmediumint, tint, tbigint) values(9, 9, 9, 9, 9, 9);
