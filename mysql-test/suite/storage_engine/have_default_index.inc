#
# This include file either just prints the message, or skips the rest of the test
# Usage:
#   --let $continue = 1; # optional
#   --source have_default_index.inc
#

--let $have_default_index = 1
if ($default_index == '/*!*/ /*Custom index*/')
{
  if (!$continue)
  {
    --source cleanup_engine.inc
    --skip # Configuration does not allow indexes. Check \$default_index value
  }
  --echo # According to the configuration, the engine does not allow indexes. Check \$default_index value
  --let $have_default_index = 0
  --let $continue = 0
}

