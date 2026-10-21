# The caller defines UPGRADE_HISTORY_FILE once for the test. For each
# invocation, it sets HISTORY_VERSION, HISTORY_VERSION_WITH_UPDATE, and the
# optional EXPECT_LEGACY_PREVIOUS_VERSION,
# EXPECT_UPGRADE_HISTORY_ENTRY_COUNT, EXPECT_UPDATE_HISTORY_ENTRY_COUNT, and
# EXPECT_FIRST_UPDATE_HISTORY_VERSION. EXPECT_LAST_UPGRADE_HISTORY_DATE and
# EXPECT_LAST_UPGRADE_HISTORY_MATURITY optionally verify preserved members of
# a matching legacy entry.
# Exact versions are stored in the latest entry's update_history array. The
# optional values verify a preserved legacy first entry and expected history
# and update-history entry counts. Load the server-created file through SQL so
# the test does not depend on an external JSON parser.
--disable_query_log

CREATE TABLE test.upgrade_history_contents (contents LONGTEXT NOT NULL);

--eval LOAD DATA INFILE '$UPGRADE_HISTORY_FILE' INTO TABLE test.upgrade_history_contents;

--let $upgrade_history_is_valid= `SELECT JSON_VALID(contents) FROM test.upgrade_history_contents`
if ($upgrade_history_is_valid != 1)
{
  --die upgrade history is not valid JSON
}

--let $upgrade_history_version_matches= `SELECT JSON_UNQUOTE(JSON_EXTRACT(contents, '$.upgrade_history[last].version')) = '$HISTORY_VERSION' FROM test.upgrade_history_contents`
if ($upgrade_history_version_matches != 1)
{
  --die upgrade history contains an unexpected compatibility version
}

--let $upgrade_history_full_version_matches= `SELECT JSON_UNQUOTE(JSON_EXTRACT(contents, '$.upgrade_history[last].update_history[last].version_with_update')) = '$HISTORY_VERSION_WITH_UPDATE' FROM test.upgrade_history_contents`
if ($upgrade_history_full_version_matches != 1)
{
  --die upgrade history contains an unexpected full version
}

if ($EXPECT_UPGRADE_HISTORY_ENTRY_COUNT != "")
{
  --let $upgrade_history_entry_count= `SELECT JSON_LENGTH(JSON_EXTRACT(contents, '$.upgrade_history')) FROM test.upgrade_history_contents`
  if ($upgrade_history_entry_count != $EXPECT_UPGRADE_HISTORY_ENTRY_COUNT)
  {
    --die upgrade history has an unexpected number of parent entries
  }
}

if ($EXPECT_UPDATE_HISTORY_ENTRY_COUNT != "")
{
  --let $update_history_entry_count= `SELECT JSON_LENGTH(JSON_EXTRACT(contents, '$.upgrade_history[last].update_history')) FROM test.upgrade_history_contents`
  if ($update_history_entry_count != $EXPECT_UPDATE_HISTORY_ENTRY_COUNT)
  {
    --die upgrade history has an unexpected number of update entries
  }
}

if ($EXPECT_FIRST_UPDATE_HISTORY_VERSION != "")
{
  --let $first_update_history_version_matches= `SELECT JSON_UNQUOTE(JSON_EXTRACT(contents, '$.upgrade_history[last].update_history[0].version_with_update')) = '$EXPECT_FIRST_UPDATE_HISTORY_VERSION' FROM test.upgrade_history_contents`
  if ($first_update_history_version_matches != 1)
  {
    --die upgrade history does not preserve its first full version
  }
}

if ($EXPECT_LAST_UPGRADE_HISTORY_DATE != "")
{
  --let $last_upgrade_history_date_matches= `SELECT JSON_UNQUOTE(JSON_EXTRACT(contents, '$.upgrade_history[last].date')) = '$EXPECT_LAST_UPGRADE_HISTORY_DATE' FROM test.upgrade_history_contents`
  if ($last_upgrade_history_date_matches != 1)
  {
    --die upgrade history does not preserve the latest entry date
  }
}

if ($EXPECT_LAST_UPGRADE_HISTORY_MATURITY != "")
{
  --let $last_upgrade_history_maturity_matches= `SELECT JSON_UNQUOTE(JSON_EXTRACT(contents, '$.upgrade_history[last].maturity')) = '$EXPECT_LAST_UPGRADE_HISTORY_MATURITY' FROM test.upgrade_history_contents`
  if ($last_upgrade_history_maturity_matches != 1)
  {
    --die upgrade history does not preserve the latest entry maturity
  }
}

if ($EXPECT_LEGACY_PREVIOUS_VERSION != "")
{
  --let $legacy_upgrade_entry_count= `SELECT JSON_LENGTH(JSON_EXTRACT(contents, '$.upgrade_history')) FROM test.upgrade_history_contents`
  if ($legacy_upgrade_entry_count != 2)
  {
    --die unexpected number of upgrade history entries
  }

  --let $legacy_upgrade_version_matches= `SELECT JSON_UNQUOTE(JSON_EXTRACT(contents, '$.upgrade_history[0].version')) = '$EXPECT_LEGACY_PREVIOUS_VERSION' FROM test.upgrade_history_contents`
  if ($legacy_upgrade_version_matches != 1)
  {
    --die legacy upgrade history version was not preserved
  }

  --let $legacy_upgrade_has_no_update_history= `SELECT JSON_EXTRACT(contents, '$.upgrade_history[0].update_history') IS NULL FROM test.upgrade_history_contents`
  if ($legacy_upgrade_has_no_update_history != 1)
  {
    --die legacy upgrade history entry unexpectedly has update history
  }
}

DROP TABLE test.upgrade_history_contents;
--enable_query_log
