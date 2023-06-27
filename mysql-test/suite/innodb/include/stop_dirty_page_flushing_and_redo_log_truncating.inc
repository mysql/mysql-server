# This script disables checkpointing mechanism.
#
# It is useful utility in tests which need to freeze
# the checkpointing mechanism to fill the redo log.

# Intimate the developer to include "have_debug.inc in the actual test
let $have_debug = `SELECT VERSION() LIKE '%debug%'`;
if (!$have_debug)
{
  --die This script requires 'have_debug'
}

--source flush_dirty_pages_and_stop_flushing.inc

# Disable checkpoint writes
SET GLOBAL innodb_checkpoint_disabled = ON;
