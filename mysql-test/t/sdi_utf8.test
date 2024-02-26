--source include/force_myisam_default.inc
--source include/have_myisam.inc

let $MYSQLD_DATADIR = `select @@datadir`;
CREATE TABLE tæøå(i INT);
--replace_regex /_[0-9]+\.sdi/_XXX.sdi/
--list_files $MYSQLD_DATADIR/test
DROP TABLE tæøå;
