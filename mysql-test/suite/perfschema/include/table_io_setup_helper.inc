# Performance schema test template

# How to use this script in a test case
# =====================================
#
# The general table io test template is as follows
#
# --source include/not_embedded.inc
# --source include/have_perfschema.inc
# --source ../include/table_io_setup_helper.inc
# ... more setup scripts as needed ...
# update performance_schema.setup_consumers set enabled='YES';
# ... test payload here ...
# ... optionally, add this insert between statements
# ... to make the final output more readable
# insert into test.marker set a=1;
# ... more test payload here ...
# ... optionaly, add the following line (by default, only "test" is dumped) ...
# let $schema_to_dump="db1", "db2", "db3";
# --source ../include/table_io_result_helper.inc
# Optional: Repeat several times
#    update performance_schema.setup_consumers set enabled='YES';
#    ... test payload here ...
#    --source ../include/table_io_result_helper.inc
# ... cleanup
# --source ../include/table_io_cleanup_helper.inc
#
# (end of template)

# Setup

--disable_warnings
drop table if exists test.marker;
--enable_warnings

# To be used in the test payload,
# insert into marker makes the test output easier to read,
# to separate table io events between statements.
create table test.marker(a int);

update performance_schema.setup_consumers set enabled='NO';

update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES'
  where name like "wait/io/table/%";

truncate table performance_schema.events_waits_history_long;

# Reset lost counters to a known state
flush status;

# Make sure there is room to instrument tables from this test.
flush tables;

# By default, dump table io only for test.
# A test can overide this
let $schema_to_dump="test";

