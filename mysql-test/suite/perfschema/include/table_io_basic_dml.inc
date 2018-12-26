# Tests for PERFORMANCE_SCHEMA table io
#
# Payload fragment to test table io for basic DML.
#
# $table_item = <schema>.<tablename> must be set before sourcing this script.
#

insert into marker set a = 1;
eval insert into $table_item set a = 'foo', b = 1;
insert into marker set a = 1;
eval insert into $table_item set a = 'foo', b = 2;
insert into marker set a = 1;
eval insert into $table_item set a = 'foo', b = 3;
insert into marker set a = 1;
eval select * from $table_item;
insert into marker set a = 1;
eval update $table_item set a = 'bar';
insert into marker set a = 1;
eval select * from $table_item limit 2;
insert into marker set a = 1;
eval delete from $table_item where b = 3;
insert into marker set a = 1;
# This may record a fetch for "deleted" records
eval select * from $table_item;
insert into marker set a = 1;
eval optimize table $table_item;
insert into marker set a = 1;
# Same data after optimize
eval select * from $table_item;
insert into marker set a = 1;
# truncate will fail for views
--disable_abort_on_error
eval truncate table $table_item;
--enable_abort_on_error
insert into marker set a = 1;

