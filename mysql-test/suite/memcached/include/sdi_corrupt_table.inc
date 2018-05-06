SET GLOBAL innodb_limit_optimistic_insert_debug = 2;

let TABLE_NAME = t1;
let MEMCACHED_TABLE_MAPPING = desc_t1;
--source ../include/sdi_insert_get_row.inc

let SDI_ID = 10;
let SDI_TYPE = 10;
let TABLE_METADATA = 'abcd10';
--source ../include/sdi_insert_row.inc

let SDI_ID = 11;
let SDI_TYPE = 11;
let TABLE_METADATA = 'abcd11';
--source ../include/sdi_insert_row.inc

let SDI_ID = 12;
let SDI_TYPE = 12;
let TABLE_METADATA = 'abcd12';
--source ../include/sdi_insert_row.inc

let SDI_ID = 13;
let SDI_TYPE = 13;
let TABLE_METADATA = 'abcd13';
--source ../include/sdi_insert_row.inc

let SDI_ID = 14;
let SDI_TYPE = 14;
let TABLE_METADATA = 'abcd14';
--source ../include/sdi_insert_row.inc

let SDI_ID = 15;
let SDI_TYPE = 15;
let TABLE_METADATA = 'abcd15';
--source ../include/sdi_insert_row.inc

# Ensure that dirty pages of table are flushed
SET GLOBAL innodb_buf_flush_list_now = 1;

--source include/shutdown_mysqld.inc

# Corrupt SDI page

perl;
my $fname= "$ENV{'MYSQLD_DATADIR'}test/t1.ibd";
open(FILE, "+<", $fname) or die;
binmode FILE;
seek(FILE, $ENV{'INNODB_PAGE_SIZE'} * $ENV{'SDI_PAGE_NUM'}, SEEK_SET);
print FILE chr(0) x 4;
close FILE;
EOF

--error 1
--exec $INNOCHECKSUM $MYSQLD_DATADIR/test/t1.ibd

let $restart_parameters = restart: --innodb_buffer_pool_load_at_startup=OFF;
--source include/start_mysqld.inc

# SDI Page Corrupted. Re-create indexes

--source ../include/sdi_drop_index.inc
--source ../include/sdi_create_index.inc

let TABLE_NAME = t1;
let MEMCACHED_TABLE_MAPPING = desc_t1;
--source ../include/sdi_insert_get_row.inc
