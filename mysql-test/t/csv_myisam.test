--source include/force_myisam_default.inc
--source include/have_myisam.inc
#
# Test for the CSV engine
#
### Test is  Being disabled to be executed with Query Cache Enabled until BUG#11757567 is fixed
#
# Simple select test

#
#
# Bug #21328 mysqld issues warnings on ALTER CSV table to MyISAM
#
CREATE TABLE `bug21328` (
  `col1` int(11) NOT NULL,
  `col2` int(11) NOT NULL,
  `col3` int(11) NOT NULL
) ENGINE=CSV;
insert into bug21328 values (1,0,0);
alter table bug21328 engine=myisam;
drop table bug21328;
