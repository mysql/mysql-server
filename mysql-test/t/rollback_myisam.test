# MyISAM is necessary to check that rollback does not work for it.
--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# This test should fail as MyISAM doesn't have rollback
#

--disable_warnings
drop table if exists t1;
--enable_warnings
# PS doesn't work with BEGIN ... ROLLBACK
--disable_ps_protocol

create table t1 (n int not null primary key) engine=myisam;
begin work;
insert into t1 values (4);
insert into t1 values (5);
rollback;
show warnings;
show errors;
select @@warning_count,@@error_count;
select * from t1;
show warnings;
select @@warning_count;
drop table t1;
