# ==== Purpose ====
#
# Auxiliary script used by rpl_gtid_split_statements.inc.
#
# Calls rpl_gtid_drop_multiple_statements.inc four times:
# - Once for each combination of two tables (three combinations).
# - Once with all three tables.
#
# ==== Usage ====
#
# CREATE PROCEDURE create_tables () BEGIN ... END;
# CREATE PROCEDURE drop_tables () BEGIN ... END;
# --let $automatic= [0|1]
# --let $transaction_count= N
# --source extra/rpl_tests/rpl_drop_multiple_tables_in_multiple_ways.inc
#
# Parameters: see extra/rpl_tests/rpl_drop_multiple_tables.inc.

if ($transaction_count != 1)
{
  --let $transaction_count= 2
}

--let $table_list= base, temp_t
--let $remaining_table_list= temp_n
--source extra/rpl_tests/rpl_drop_multiple_tables.inc

--let $table_list= base, temp_n
--let $remaining_table_list= temp_t
--source extra/rpl_tests/rpl_drop_multiple_tables.inc

--let $table_list= temp_t, temp_n
--let $remaining_table_list= base
--source extra/rpl_tests/rpl_drop_multiple_tables.inc


if ($transaction_count != 1)
{
  --let $transaction_count= 3
}

--let $table_list= base, temp_t, temp_n
--let $remaining_table_list=
--source extra/rpl_tests/rpl_drop_multiple_tables.inc
