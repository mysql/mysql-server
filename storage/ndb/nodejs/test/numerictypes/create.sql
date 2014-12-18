use test;
drop table if exists integraltypes;
create table integraltypes (
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

drop table if exists numerictypes;
create table numerictypes (
  id int NOT NULL primary key,
  tfloat float NOT NULL,
  tdouble double,
  tnumber decimal(11,3),
  tposint int unsigned,
  tposnumber decimal(11,3) unsigned,
  tposbigint bigint unsigned
);

insert into numerictypes values(108, 108, 108, 108, 108, 108, 108);
insert into numerictypes values(109, 109, 109, 109, 109, 109, 109);
insert into numerictypes values(110, 110, 110, 110, 110, 110, 110);
insert into numerictypes values(101, 111, 111, 111, 111, 111, 111);
insert into numerictypes values(112, 112, 112, 112, 112, 112, 112);
insert into numerictypes values(113, 113, 113, 113, 113, 113, 113);
insert into numerictypes values(114, 114, 114, 114, 114, 114, 114);
insert into numerictypes values(115, 115, 115, 115, 115, 115, 115);
insert into numerictypes values(116, 116, 116, 116, 116, 116, 116);
insert into numerictypes values(117, 117, 117, 117, 117, 117, 117);
