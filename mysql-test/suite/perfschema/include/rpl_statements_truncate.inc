# Tests for the performance schema

# ==========================================
# HELPER include/rpl/truncate_statements.inc
# ==========================================

--disable_query_log
update performance_schema.setup_instruments set enabled='no', timed='no' where name like '%statement/%';
truncate table performance_schema.events_waits_history_long;
truncate table performance_schema.events_statements_summary_by_digest;
truncate table performance_schema.events_statements_history;
truncate table performance_schema.events_statements_history_long;
update performance_schema.setup_instruments set enabled='yes', timed='yes' where name like '%statement/%';
--enable_query_log

