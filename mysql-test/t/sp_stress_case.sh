#!/bin/sh

#
# Bug#19194 (Right recursion in parser for CASE causes excessive stack
#   usage, limitation)
#
# Because the code for the CASE statement is so massive,
# checking in an already generated .test is not practical,
# due to it's size (10 000 lines or 356 000 bytes).
#
# Patches are sent by email, which introduce size limitations.
#
# As a result, code is generated dynamically here.
# This script takes no argument, and generates code in stdout.
#

cat <<EOF
echo 'Silently creating PROCEDURE bug_19194_a';
--disable_query_log
delimiter |;
CREATE PROCEDURE bug_19194_a(i int)
BEGIN
DECLARE str CHAR(10);
  CASE i
EOF

count=1;
while true; do
  echo "    WHEN $count THEN SET str=\"$count\";"
  count=`expr $count + 1`
  test $count -gt 5000 && break
done

cat <<EOF
    ELSE SET str="unknown";
  END CASE;
  SELECT str;
END|
delimiter ;|
--enable_query_log
EOF

cat <<EOF
echo 'Silently creating PROCEDURE bug_19194_b';
--disable_query_log
delimiter |;
CREATE PROCEDURE bug_19194_b(i int)
BEGIN
DECLARE str CHAR(10);
  CASE
EOF

count=1;
while true; do
  echo "    WHEN i=$count THEN SET str=\"$count\";"
  count=`expr $count + 1`
  test $count -gt 5000 && break
done

cat <<EOF
    ELSE SET str="unknown";
  END CASE;
  SELECT str;
END|
delimiter ;|
--enable_query_log
EOF

