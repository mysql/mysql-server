-- require r/not_embedded.require
disable_query_log;
select version() like N'%embedded%' as 'have_embedded';
enable_query_log;

