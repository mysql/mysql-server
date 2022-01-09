#
# Do clean shutdown of MySQL and remove log files.
#
Prepared a file with size = 0 bytes
Prepared a file with size = 1 bytes
Prepared a file with size = 65535 bytes
Prepared a file with size = 65537 bytes
#
# Create #ib_redo0 as empty file and start MySQL.
# We should detect that #ib_redo0 is empty.
#
Pattern "redo log file .*#ib_redo0 is empty" found
#
# Create #ib_redo0 with 1 byte inside and start MySQL.
# We should detect that #ib_redo0 is too small.
#
Pattern "redo log file .*#ib_redo0 is smaller than 65536 bytes" found
#
# Create #ib_redo0 with 64kB-1 bytes inside and start MySQL.
# We should detect that #ib_redo0 is too small.
#
Pattern "redo log file .*#ib_redo0 is smaller than 65536 bytes" found
#
# Create #ib_redo0 with 64kB+1 bytes inside and start MySQL.
# We should detect that #ib_redo0's size is not divisible by page size.
#
Pattern ".*#ib_redo0 size 65537 is not a multiple of innodb_page_size" found

#
# Start MySQL afterwards.
#
# restart:
