--source have_response_time_distribution.inc
--source include/have_xtradb.inc
SELECT Variable_name FROM INFORMATION_SCHEMA.SESSION_VARIABLES ORDER BY 1;
