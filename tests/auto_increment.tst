#
# Test of auto_increment
#
# run this program with mysql -vvf test < this file

drop table if exists auto_incr_test,auto_incr_test2 ;

create table auto_incr_test (id int not null auto_increment, name char(40), timestamp timestamp, primary key (id)) ;

insert into auto_incr_test (name) values ("first record");
insert into auto_incr_test values (last_insert_id()+1,"second record",null);
insert into auto_incr_test (id,name) values (10,"tenth record");
insert into auto_incr_test values (0,"eleventh record",null);
insert into auto_incr_test values (last_insert_id()+1,"12","1997-01-01");
insert into auto_incr_test values (12,"this will not work",NULL);
replace into auto_incr_test values (12,"twelfth record",NULL);

select * from auto_incr_test ;

create table auto_incr_test2 (id int not null auto_increment, name char(40), primary key (id)) ;
insert into auto_incr_test2 select NULL,name from auto_incr_test;
insert into auto_incr_test2 select id,name from auto_incr_test;
replace into auto_incr_test2 select id,name from auto_incr_test;

select * from auto_incr_test2 ;

drop table auto_incr_test,auto_incr_test2;
