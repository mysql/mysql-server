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
  unique key(ttinyint) using hash,
  key (tsmallint),
  primary key(id)

);

create table if not exists numerictypes (
  id int NOT NULL,
  tposint int unsigned,
  tfloat float NOT NULL,
  tposfloat float unsigned,
  tdouble double,
  tnumber decimal(11,3),
  tposnumber decimal(11,3) unsigned
);

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

insert into numerictypes values (1, 1, -1.0, 1.0, 1.0, -1.001, 1.001);
insert into numerictypes values (2, 2, -2.0, 2.0, 2.0, -2.020, 2.020);
insert into numerictypes values (3, 3, -3.0, 3.0, 3.0, -3.300, 3.300);
insert into numerictypes values (4, 4, -4.0, 4.0, 4.0, NULL, NULL);
insert into numerictypes values (5, 5, -5.0, 5.0, 5.0, NULL, NULL);

