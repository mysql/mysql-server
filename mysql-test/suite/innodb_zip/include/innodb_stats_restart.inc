--echo ===============
--echo After Restart Chekc the stats of the table
--echo ===============

-- echo # testcase : pass = 1 fail = 0
SELECT count(*) > 0 as "compress_stat"
FROM information_schema.innodb_cmp_per_index
WHERE 
compress_ops_ok BETWEEN @comp_val AND 1000
AND uncompress_ops BETWEEN @uncomp_val AND 1000
AND table_name='tab5' AND database_name='test'
AND index_name like 'idx%' ;
