# We will use table_id as SDI ID & TYPE
let SDI_ID = `SELECT TABLE_ID FROM INFORMATION_SCHEMA.INNODB_TABLES
WHERE NAME = 'test/$TABLE_NAME'`;
let SDI_TYPE = $SDI_ID;

# Insert t1 Metadata into SDI Table
# Note this is temporary as Server is not using it.
# Server will insert a JSON/BSON into the SDI Table
let TABLE_METADATA = `SHOW CREATE TABLE $TABLE_NAME`;

# echo "t1_metadata is $TABLE_METADATA";
# echo "t1 sdi id is $SDI_ID";
# echo "t1 sdi type is $SDI_TYPE";
--source sdi_insert_row.inc
--source sdi_get_row.inc

