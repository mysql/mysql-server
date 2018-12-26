# This tests the functionality of the fulltext search of Myisam engine
# The implementation of the fulltext search is different in InnoDB engine
# All tests are required to run with Myisam.
# Hence MTR starts mysqld with MyISAM as default
--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Test for bug by voi@ims.at
#

--disable_warnings
drop table if exists test;
--enable_warnings
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE test (
  gnr INT(10) UNSIGNED NOT NULL AUTO_INCREMENT,
  url VARCHAR(80) DEFAULT '' NOT NULL,
  shortdesc VARCHAR(200) DEFAULT '' NOT NULL,
  longdesc text DEFAULT '' NOT NULL,
  description VARCHAR(80) DEFAULT '' NOT NULL,
  name VARCHAR(80) DEFAULT '' NOT NULL,
  FULLTEXT(url,description,shortdesc,longdesc),
  PRIMARY KEY(gnr)
);
SET sql_mode = default;
insert into test (url,shortdesc,longdesc,description,name) VALUES 
("http:/test.at", "kurz", "lang","desc", "name");
insert into test (url,shortdesc,longdesc,description,name) VALUES 
("http:/test.at", "kurz", "","desc", "name");
update test set url='test', description='ddd', name='nam' where gnr=2;
update test set url='test', shortdesc='ggg', longdesc='mmm', 
description='ddd', name='nam' where gnr=2;
check table test;
drop table test;

# End of 4.1 tests
