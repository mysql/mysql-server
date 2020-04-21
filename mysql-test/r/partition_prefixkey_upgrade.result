# Copy the 5.7 data zip file to working directory.
# Check that the zip file exists in the working directory.
# Unzip 5.7 data directory.
# Set data directory to the 5.7 data directory.
# Set log directory.
# Stop DB server which was created by MTR default
# Restart server to trigger upgrade.
# Check for errors in the error log.
Pattern "\[ERROR\]" not found

# Should show deprecation warnings in the error log when upgrading
# from 5.7.
Pattern "Usage of column 'test\.t1\.a' having prefix key part 'a\(100\)' in the PARTITION BY KEY\(\) clause is deprecated and will be removed in a future release\." not found
Pattern "Usage of column 'test\.t3\.a' having prefix key part 'a\(25\)' in the PARTITION BY KEY\(\) clause is deprecated and will be removed in a future release\." not found
# Check output of SHOW CREATE TABLE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10000) NOT NULL,
  `b` varchar(10) NOT NULL,
  PRIMARY KEY (`a`(100),`b`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
/*!50100 PARTITION BY KEY ()
PARTITIONS 3 */
# Create equivalent table after upgrade and insert the same data.
CREATE TABLE t2 (
a VARCHAR(10000) NOT NULL,
b VARCHAR(10) NOT NULL,
PRIMARY KEY (a(100),b)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PARTITION BY KEY() PARTITIONS 3;
Warnings:
Warning	1681	Column 'test.t2.a' having prefix key part 'a(100)' is ignored by the partitioning function. Use of prefixed columns in the PARTITION BY KEY() clause is deprecated and will be removed in a future release.
INSERT INTO t2 VALUES
('a','a'),
('a','b'),
('b','a'),
('a','aa'),
('aa','a'),
('a','zz'),
('zz','a');
# Ensure that partition calculations are the same for upgraded table
# and newly created table.
SELECT  (SELECT GROUP_CONCAT('(',a,',',b,')') FROM t1 PARTITION(p0))
= (SELECT GROUP_CONCAT('(',a,',',b,')') FROM t2 PARTITION(p0))
as `p0_equal`,
(SELECT GROUP_CONCAT('(',a,',',b,')') FROM t1 PARTITION(p1))
= (SELECT GROUP_CONCAT('(',a,',',b,')') FROM t2 PARTITION(p1))
as `p1_equal`,
(SELECT GROUP_CONCAT('(',a,',',b,')') FROM t1 PARTITION(p2))
= (SELECT GROUP_CONCAT('(',a,',',b,')') FROM t2 PARTITION(p2))
as `p2_equal`;
p0_equal	p1_equal	p2_equal
1	1	1
# Stop the server.
# Check for mysql_upgrade_info file in data directory.
# Cleanup.

# Copy the 8.0.19 data zip file to working directory.

# Check that the zip file exists in the working directory.

# Unzip 8.0.19 data directory.

# Set data directory to the 8.0.19 data directory.

# Set log directory.

# Restart server to trigger upgrade.

# Check for errors in the error log.
Pattern "\[ERROR\]" not found

# Should show a deprecation warning in the error log when upgrading
# from 8.0.19.
Pattern "Column 'test\.t1\.a' having prefix key part 'a\(100\)' is ignored by the partitioning function. Use of prefixed columns in the PARTITION BY KEY\(\) clause is deprecated and will be removed in a future release\." found

# Stop the server.

# Check for mysql_upgrade_info file in data directory

# Cleanup.

# Restart the server with default options.
# restart
