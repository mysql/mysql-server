# Wait until DML changes by own session has been written to the
# local binlog by the ndb_binlog thread.
#
# This is necessary since the ndb_binlog thread is asynchronously recording
# changes that happens in NDB. Basically it monitors all tables it's interested
# in by settting up subscriptions which  will cause NDB to send a change
# notification(event) when data in the table changes. These notifications
# will then be processed by the ndb_binlog thread and written (injected) into
# the local binlog of the MySQL Server.
#
# There are several ways to wait for own session changes and this include file
# uses the mechanism implemented in mysqltest which is triggered
# when saving the current position in the binlog. In order to see #your own
# writes" mysqltest will wait for the epoch of sessions last change to be
# reported as handled by the ndb_binlog thread, after that it will save the
# binlog's position in the normal way (using SQL query).

echo # Wait for ndb_binlog thread...;
--save_master_pos
