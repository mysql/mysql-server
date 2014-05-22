
#
# Bug #29015: Stack overflow in processing temporary table name when tmpdir path
#             is long
#

create view v1 as select table_name from information_schema.tables;
drop view v1;

--echo End of 5.0 tests
