##########   include/no_running_events.inc   ###################################
#                                                                              #
# Wait till all event executors have finished their work.                      #
#                                                                              #
# Different event executors share the characteristics that their entry within  #
# processlist contains command = 'Connect'.                                    #
# Of course the corresponding query will also catch other connections being    #
# within the connect phase. This is no problem since the connect phase is      #
# usually very short.                                                          #
#                                                                              #
# A wait_timeout of >= 3 seconds was during experiments in case of "simple"    #
# SQL commands sufficient even on a testing box with heavy parallel load.      #
# "simple" = no sleeps, no long running commands, no waiting for lock ...      #
# We use here the default of 30 seconds because this wastes some time only in  #
# case of unexpected situations.                                               #
#                                                                              #
# Creation:                                                                    #
# 2008-12-19 mleich Implement this check needed for test bug fixes             #
#                                                                              #
################################################################################

let $wait_condition=
  SELECT COUNT(*) = 0 FROM information_schema.processlist
  WHERE command = 'Connect';
--source include/wait_condition.inc
