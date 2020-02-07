#
# Bug#25153261 : SEG FAULT IN UPGRADING PARTITIONED TABLE
#
# Set different paths for --datadir
# Copy the remote tablespace & DB zip files from suite location to working location.
# Check that the file exists in the working folder.
# Unzip the zip file.
# Stop DB server which was created by MTR default
# Create a bootstrap file in temp location
MYSQL_TMP_DIR/bootstrap.log
# Start the server. Without fix, there is a segmentation fault in this step.
SHOW CREATE TABLE partitions.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(50) CHARACTER SET koi8r COLLATE koi8r_bin DEFAULT NULL,
  `b` varchar(50) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1
/*!50500 PARTITION BY RANGE  COLUMNS(a)
(PARTITION p0 VALUES LESS THAN ('ц├ц┤') ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (MAXVALUE) ENGINE = InnoDB) */
# Stop the server
# Check if .ibds have SDI after upgrade
# Check SDI from sys_config.ibd
# Check SDI from t1#P#p1.ibd
# Check SDI from t1#P#p0.ibd
# Remove json files
# Remove copied files
# Restart the server with default options.
# restart
