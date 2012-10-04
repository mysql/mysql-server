# The performance schema internal structures are compiled with PFS_ALIGN,
# and the sizeof() structures is platform dependent.
#
# For tests sensitive to the internal sizes (show engine performance_schema
# status), make sure we use a platform with aligned memory.

--disable_query_log
let $aligned = `SELECT count(*) from performance_schema.session_connect_attrs where PROCESSLIST_ID = connection_id() and ATTR_NAME = '_os' and ATTR_VALUE in ('Linux', 'Windows')`;
if (!$aligned)
{
  skip Need a platform with aligned memory;
}
--enable_query_log
