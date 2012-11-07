let $QC_check = `SELECT @@GLOBAL.query_cache_type = 'ON'`;
if ( $QC_check == 1 )
{
   --skip Test requires: Query Cache to be disabled
}
