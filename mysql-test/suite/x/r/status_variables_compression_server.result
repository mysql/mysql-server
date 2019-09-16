## 1. Verify compression at server side using ZLIB algorithm
RUN use performance_schema

0 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send1,@send_comp1,@send_uncomp1

1 rows affected
RUN SHOW STATUS LIKE 'Mysqlx_%'
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send2,@send_comp2,@send_uncomp2

1 rows affected
RUN SHOW STATUS LIKE 'Mysqlx_%'
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send3,@send_comp3,@send_uncomp3

1 rows affected
RUN SELECT @send3 > 0;
Verify ["SELECT @send3 > 0" returns in first column, following value 1]
RUN SELECT @send2 > 0;
Verify ["SELECT @send2 > 0" returns in first column, following value 1]
RUN SELECT @send1 > 0;
Verify ["SELECT @send1 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp3 > 0;
Verify ["SELECT @send_uncomp3 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp2 > 0;
Verify ["SELECT @send_uncomp2 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp3 > 0+@send_uncomp2;
Verify ["SELECT @send_uncomp3 > 0+@send_uncomp2" returns in first column, following value 1]
RUN SELECT @send_comp3 > 0;
Verify ["SELECT @send_comp3 > 0" returns in first column, following value 1]
RUN SELECT @send_comp2 > 0;
Verify ["SELECT @send_comp2 > 0" returns in first column, following value 1]
RUN SELECT @send_comp3 > 0+@send_comp2;
Verify ["SELECT @send_comp3 > 0+@send_comp2" returns in first column, following value 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## 2. Verify compression at server side using LZ4frame algorithm
RUN use performance_schema

0 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send1,@send_comp1,@send_uncomp1

1 rows affected
RUN SHOW STATUS LIKE 'Mysqlx_%'
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send2,@send_comp2,@send_uncomp2

1 rows affected
RUN SHOW STATUS LIKE 'Mysqlx_%'
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send3,@send_comp3,@send_uncomp3

1 rows affected
RUN SELECT @send3 > 0;
Verify ["SELECT @send3 > 0" returns in first column, following value 1]
RUN SELECT @send2 > 0;
Verify ["SELECT @send2 > 0" returns in first column, following value 1]
RUN SELECT @send1 > 0;
Verify ["SELECT @send1 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp3 > 0;
Verify ["SELECT @send_uncomp3 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp2 > 0;
Verify ["SELECT @send_uncomp2 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp3 > 0+@send_uncomp2;
Verify ["SELECT @send_uncomp3 > 0+@send_uncomp2" returns in first column, following value 1]
RUN SELECT @send_comp3 > 0;
Verify ["SELECT @send_comp3 > 0" returns in first column, following value 1]
RUN SELECT @send_comp2 > 0;
Verify ["SELECT @send_comp2 > 0" returns in first column, following value 1]
RUN SELECT @send_comp3 > 0+@send_comp2;
Verify ["SELECT @send_comp3 > 0+@send_comp2" returns in first column, following value 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## 3. Verify compression at server side using ZSTD algorithm
RUN use performance_schema

0 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send1,@send_comp1,@send_uncomp1

1 rows affected
RUN SHOW STATUS LIKE 'Mysqlx_%'
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send2,@send_comp2,@send_uncomp2

1 rows affected
RUN SHOW STATUS LIKE 'Mysqlx_%'
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_sent') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'Mysqlx_bytes_sent_uncompressed_frame')
       INTO @send3,@send_comp3,@send_uncomp3

1 rows affected
RUN SELECT @send3 > 0;
Verify ["SELECT @send3 > 0" returns in first column, following value 1]
RUN SELECT @send2 > 0;
Verify ["SELECT @send2 > 0" returns in first column, following value 1]
RUN SELECT @send1 > 0;
Verify ["SELECT @send1 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp3 > 0;
Verify ["SELECT @send_uncomp3 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp2 > 0;
Verify ["SELECT @send_uncomp2 > 0" returns in first column, following value 1]
RUN SELECT @send_uncomp3 > 0+@send_uncomp2;
Verify ["SELECT @send_uncomp3 > 0+@send_uncomp2" returns in first column, following value 1]
RUN SELECT @send_comp3 > 0;
Verify ["SELECT @send_comp3 > 0" returns in first column, following value 1]
RUN SELECT @send_comp2 > 0;
Verify ["SELECT @send_comp2 > 0" returns in first column, following value 1]
RUN SELECT @send_comp3 > 0+@send_comp2;
Verify ["SELECT @send_comp3 > 0+@send_comp2" returns in first column, following value 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
