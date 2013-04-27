use test;

CREATE TABLE if not exists `towns` (
  `town` varchar(50) NOT NULL,
  `county` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`town`)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

