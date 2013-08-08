#
# clean up if set-up created in
# suite/perfschema/include/program_nested_setup.inc
#

DROP PROCEDURE c4;
DROP PROCEDURE c3;
DROP PROCEDURE c2;
DROP PROCEDURE c1;
DROP PROCEDURE inc;
DROP PROCEDURE inc2;
DROP PROCEDURE iotest;
DROP FUNCTION mul;
DROP FUNCTION inc;
DROP FUNCTION fac;
DROP FUNCTION fun;
DROP PROCEDURE ifac;
DROP TRIGGER trg;
DROP TABLE t1,t2;

DROP DATABASE nested_sp;
