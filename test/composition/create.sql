create database if not exists testfk;
drop table if exists testfk.fkdifferentdb;

use test;
# drop tables in proper order to avoid foreign key constraint violations
drop table if exists lineitem;
drop table if exists shoppingcart;
drop table if exists customerdiscount;
drop table if exists customer;
drop table if exists item;
drop table if exists discount;

# Customer has a oneToZeroOrOne relationship to ShoppingCart
# Customer has a manyToMany relationship to Discount via table customerdiscount
create table customer (
  id int not null,
  firstname varchar(32),
  lastname varchar(32),
  primary key(id)
);

# ShoppingCart has a oneToOne relationship to Customer
# ShoppingCart has a oneToMany relationship to LineItem
create table shoppingcart (
  id int not null,
  customerid int not null,
  created datetime not null default now(),
  foreign key fkcustomerid(customerid) references customer(id),
  primary key(id)
);

# Item has a oneToMany relationship to LineItem
create table item(
  id int not null,
  description varchar(99),
  primary key(id)
);

# LineItem has a manyToOne relationship to ShoppingCart
# LineItem has a manyToOne relationship to Item
create table lineitem (
  line int not null,
  shoppingcartid int not null,
  quantity int not null,
  itemid int not null,
  primary key(shoppingcartid, line),
  foreign key fkitemid(itemid) references item(id),
  foreign key fkshoppingcartid(shoppingcartid) references shoppingcart(id)
);

# Discount has a manyToMany relationship to Customer via table customerdiscount
create table discount(
  id int not null,
  description varchar(32) not null,
  percent int not null,
  primary key(id)
);

# customerdiscount is a simple join table
create table customerdiscount(
  customerid int not null,
  discountid int not null,
  foreign key fkcustomerid(customerid) references customer(id),
  foreign key fkdiscountid(discountid) references discount(id),
  primary key (customerid, discountid)
);

insert into customer(id, firstname, lastname) values (100, 'Craig', 'Walton');
insert into customer(id, firstname, lastname) values (101, 'Sam', 'Burton');
insert into customer(id, firstname, lastname) values (102, 'Wal', 'Greeton');
insert into customer(id, firstname, lastname) values (103, 'Burn', 'Sexton');

insert into shoppingcart(id, customerid) values(1000, 100);
insert into shoppingcart(id, customerid) values(1002, 102);
insert into shoppingcart(id, customerid) values(1003, 103);

insert into item(id, description) values(10000, 'toothpaste');
insert into item(id, description) values(10001, 'razor blade 10 pack');
insert into item(id, description) values(10002, 'deodorant');
insert into item(id, description) values(10003, 'hatchet');
insert into item(id, description) values(10004, 'weed-b-gon');
insert into item(id, description) values(10005, 'cola 24 pack');
insert into item(id, description) values(10006, 'diet cola 24 pack');
insert into item(id, description) values(10007, 'diet root beer 12 pack');
insert into item(id, description) values(10008, 'whole wheat bread');
insert into item(id, description) values(10009, 'raisin bran');
insert into item(id, description) values(10010, 'milk gallon');
insert into item(id, description) values(10011, 'half and half');
insert into item(id, description) values(10012, 'tongue depressor');
insert into item(id, description) values(10013, 'smelling salt');
insert into item(id, description) values(10014, 'holy bible');

insert into lineitem(line, shoppingcartid, quantity, itemid) values(0, 1000, 1, 10000);
insert into lineitem(line, shoppingcartid, quantity, itemid) values(1, 1000, 5, 10014);
insert into lineitem(line, shoppingcartid, quantity, itemid) values(2, 1000, 2, 10011);
insert into lineitem(line, shoppingcartid, quantity, itemid) values(0, 1002, 10, 10008);
insert into lineitem(line, shoppingcartid, quantity, itemid) values(1, 1002, 4, 10010);
insert into lineitem(line, shoppingcartid, quantity, itemid) values(2, 1002, 40, 10002);
insert into lineitem(line, shoppingcartid, quantity, itemid) values(3, 1002, 100, 10011);
insert into lineitem(line, shoppingcartid, quantity, itemid) values(4, 1002, 1, 10013);
insert into lineitem(line, shoppingcartid, quantity, itemid) values(5, 1002, 8, 10005);

insert into discount(id, description, percent) values(0, 'new customer', 10);
insert into discount(id, description, percent) values(1, 'good customer', 15);
insert into discount(id, description, percent) values(2, 'spring sale', 10);
insert into discount(id, description, percent) values(3, 'internet special', 20);
insert into discount(id, description, percent) values(4, 'closeout', 50);

insert into customerdiscount(customerid, discountid) values(100, 0);
insert into customerdiscount(customerid, discountid) values(101, 1);
insert into customerdiscount(customerid, discountid) values(101, 3);
insert into customerdiscount(customerid, discountid) values(101, 4);
insert into customerdiscount(customerid, discountid) values(102, 2);
insert into customerdiscount(customerid, discountid) values(103, 3);

create table testfk.fkdifferentdb (
  id int not null primary key,
  foreign key fkcustomerid(id) references test.customer(id)
);

