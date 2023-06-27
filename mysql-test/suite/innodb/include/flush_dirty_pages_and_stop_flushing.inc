# This script disables page cleaner threads
#
# It is useful utility in tests where we want to collect dirty pages to
# flush them later.

# Intimate the developer to include "have_debug.inc in the actual test
let $have_debug = `SELECT VERSION() LIKE '%debug%'`;
if (!$have_debug)
{
  --die This script requires 'have_debug'
}

# Flush all dirty pages and write a checkpoint so we would
# always start filling redo log starting from an empty one.
SET GLOBAL innodb_log_checkpoint_now = ON;

# Disable page cleaner threads, because we are going to fill
# redo log and we want to collect a group of dirty pages for
# which the redo log records protect changes.
SET GLOBAL innodb_page_cleaner_disabled_debug = ON;