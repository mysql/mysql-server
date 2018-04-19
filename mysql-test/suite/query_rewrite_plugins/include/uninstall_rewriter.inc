disable_query_log;
DROP DATABASE IF EXISTS query_rewrite;
DROP FUNCTION IF EXISTS load_rewrite_rules;
error 0, ER_SP_DOES_NOT_EXIST;
UNINSTALL PLUGIN rewriter;
enable_query_log;
echo # Query rewrite plugin was queued for uninstalling.;
