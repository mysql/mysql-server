#######################
# Author: JBM
# Date: 2006-01-17
# Purpose: To skip a test when default-storage-engine=ndb
#######################
-- require r/not_ndb_table.require
disable_query_log;
show variables like "table_type";
enable_query_log;

