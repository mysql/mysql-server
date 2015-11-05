# Take a snapshot of SOCKET_INSTANCES
CREATE TEMPORARY TABLE my_socket_instances AS
SELECT * FROM performance_schema.socket_instances;
# Get thread id of the default connection
SELECT THREAD_ID INTO @thread_id
FROM performance_schema.threads
WHERE PROCESSLIST_ID = CONNECTION_ID();
# Establish local TCP/IP connection (con1,localhost,root,,test,,)
# Store the thread id of connection 1 (tcp/ip)
SELECT THREAD_ID INTO @thread_id
FROM performance_schema.threads
WHERE PROCESSLIST_ID = CONNECTION_ID();
# Store the port of connection 1 (tcp/ip)
SELECT PORT INTO @port
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
# Switch to connection default
# Establish second local TCP/IP connection (con1,localhost,root,,test,,)
# Store the thread_id of connection 2 (tcp/ip)
SELECT THREAD_ID INTO @thread_id
FROM performance_schema.threads
WHERE PROCESSLIST_ID = CONNECTION_ID();
# Store the port of connection 2 (tcp/ip)
SELECT PORT INTO @port
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
# Switch to connection default
# Establish local unix domain connection (con3,localhost,root,,test,,)
# Store the thread id of connection 3 (unix domain)
SELECT THREAD_ID INTO @thread_id
FROM performance_schema.threads
WHERE PROCESSLIST_ID = CONNECTION_ID();
# Store the port of connection 3 (unix domain)
SELECT PORT INTO @port
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
# Switch to connection default
# EVENT_NAME is the "wait/io/socket/*" instrument identifier.
SELECT COUNT(*) = 0 AS "Expect 1"
FROM performance_schema.socket_instances
WHERE EVENT_NAME NOT LIKE 'wait/io/socket/%';
Expect 1
1
# OBJECT_INSTANCE_BEGIN is an arbitrary identifier, guaranteed to be unique.
SELECT COUNT(*) = COUNT(DISTINCT OBJECT_INSTANCE_BEGIN) AS "Expect 1"
FROM performance_schema.socket_instances;
Expect 1
1
# SOCKET_ID is the internal file handle assigned to the socket.
SELECT COUNT(*) = COUNT(DISTINCT SOCKET_ID) AS "Expect 1"
FROM performance_schema.socket_instances;
Expect 1
1
# Characteristics per our thread
#    There must be only one entry with our thread_id
SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
# TCP/IP connections should have a unique port number
# Connection 1 (tcp/ip)
SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE PORT = @port;
Expect 1
1
# Connection 2 (tcp/ip)
SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE PORT = @port;
Expect 1
1
#    Check the content for the default connection (unix domain)
SELECT COUNT(*) = 1 as 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME = 'wait/io/socket/sql/client_connection'
AND PORT = 0 AND THREAD_ID = @thread_id;
Expect 1
1
# Characteristics of 'server_tcpip_socket' entry
#    Server listening socket, TCP/IP
#    There is only one entry with 'wait/io/socket/sql/server_tcpip_socket'.
#    It shares the same thread id as 'wait/io/socket/sql/server_unix_socket'.
SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME = 'wait/io/socket/sql/server_tcpip_socket';
Expect 1
1
# Get the 'server_tcpip_socket' thread id
SELECT THREAD_ID INTO @thread_id
FROM performance_schema.socket_instances
WHERE EVENT_NAME = 'wait/io/socket/sql/server_tcpip_socket';
#    Check the content.
SELECT THREAD_ID = @thread_id
AND (IP = '0.0.0.0' OR IP = '::')
AND PORT = @port
AND STATE = 'ACTIVE' AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME = 'wait/io/socket/sql/server_tcpip_socket';
Expect 1
1
# Characteristics of 'server_unix_socket' entry
#    Server listening socket, unix domain (socket file)
#    There is only one entry with 'wait/io/socket/sql/server_unix_socket'.
#    It shares the same thread id as 'wait/io/socket/sql/server_tcpip_socket'.
SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME = 'wait/io/socket/sql/server_unix_socket';
Expect 1
1
# Get the 'server_unix_socket' thread id
SELECT THREAD_ID INTO @thread_id
FROM performance_schema.socket_instances
WHERE EVENT_NAME = 'wait/io/socket/sql/server_unix_socket';
#    Check the content.
SELECT THREAD_ID = @thread_id
AND IP = ''
     AND PORT = 0
AND STATE = 'ACTIVE' AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE EVENT_NAME = 'wait/io/socket/sql/server_unix_socket';
Expect 1
1
# Server listening sockets (TCP and Unix) are handled on the same thread
SELECT COUNT(*) = 2 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
SELECT COUNT(*) = 2 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
#Compare server listener socket thread ids
select @match_thread_id;
@match_thread_id
1
#    Check content for client connection 1 (tcpip)
SELECT EVENT_NAME = 'wait/io/socket/sql/client_connection'
        AND (IP LIKE '%127.0.0.1' OR IP = '::1')
AND PORT = @port
AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
# Characteristics of entries with THREAD_ID of con1
#    There is only one entry with this id.
SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
#    Check content for client connection 2 (tcpip)
SELECT EVENT_NAME = 'wait/io/socket/sql/client_connection'
        AND (IP LIKE '%127.0.0.1' OR IP = '::1')
AND PORT = @port
AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
# Characteristics of entries with THREAD_ID of con2
#    There is only one entry with this id.
SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
#    Check the content for client connection 3 (unix domain).
SELECT EVENT_NAME = 'wait/io/socket/sql/client_connection'
     AND IP = ''
     AND PORT = 0
AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
# Characteristics of entries with THREAD_ID of con3
#    There is only one entry with this id.
SELECT COUNT(*) = 1 AS 'Expect 1'
FROM performance_schema.socket_instances
WHERE THREAD_ID = @thread_id;
Expect 1
1
# Show differences to socket_instances before con1, con2 and con3 connecting
SELECT EVENT_NAME, IP
FROM performance_schema.socket_instances
WHERE (EVENT_NAME,OBJECT_INSTANCE_BEGIN,THREAD_ID,SOCKET_ID,IP,PORT,STATE)
NOT IN (SELECT EVENT_NAME,OBJECT_INSTANCE_BEGIN,THREAD_ID,SOCKET_ID,IP,PORT,STATE
FROM my_socket_instances)
ORDER BY THREAD_ID;
EVENT_NAME	IP
wait/io/socket/sql/client_connection	<LOCALHOST>
wait/io/socket/sql/client_connection	<LOCALHOST>
wait/io/socket/sql/client_connection	
# Disconnect con1, con2 and con3
# After waiting a bit we should have no differences to socket_instances
# before con1, con2, con3 connecting.
SELECT *
FROM performance_schema.socket_instances
WHERE (EVENT_NAME,OBJECT_INSTANCE_BEGIN,THREAD_ID,SOCKET_ID,IP,PORT,STATE)
NOT IN (SELECT EVENT_NAME,OBJECT_INSTANCE_BEGIN,THREAD_ID,SOCKET_ID,IP,PORT,STATE
FROM my_socket_instances)
ORDER BY THREAD_ID;
EVENT_NAME	OBJECT_INSTANCE_BEGIN	THREAD_ID	SOCKET_ID	IP	PORT	STATE
