#==============================================================================
# Establish the level of IPV6 support
#==============================================================================
#==============================================================================
# Get hostname, port number
#==============================================================================
SELECT @@hostname INTO @MY_HOSTNAME;
SELECT @@port INTO @MY_MASTER_PORT;
#==============================================================================
# 1.0 Get the default connection object_instance_begin, thread id and verify
#     the expected number of client connections.
#==============================================================================

# 1.1 Confirm only one client connection

SELECT COUNT(*) INTO @my_client_connections
FROM performance_schema.socket_instances
WHERE EVENT_NAME LIKE "%client_connection%";

# 1.2 Get the default THREAD_ID;

SELECT THREAD_ID INTO @my_thread_id
FROM performance_schema.threads
WHERE PROCESSLIST_ID = CONNECTION_ID();

# 1.3 Get the default OBJECT_INSTANCE_BEGIN

SELECT OBJECT_INSTANCE_BEGIN INTO @my_object_instance_begin
FROM performance_schema.socket_instances
WHERE THREAD_ID = @my_thread_id;
#==============================================================================
# 2.0 ESTABLISH TCP/IP CONNECTION 1
#     Connect with IP = localhost (127.0.0.1 or ::1)
#==============================================================================

# 2.1 Get the connection thread id

SELECT THREAD_ID INTO @my_thread_id
FROM performance_schema.threads
WHERE PROCESSLIST_ID = CONNECTION_ID();

# 2.2 Get the connection object instance begin

SELECT OBJECT_INSTANCE_BEGIN INTO @my_object_instance_begin
FROM performance_schema.socket_instances
WHERE THREAD_ID = @my_thread_id;

# 2.3 Get the connection port

SELECT PORT INTO @my_port
FROM performance_schema.socket_instances
WHERE THREAD_ID = @my_thread_id;

# 2.4 Verify that the connection is 127.0.0.1 or ::1

SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME LIKE '%client_connection%'
  AND (IP LIKE '%127.0.0.1' OR IP LIKE '%::1')
AND PORT= @con1_port
AND OBJECT_INSTANCE_BEGIN= @con1_object_id;
Expect 1
1

# 2.5 Verify that the same connection is in the summary instance table

SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_summary_by_instance
WHERE EVENT_NAME LIKE '%client_connection%'
  AND OBJECT_INSTANCE_BEGIN= @con1_object_id;
Expect 1
1

# Switch to connection default

#==============================================================================
# 3.0 ESTABLISH TCP/IP CONNECTION 2
#     Connect with IP = localhost (127.0.0.1 or ::1)
#==============================================================================

# 3.1 Get the connection thread id

SELECT THREAD_ID INTO @my_thread_id
FROM performance_schema.threads
WHERE PROCESSLIST_ID = CONNECTION_ID();

# 3.2 Get the connection object instance begin

SELECT OBJECT_INSTANCE_BEGIN INTO @my_object_instance_begin
FROM performance_schema.socket_instances
WHERE THREAD_ID = @my_thread_id;

# 3.3 Get the connection port

SELECT PORT INTO @my_port
FROM performance_schema.socket_instances
WHERE THREAD_ID = @my_thread_id;

# 3.4 Verify that the connection is 127.0.0.1 or ::1

SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME LIKE '%client_connection%'
  AND (IP LIKE '%127.0.0.1' OR IP LIKE '%::1')
AND PORT= @con2_port
AND OBJECT_INSTANCE_BEGIN= @con2_object_id;
Expect 1
1

# 3.5 Verify that the same connection is in the summary instance table

SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_summary_by_instance
WHERE EVENT_NAME LIKE '%client_connection%'
  AND OBJECT_INSTANCE_BEGIN= @con2_object_id;
Expect 1
1

# 3.6 Verify that the connection is 127.0.0.1 or ::1

SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME LIKE '%client_connection%'
  AND (IP LIKE '%127.0.0.1' OR IP LIKE '%::1')
AND PORT= @con2_port
AND OBJECT_INSTANCE_BEGIN= @con2_object_id;
Expect 1
1
#==============================================================================
# 4.0 Verify both connections exist in the instance tables
#==============================================================================

# 4.1 Verify that there are two TCP/IP connections in the socket instance table

SELECT COUNT(*) = 2 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME LIKE '%client_connection%'
  AND OBJECT_INSTANCE_BEGIN <> @default_object_instance_begin
AND (IP LIKE '%127.0.0.1' OR IP LIKE '%::1');
Expect 1
1

# 4.2 Verify that there are two TCP/IP connections in the summary instance table

SELECT COUNT(*) = 2 AS 'Expect 1'
FROM performance_schema.socket_summary_by_instance
WHERE EVENT_NAME LIKE '%client_connection%'
  AND OBJECT_INSTANCE_BEGIN <> @default_object_instance_begin;
Expect 1
1
#==============================================================================
# 5.0 Drop the client connections
#==============================================================================
# 5.1 Disconnect con1
# 5.2 Disconnect con2
#==============================================================================
# 6.0 Verify sockets were removed from the instance tables
#==============================================================================

# 6.1 Verify that there are no TCP/IP connections in the socket instance table

SELECT COUNT(*) = 0 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME LIKE '%client_connection%'
  AND OBJECT_INSTANCE_BEGIN <> @default_object_instance_begin
AND (IP LIKE '%127.0.0.1' OR IP LIKE '%::1');
Expect 1
1

# 6.2 Verify that there are no TCP/IP connections in the summary instance table

SELECT COUNT(*) = 0 AS 'Expect 1'
FROM performance_schema.socket_summary_by_instance
WHERE EVENT_NAME LIKE '%client_connection%'
  AND OBJECT_INSTANCE_BEGIN <> @default_object_instance_begin;
Expect 1
1
