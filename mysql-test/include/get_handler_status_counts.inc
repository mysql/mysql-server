#
# Get Handler_% values and suppress SHOW STATUS deprecation warning.
#
--disable_warnings
eval $get_handler_status_counts;
--enable_warnings
