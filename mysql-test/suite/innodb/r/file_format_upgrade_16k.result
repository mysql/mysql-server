#
# Bug#20691930 5.7.6 CRASH WHEN RUNNING MYSQL_UPGRADE
# AFTER BINARY UPGRADE
#
CREATE TABLE t (a int primary key, b char(255) not null default '', index(b))
ENGINE=InnoDB;
INSERT t(a) VALUES(1),(2),(3),(4),(5),(6),(7),(8);
# restart
INSERT t(a) SELECT a+8 FROM t;
INSERT t(a) SELECT a+16 FROM t;
INSERT t(a) SELECT a+32 FROM t;
INSERT t(a) SELECT a+64 FROM t;
# Page types: 00080005000345bd45bf45bf
# restart
DROP TABLE t;
