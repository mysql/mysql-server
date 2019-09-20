#####################################################################
#                                                                   #
# Aim of the test is to test compression between the mysql client   #
# and server using the zstd library. It does the following :-       #
#  - Loads table with appropriate data.                             #
#  - Recieves it through the client using different compression     #
#    levels.                                                        #
#  - Compares the number of bytes sent from the server for each     #
#    instance.                                                      #
# Creation Date: 2019-05-29                                         #
# Author: Srikanth B R                                              #
#                                                                   #
#####################################################################

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

# Create and populate the table to be used
CREATE TABLE t1 (
 id int,
 c2 int,
 c3 varchar(20),
 c4 varchar(20),
 c5 int,
 c6 int,
 c7 varchar(20),
 c8 varchar(20),
 c9 varchar(20),
 c10 int,
 c11 double,
 c12 varchar(20),
 c13 varchar(20),
 c14 double,
 c15 varchar(20),
 c16 int,
 c17 varchar(20)
) ENGINE = InnoDB;

LOAD DATA INFILE '../../std_data/inconsistent_scan.csv' INTO TABLE t1 COLUMNS TERMINATED BY "," IGNORE 1 LINES;

# Test without compression
SHOW STATUS LIKE 'Compression%';
FLUSH STATUS;
SELECT * FROM t1;
--let $size_uncompressed = query_get_value(SHOW STATUS LIKE 'Bytes_sent', Value, 1)


SET GLOBAL protocol_compression_algorithms="zstd";
# Test zstd compressed connection with maximum compression level.
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect(zstd_con, localhost, root,,,,,,,zstd,22);
SHOW STATUS LIKE 'Compression%';
FLUSH STATUS;
SELECT * FROM t1;
--let $size_compressed_level22 = query_get_value(SHOW STATUS LIKE 'Bytes_sent', Value, 1)
connection default;
disconnect zstd_con;

# Validate size of data transferred between the highest compression level offered by zstd and uncompressed data.
--let $assert_cond = $size_compressed_level22 < $size_uncompressed
--let $assert_text = Size of data transferred with default zstd level 22 compression should be less than the uncompressed data.
--source include/assert.inc

SET @@global.protocol_compression_algorithms=default;
# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

DROP TABLE t1;
