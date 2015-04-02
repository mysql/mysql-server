use test;
DROP TABLE if EXISTS towns;
DROP TABLE if EXISTS a;

CREATE TABLE if not exists `towns` (
  `town` varchar(50) NOT NULL,
  `county` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`town`)
);

CREATE TABLE if not exists a (
  id int not null primary key,
  cint int
);
