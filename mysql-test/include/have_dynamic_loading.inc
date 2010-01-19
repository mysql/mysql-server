#
# Whether server supports dynamic loading.
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip The test requires dynamic loading
}
disable_query_log;
show variables like 'have_dynamic_loading';
enable_query_log;
