--require r/not_staging.require
disable_query_log;
eval select $STAGING_RUN as using_staging_run;
enable_query_log;
