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

# Flush all dirty pages and write a checkpoint so we would
# always start filling redo log starting from an empty one.
SET GLOBAL innodb_log_checkpoint_now = ON;

# Disable checkpoint writes
SET GLOBAL innodb_checkpoint_disabled = ON;
