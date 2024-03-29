#
# This test upgrades a partitioned table from 5.7 to 8.0.x and
# checks that INFORMATION_SCHEMA.INNODB_COLUMNS shows columns
# for all the partitions
#
# Stop the running the server
######################################################################
# Unzip a 5.7 datadir with partitioned table, start the server using
# this directory
######################################################################
######################################################################
# Copy and unzip the datadir.
######################################################################
# Restart the server against the unzipped datadir
# restart: --datadir=BUGDATADIR --innodb_page_size=16k
# Checks the contents of INFORMATION_SCHEMA.INNODB_COLUMNS
SELECT lower(t.name), lower(c.name) FROM
INFORMATION_SCHEMA.INNODB_COLUMNS c
JOIN INFORMATION_SCHEMA.INNODB_TABLES t
WHERE t.table_id=c.table_id AND t.name LIKE '%t1%'
  ORDER BY t.name, c.name;
lower(t.name)	lower(c.name)
partitions/t1#p#p0	a
partitions/t1#p#p0	b
partitions/t1#p#p1	a
partitions/t1#p#p1	b
# Cleanup
# Shutdown server
# Remove copied files
# Resart the server
# restart
