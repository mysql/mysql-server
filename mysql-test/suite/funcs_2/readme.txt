funcs_2 suite
=============

Serge Kozlov 11/17/05
---------------------

Currently the suite covers testing all combination of charsets/collations
for MySQL 5.0 only. All cases separated by 4 test scenarios (by engines): 
 - innodb_charset.test;
 - memory_charset.test;
 - myisam_charset.test;
 - ndb_charset.test;
Note: if you use standard binary distributions or compile from source tree
without cluster support then ndb_charset.test will be skipped. Use 
BUILD/compile-*****-max shellscript for compilation with ndb support or
download MAX package.

Before running the suite under Windows/cygwin make sure that all files
inside it converted to unix text format.

Whole suite can be running by following command line:
./mysql-test-run.pl --suite=funcs_2
