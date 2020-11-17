INSTALL COMPONENT "file://component_mysqlx_global_reset";


## I. Scenario that triggers issue described under: Bug#31742798
#
# 1. Simulate Group-Replication notification (D1), and hang it
#    before processing clients
# 2. Make a new X Connection (X2), and hang in after accepting.
#    This prevents the session from being created. Synchronization
#    points are going to execute thread in order causin crash
# 3. Unblock X2, and validate that its possible to authenticate.

#
# I.1
Adding debug point 'xsync_gr_notice_bug' to @@GLOBAL.debug
RUN DO emit_member_role_change("VIEW-ID")

0 rows affected

#
# I.2
connecting...
active session is now 'X2'

#
# I.3
switched to session default
Removing debug point 'xsync_gr_notice_bug' from @@GLOBAL.debug
switched to session X2
Login OK
closing session X2
Mysqlx.Ok {
  msg: "bye!"
}
ok
UNINSTALL COMPONENT "file://component_mysqlx_global_reset";
