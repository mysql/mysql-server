--disable_query_log
--disable_result_log
--write_file $MYSQL_TMP_DIR/grep_check
123
456
EOF
--error 0,1,2,42,127,512
--exec grep -v -e "123" $MYSQL_TMP_DIR/grep_check
let $status= $__error;

--remove_file $MYSQL_TMP_DIR/grep_check
--let $GREP= grep
--enable_query_log
--enable_result_log

if ($status)
{
  --skip Test requires gnu grep
}
