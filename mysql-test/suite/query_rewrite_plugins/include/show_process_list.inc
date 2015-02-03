# We have to hide quite a few columns from the process list, as they will
# differ between runs, depending on how the server was started and how the test
# was run. There are also differences between platforms. The State and Info
# columns will have different content on Solaris, for instance. The main point
# for the refresh_thread test - that this include is intended for - is merely
# to verify that the server is indeed aware of the refresh thread.

--replace_column 1 <id> 4 <db> 5 <command> 6 <time> 7 <state> 8 <info>
--replace_regex /localhost(:[0-9]+)?/<host>/
SHOW PROCESSLIST;
