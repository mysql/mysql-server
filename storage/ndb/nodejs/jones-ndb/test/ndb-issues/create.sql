-- Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

use test;
DROP TABLE if EXISTS towns2;

CREATE TABLE `towns2` (
  `town` varchar(50) NOT NULL,
  `county` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`town`)
);

