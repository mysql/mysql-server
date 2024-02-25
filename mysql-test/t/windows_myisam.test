# Windows-specific tests
--source include/windows.inc

--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Bug17489: ailed to put data file in custom directory use "data directory" option
#

--disable_warnings
drop table if exists t1;
--enable_warnings
CREATE TABLE t1 ( `ID` int(6) ) data directory 'c:/tmp/' index directory 'c:/tmp/' engine=MyISAM;
drop table t1;

# End of 4.1 tests

