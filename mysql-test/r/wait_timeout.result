SET @@global.log_error_verbosity= 3;
SET @@global.wait_timeout= 2;
disconnect default;
connection wait_con;
connection default;
--disable_reconnect;
SELECT 1;
1
1
connection wait_con;
connection default;
SELECT 2;
Got one of the listed errors
--enable_reconnect;
SELECT 3;
3
3
disconnection default;
connection wait_con;
connection con1;
--disable_reconnect;
SELECT 1;
1
1
connection wait_con;
connection con1;
SELECT 2;
Got one of the listed errors
--enable_reconnect;
SELECT 3;
3
3
SET @@global.wait_timeout= <start_value>;
disconnection con1;
#
# Bug#54790: Use of non-blocking mode for sockets limits performance
#
#
# Test UNIX domain sockets timeout.
#
# Open con1 and set a timeout.
SET @@SESSION.wait_timeout = 2;
# Wait for con1 to be disconnected.
# Check that con1 has been disconnected.
SELECT 1;
Got one of the listed errors
#
# Test TCP/IP sockets timeout.
#
# Open con1 and set a timeout.
SET @@SESSION.wait_timeout = 2;
# Wait for con1 to be disconnected.
# Check that con1 has been disconnected.
# Client interaction timeout
SELECT 1;
Got one of the listed errors
SELECT "Check that we don't reconnect with reconnection disabled.";
Got one of the listed errors
# must find the pattern
Pattern "'wait_timeout' period of .* seconds was exceeded for .*\. The idle time since last command was too long\." found
