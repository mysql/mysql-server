# Print if the mysql.ndb_schema table is upgraded or not
--source include/have_debug.inc

# Query the mysql.tables DD table to check of schema_op_id existence
# If the column exist, then the table is upgraded.

--disable_query_log
set GLOBAL debug="+d,skip_dd_table_access_check";
select (if (count(*) = 1, "Yes", "No"))
  as "Is 'mysql.ndb_schema' table upgraded?"
from mysql.tables t, mysql.columns c
where t.name="ndb_schema"
  and t.id=c.table_id
  and c.name="schema_op_id";
set GLOBAL debug="-d,skip_dd_table_access_check";
--enable_query_log
