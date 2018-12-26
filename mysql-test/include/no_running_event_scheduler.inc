##########   include/no_running_event_scheduler.inc   ##########################
#                                                                              #
# Wait till the event scheduler disappeared from processlist.                  #
#                                                                              #
# The characteristics of the event_scheduler entry within the processlist is   #
# user = 'event_scheduler' and command = 'Daemon'. I am not 100% sure if       #
# ther is no short phase with command <> 'Daemon'.                             #
# A query with WHERE user = 'event_scheduler' only will also catch events in   #
# startup phase. This is no problem since this phase is very short.            #
#                                                                              #
# A wait_timeout of >= 3 seconds was within experiments sufficient even on a   #
# testing box with heavy parallel load. Therefore 5 seconds should be enough.  #
#                                                                              #
# Creation:                                                                    #
# 2008-12-19 mleich Implement this check needed for test bug fixes             #
#                                                                              #
################################################################################

let $wait_timeout= 5;
let $wait_condition=
  SELECT COUNT(*) = 0 FROM information_schema.processlist
  WHERE user = 'event_scheduler';
--source include/wait_condition.inc
