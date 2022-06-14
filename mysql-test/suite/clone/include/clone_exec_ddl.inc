## Execute / SHOW DDL statement
## 1. ddl_op         : DDL operation - CREATE TABLE,  DROP TABLE,  ALTER TABLE ...
## 2. table_num      : Table number  - 1,2,3 ... [t1, t2 ...]
## 3. ddl_stmt       : DDL statement text following table name
## 4. ddl_post       : Any other follow up clause like partition
## 5. ddl_table_rename : If DDL is renaming one table to another
## 6. ddl_table_extn_from : Old table name extension - _old [t1_old, t2_old ...]
## 7. ddl_table_extn : Second table name extension - _new [t1_new, t2_new ...]
## 8. ddl_table_extn_show : Table name extension at test end - _new [t1_new, t2_new ...]
## 9. ddl_count : Number of DDLs to execute

if (!$ddl_count) {
  --let $ddl_count = 1
}

--let $ddl_num = 1

if ($ddl_exec) {

  while ($ddl_num <= $ddl_count) {

    --let $cur_ddl_op    = \$ddl_op$ddl_num$after_dml
    --let $cur_ddl_stmt  = \$ddl_stmt$ddl_num$after_dml
    --let $cur_ddl_post  = \$ddl_post$ddl_num$after_dml

    --let $cur_ddl_table_rename = \$ddl_table_rename$ddl_num$after_dml
    --let $cur_ddl_table_extn  = \$ddl_table_extn$ddl_num$after_dml
    --let $cur_ddl_table_extn_from  = \$ddl_table_extn_from$ddl_num$after_dml

    if (!$ddl_undo) {

      if (!$cur_ddl_table_rename) {

        if ($ddl_redo_encrypt) {
          --eval $cur_ddl_op
        }

        if (!$ddl_redo_encrypt) {
          --eval $cur_ddl_op t$table_num $cur_ddl_stmt $cur_ddl_post
        }
      }

      if ($cur_ddl_table_rename) {
        --eval $cur_ddl_op t$table_num$cur_ddl_table_extn_from $cur_ddl_stmt t$table_num$cur_ddl_table_extn
      }
    }

    if ($ddl_undo) {

      --let $cur_ddl_post =

      if ($ddl_undo_create) {
        --let $cur_ddl_post = 'undo_0$table_num.ibu'
      }

      --eval $cur_ddl_op undo_0$table_num $cur_ddl_stmt $cur_ddl_post

      if ($ddl_undo_empty) {
        --let $inactive_undo_space = undo_0$table_num
        --source include/wait_until_undo_space_is_empty.inc
      }
    }
    --inc $ddl_num
  }
}

if ($ddl_show) {
  --eval SHOW CREATE TABLE t$table_num$ddl_table_extn_show
  --eval CHECK TABLE t$table_num$ddl_table_extn_show EXTENDED
  --eval SELECT count(*) from t$table_num$ddl_table_extn_show
  --eval SELECT col1, col2, col3 FROM t$table_num$ddl_table_extn_show ORDER BY col1 LIMIT 10;
  --eval SELECT col1, col2, col3 FROM t$table_num$ddl_table_extn_show ORDER BY col1 DESC LIMIT 10;

  if ($ddl_undo) {

    SELECT NAME, SPACE_TYPE, STATE
    FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
    WHERE SPACE_TYPE = 'Undo'
    ORDER BY NAME;

    SELECT TABLESPACE_NAME, FILE_NAME, FILE_TYPE, ENGINE
    FROM INFORMATION_SCHEMA.FILES
    WHERE FILE_NAME LIKE '%undo_0%'
    ORDER BY TABLESPACE_NAME;
  }
}
