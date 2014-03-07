# This is pure myisam test, since autoinc behavior 
# in myisam is different from innodb.
--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Bug#20573 Strict mode auto-increment
#

let $type= 'MYISAM' ;
--source include/strict_autoinc.inc

# end of test
