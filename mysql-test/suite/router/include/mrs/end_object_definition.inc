--disable_query_log
--disable_result_log
COMMIT;
--enable_query_log
--enable_result_log


if ($extra_mrs_router_id)
{
   if ($_mrs_object_definitions > 0)
   {
     --let $mrs_changes_inc=$_mrs_object_definitions
     --source changes/wait_object.inc
   }

   # Default schema has some "options files", add separate
   # counter for it ?
   #--let $mrs_changes_inc=$_mrs_files_definitions
   #--source changes/wait_files.inc
}
