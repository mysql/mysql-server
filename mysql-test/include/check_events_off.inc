##########   include/check_events_off.inc   ####################################
#                                                                              #
# Purpose:                                                                     #
# Wait till we can expect that we have no event activity till the scheduler is #
# switched on again.                                                           #
# = There will be no modifications of user tables by existing events           #
#   except they use "INSERT DELAYED" or the server system variable             #
#   "concurrent_inserts" is not switched off.                                  #
#   Only some storage engines support concurrent_inserts" or "INSERT DELAYED". #
#                                                                              #
# Creation:                                                                    #
# 2008-12-19 mleich Implement this check needed for bug fixes in tests         #
#                                                                              #
################################################################################

# 1. Check that the server system variable shows the state needed
if (`SELECT @@global.event_scheduler <> 'OFF'`)
{
   --echo # Error: We expect here that the event scheduler is switched off.
   SELECT @@global.event_scheduler;
   --echo #        Thinkable reasons:
   --echo #        1. SET GLOBAL event_scheduler = OFF had not the expected effect.
   --echo #        2. Use of the current routine (include/check_events_off.inc)
   --echo #           within the wrong situation
   --die
}
# 2. Wait till we have no event_scheduler session within the processlist
--source include/no_running_event_scheduler.inc
# 3. Wait till we have no event executor sessions within the processlist
--source include/no_running_events.inc

