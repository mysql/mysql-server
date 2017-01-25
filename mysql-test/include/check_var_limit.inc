#
# Check that second part of $LIMIT is between $MIN_LIMIT and $MAX_LIMIT
# This is useful to check that a variable from SHOW_VARIABLES is within
# certain limits. Check query_cache_merge.test for an example of using this.
#
let $check_var_limit= `SELECT SUBSTRING_INDEX("$LIMIT", "\\t", -1) BETWEEN $MIN_LIMIT AND $MAX_LIMIT`;
if (!$check_var_limit)
{
  --skip Test requires 'check_var_limit'
}
