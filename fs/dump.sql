# MySQL dump 8.12
#
# Host: localhost    Database: mysqlfs
#--------------------------------------------------------
# Server version	3.23.33

#
# Table structure for table 'functions'
#

CREATE TABLE functions (
  type enum('server','database','table','field','key') NOT NULL default 'server',
  name char(20) NOT NULL default '',
  sql char(128) NOT NULL default '',
  PRIMARY KEY (type,name)
) TYPE=MyISAM;

#
# Dumping data for table 'functions'
#

INSERT INTO functions VALUES ('server','uptime','SHOW STATUS like \'Uptime\'');
INSERT INTO functions VALUES ('server','version','SELECT VERSION()');
INSERT INTO functions VALUES ('table','count','SELECT COUNT(*) FROM `%table`');
INSERT INTO functions VALUES ('key','min','SELECT MIN(%key) FROM `%table`');
INSERT INTO functions VALUES ('key','max','SELECT MAX(%key) FROM `%table`');
INSERT INTO functions VALUES ('key','avg','SELECT AVG(%key) FROM `%table`');

