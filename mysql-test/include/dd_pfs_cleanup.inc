#
# Remove the schema and table used for storing the DD PFS memory events.
# Typically called once at the end of an MTR test.
#
--disable_query_log
DROP TABLE dd_pfs.mem_events;
DROP SCHEMA dd_pfs;
--enable_query_log
