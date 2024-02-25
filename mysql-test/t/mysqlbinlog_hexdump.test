
#
# Bug#11747887 - MYSQLBINLOG --HEXDUMP PRINTS LAST ROW OF HEXDUMP
#                INCORRECTLY

--replace_regex s/	Xid = [0-9]*/	Xid = <number>/#
--exec $MYSQL_BINLOG --hexdump suite/binlog/std_data/bug11747887-bin.000003
