#
# Whether server supports dynamic loading.
#
--require r/have_dynamic_loading.require
disable_query_log;
show variables like 'have_dynamic_loading';
enable_query_log;
