###########################################
#
# This is a stub of the include file cleanup_engine.inc which 
# should be placed in storage/<engine>/mysql-test/storage_engine folder.
#
################################
#
# Here you can add whatever is needed to cleanup 
# in case your define_engine.inc created any artefacts,
# e.g. an additional schema and/or tables.
--disable_query_log
--disable_warnings
DROP DATABASE IF EXISTS mrg;
--enable_warnings
--enable_query_log

