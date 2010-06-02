-- require r/have_ssl_is_yes_or_disabled_only.require
disable_query_log;
# "yes" means that the ssl library is found and the server can communicate using SSL.
# "no" means that there are no SSL functions in the server.
# "disabled" means that ssl functions exist, but communication is disabled for some reason.
replace_regex s/(YES|DISABLED)/yesordisabled/;
show variables like "have_ssl";
enable_query_log;
