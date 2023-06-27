# This script disables:
# - page cleaner threads,
# - threads which could assume that pages should have
#   been flushed eventually,
# - background threads which potentially could try to
#   modify a page,
# - checkpointing mechanism.
#
# This is a very useful utility in tests which need to freeze
# the checkpointing mechanism to fill the redo log. It allows
# to do that avoiding risk that some background thread could
# be awaiting for checkpoint advanced.

# Disable background threads that could modify pages.

SET GLOBAL innodb_dict_stats_disabled_debug = ON;
SET GLOBAL innodb_master_thread_disabled_debug = ON;

# Disable generating GTIDs, and disable periodic GTID write-out.
# Generating GTIDs can lead to foreground threads wait for the write-out.

SET GLOBAL debug = "+d,disable_se_persists_gtid";

FLUSH ENGINE LOGS;

SET GLOBAL debug = "+d,gtid_persist_flush_disable";

# Flush all dirty pages and disable page cleaner threads
--source stop_dirty_page_flushing_and_redo_log_truncating.inc

# Disable undo purge coordinator thread, because it could try
# to use a mini-transaction and end up awaiting on a page,
# when keeping some rw-lock from DD (leading to a global stall
# when other threads needed that rw-lock).

SET GLOBAL innodb_purge_stop_now = ON;
