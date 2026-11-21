# ==== Purpose ====
#
# Wait until no session runs a statement whose text matches a LIKE pattern,
# and fail the test if that does not happen within a timeout. Used to check
# that a statement sent with --send does not hang.
#
# Unlike include/wait_condition.inc, it runs no replication debug queries on
# timeout: SHOW REPLICA STATUS and similar need the channel_map lock, which a
# hung statement may hold, so they would hang the test instead of failing it.
#
# ==== Usage ====
#
# --let $imr_statement_pattern= SET GLOBAL max_relay_log_size%
# --let $imr_statement_timeout= 30
# --source suite/rpl_gtid/t/rpl_csa_imr_wait_statement_done.inc
#
# Parameters:
#   $imr_statement_pattern
#     LIKE pattern matched against performance_schema.processlist.INFO.
#   $imr_statement_timeout
#     Seconds to wait.

--let $_imr_wsd_counter= `SELECT $imr_statement_timeout * 10`
--let $_imr_wsd_done= 0
while (!$_imr_wsd_done)
{
  --let $_imr_wsd_done= `SELECT COUNT(*) = 0 FROM performance_schema.processlist WHERE INFO LIKE '$imr_statement_pattern'`
  if (!$_imr_wsd_done)
  {
    --dec $_imr_wsd_counter
    if (!$_imr_wsd_counter)
    {
      --echo The statement '$imr_statement_pattern' did not finish within $imr_statement_timeout seconds
      --die A statement sent with --send did not finish in time
    }
    --sleep 0.1
  }
}
