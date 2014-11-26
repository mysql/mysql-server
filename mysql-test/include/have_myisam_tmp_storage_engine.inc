-- require r/have_myisam_tmp_storage_engine.require
disable_query_log;
show variables like 'internal_tmp_disk_storage_engine';
enable_query_log;

