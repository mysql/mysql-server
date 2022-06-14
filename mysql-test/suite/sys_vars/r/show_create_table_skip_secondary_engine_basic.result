#
# WL#13153: Don't dump SECONDARY TABLE if requested by the end user
#
# test: should be off by default
select @@session.show_create_table_skip_secondary_engine;
@@session.show_create_table_skip_secondary_engine
0
SET SESSION show_create_table_skip_secondary_engine=default;
# test: should be off by default
select @@session.show_create_table_skip_secondary_engine;
@@session.show_create_table_skip_secondary_engine
0
SET GLOBAL show_create_table_skip_secondary_engine=default;
ERROR HY000: Variable 'show_create_table_skip_secondary_engine' is a SESSION variable and can't be used with SET GLOBAL
SET PERSIST show_create_table_skip_secondary_engine=default;
ERROR HY000: Variable 'show_create_table_skip_secondary_engine' is a SESSION variable and can't be used with SET GLOBAL
SET PERSIST_ONLY show_create_table_skip_secondary_engine=default;
ERROR HY000: Variable 'show_create_table_skip_secondary_engine' is a SESSION variable and can't be used with SET GLOBAL
