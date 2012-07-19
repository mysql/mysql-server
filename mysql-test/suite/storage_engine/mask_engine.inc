#
# This include file just replaces the storage engine under test by the generic string <STORAGE_ENGINE>
# in the next statement. More masks can be added by defining $add_regex, e.g.
# let $add_regex = /$data_dir/<DATA_DIR>/ /$index_dir/<INDEX_DIR>/
#

--let $regex = /$storage_engine/<STORAGE_ENGINE>/i
if ($add_regex)
{
  --let $regex = $regex $add_regex
}
--let $add_regex = 
--replace_regex $regex

