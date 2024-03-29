#############   include/running_event_scheduler.inc   ##########################
#                                                                              #
# Wait till the event scheduler reached its final state within the processlist.#
#                                                                              #
# The characteristics of the event_scheduler entry within the processlist is   #
# user = 'event_scheduler' and command = 'Daemon'. I am not 100% sure if       #
# ther is no short phase with command <> 'Daemon'.                             #
# A query with WHERE user = 'event_scheduler' only will also catch events in   #
# startup phase.                                                               #
#                                                                              #
# Creation:                                                                    #
# 2008-12-19 mleich Implement this check needed for test bug fixes             #
#                                                                              #
################################################################################

# 1. Check that the server system variable shows the state needed
if (`SELECT @@global.event_scheduler <> 'ON'`)
{
   --echo # Error: We expect here that the event scheduler is switched on.
   SELECT @@global.event_scheduler;
   --echo #        Thinkable reasons:
   --echo #        1. SET GLOBAL event_scheduler = ON had not the expected effect.
   --echo #        2. Use of the current routine (include/running_event_scheduler.inc)
   --echo #           within the wrong situation
   --die
}
let $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
  WHERE user = 'event_scheduler' AND command = 'Daemon';
--source include/wait_condition.inc
