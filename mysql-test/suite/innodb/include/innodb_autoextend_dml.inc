# Insert given number of rows in the table

--disable_query_log
while ($row_count > 0) {
    --eval INSERT INTO $table_name VALUES($row_count, repeat('aaaaaa', 10000));
    dec $row_count;
}
commit;
--enable_query_log
