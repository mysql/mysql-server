#
# Check that second part of $LIMIT is between $MIN_LIMIT and $MAX_LIMIT
# This is useful to check that a variable from SHOW_VARIABLES is within
# certain limits. Check query_cache_merge.test for an example of using this.
#
-- require r/check_var_limit.require
disable_query_log;
eval select SUBSTRING_INDEX("$LIMIT", "\\t", -1) BETWEEN $MIN_LIMIT AND $MAX_LIMIT as "limit";
enable_query_log;
