use test;
# drop tables in proper order to avoid foreign key constraint violations
drop table if exists testfk.fkdifferentdb;
drop table if exists lineitem;
drop table if exists shoppingcart;
drop table if exists customerdiscount;
drop table if exists customer;
drop table if exists item;
drop table if exists discount;
