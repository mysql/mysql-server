#
# This test upgrades a 5.7 database to 8.0 and validates that no
# PRIMARY index entries are seen in mysql.innodb_index_stats
# if there are no primary keys defined on the table
#
# Stop the running server
##############################################################
# Copy and unzip the datadir
##############################################################
# Restart the server with the unzipped datadir
# restart: --datadir=DATADIR --innodb_page_size=16k
# Check the contents of mysql.innodb_index_stats
SHOW CREATE TABLE t_nopart;
Table	Create Table
t_nopart	CREATE TABLE `t_nopart` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1
SHOW CREATE TABLE t_no_part_pk;
Table	Create Table
t_no_part_pk	CREATE TABLE `t_no_part_pk` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  PRIMARY KEY (`c1`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
SHOW CREATE TABLE t_part;
Table	Create Table
t_part	CREATE TABLE `t_part` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1
/*!50100 PARTITION BY RANGE (`c1`)
(PARTITION t_part_p1 VALUES LESS THAN (20) ENGINE = InnoDB,
 PARTITION t_part_p2 VALUES LESS THAN (100) ENGINE = InnoDB) */
SHOW CREATE TABLE t_part_pk;
Table	Create Table
t_part_pk	CREATE TABLE `t_part_pk` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  PRIMARY KEY (`c1`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
/*!50100 PARTITION BY RANGE (`c1`)
(PARTITION t_part_p1 VALUES LESS THAN (20) ENGINE = InnoDB,
 PARTITION t_part_p2 VALUES LESS THAN (100) ENGINE = InnoDB) */
SELECT lower(database_name), lower(table_name),
lower(index_name), lower(stat_name) FROM mysql.innodb_index_stats
ORDER BY lower(database_name), lower(table_name),
lower(index_name), lower(stat_name);
lower(database_name)	lower(table_name)	lower(index_name)	lower(stat_name)
mysql	component	primary	n_diff_pfx01
mysql	component	primary	n_leaf_pages
mysql	component	primary	size
sys	sys_config	primary	n_diff_pfx01
sys	sys_config	primary	n_leaf_pages
sys	sys_config	primary	size
test	t_no_part_pk	primary	n_diff_pfx01
test	t_no_part_pk	primary	n_leaf_pages
test	t_no_part_pk	primary	size
test	t_nopart	gen_clust_index	n_diff_pfx01
test	t_nopart	gen_clust_index	n_leaf_pages
test	t_nopart	gen_clust_index	size
test	t_part#p#t_part_p1	gen_clust_index	n_diff_pfx01
test	t_part#p#t_part_p1	gen_clust_index	n_leaf_pages
test	t_part#p#t_part_p1	gen_clust_index	size
test	t_part#p#t_part_p2	gen_clust_index	n_diff_pfx01
test	t_part#p#t_part_p2	gen_clust_index	n_leaf_pages
test	t_part#p#t_part_p2	gen_clust_index	size
test	t_part_pk#p#t_part_p1	primary	n_diff_pfx01
test	t_part_pk#p#t_part_p1	primary	n_leaf_pages
test	t_part_pk#p#t_part_p1	primary	size
test	t_part_pk#p#t_part_p2	primary	n_diff_pfx01
test	t_part_pk#p#t_part_p2	primary	n_leaf_pages
test	t_part_pk#p#t_part_p2	primary	size
# Cleanup
# Shutdown server
# Remove copied files
# Restart the server
# restart
