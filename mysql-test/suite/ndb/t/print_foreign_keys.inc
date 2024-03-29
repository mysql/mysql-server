#
# Print the foreign key constraints associated with the given table
#
# ==== Usage ====
# [--let $ndb_db_name= <database name>]
# --let $ndb_table_name= <table name>
# --source print_foreign_keys.inc
#
# Parameters:
#   $ndb_db_name, $ndb_table_name,


# Table name is mandatory option
if (!$ndb_table_name) {
 --die ERROR IN TEST: Please set $ndb_table_name before calling print_foreign_keys.inc script
}

if (!$ndb_db_name) {
 let $ndb_db_name=test;
}


# Find the object id of the table whose FKs we have to list and verify
let $ndb_table_id= `SELECT id FROM ndbinfo.dict_obj_info
                    WHERE type = 2 /* UserTable */
                      AND fq_name = "$ndb_db_name/def/$ndb_table_name"`;

--disable_query_log

# Now list all the foreign key names using this table id.
# They will be of form <parent_id>/<child_id>/fk_name
--vertical_results
# truncated table as parent
eval SELECT REGEXP_REPLACE(fk_info.fq_name, '$ndb_table_id/[0-9]+/', '')
      AS "Foreign keys referenced by $ndb_db_name.$ndb_table_name : "
    FROM ndbinfo.dict_obj_info as fk_info
    WHERE fk_info.type = 25 /* ForeignKey */
      AND fk_info.fq_name LIKE '$ndb_table_id/%';
# truncated table as child
eval SELECT REGEXP_REPLACE(fk_info.fq_name, '[0-9]+/$ndb_table_id/', '')
       AS "Foreign keys on $ndb_db_name.$ndb_table_name : "
     FROM ndbinfo.dict_obj_info as fk_info
       WHERE fk_info.type = 25 /* ForeignKey */
     AND fk_info.fq_name LIKE '%/$ndb_table_id/%';
--horizontal_results

--enable_query_log

# Reset input parameters
--let $ndb_db_name=
--let $ndb_table_name=

