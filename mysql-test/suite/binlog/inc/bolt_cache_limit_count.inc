# Counts the INSERT statements that fit in one transaction before
# ER_TRANS_CACHE_FULL, with the optimization OFF and then ON.
#
# Parameters:
#   $bolt_row_bytes        bytes of data in each inserted row
#   $bolt_max_cache_size   max_binlog_cache_size for the transaction
#
# Sets:
#   $statements_off  statements that fit with the optimization OFF
#   $statements_on   statements that fit with the optimization ON
#
--eval SET GLOBAL max_binlog_cache_size = $bolt_max_cache_size
--let $round = 1
while ($round <= 2)
{
  --let $knob = OFF
  if ($round == 2)
  {
    --let $knob = ON
  }
  --eval SET GLOBAL binlog_large_transaction_optimization_enabled = $knob
  # A new session opens its binlog cache with the sizes set above.
  --connect (limit_session, localhost, root,,)
  --disable_query_log
  BEGIN;
  --let $statements = 0
  --let $full = 0
  while (!$full)
  {
    --error 0, ER_TRANS_CACHE_FULL
    --eval INSERT INTO t1 VALUES ($statements, REPEAT('x', $bolt_row_bytes))
    if ($mysql_errno)
    {
      --let $full = 1
    }
    if (!$mysql_errno)
    {
      --inc $statements
    }
  }
  ROLLBACK;
  --enable_query_log
  --disconnect limit_session
  --connection default
  if ($round == 1)
  {
    --let $statements_off = $statements
  }
  if ($round == 2)
  {
    --let $statements_on = $statements
  }
  --inc $round
}
