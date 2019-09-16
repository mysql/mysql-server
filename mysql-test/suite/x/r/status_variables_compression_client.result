## 1. Verify compression at client side using ZLIB algorithm
RUN use performance_schema

0 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received1,@received_comp1,@received_uncomp1

1 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received2,@received_comp2,@received_uncomp2

1 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received3,@received_comp3,@received_uncomp3

1 rows affected
RUN SELECT @received3 > 0;
Verify ["SELECT @received3 > 0" returns in first column, following value 1]
RUN SELECT @received2 > 0;
Verify ["SELECT @received2 > 0" returns in first column, following value 1]
RUN SELECT @received1 > 0;
Verify ["SELECT @received1 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp3 > 0;
Verify ["SELECT @received_uncomp3 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp2 > 0;
Verify ["SELECT @received_uncomp2 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp3 > @received_uncomp2;
Verify ["SELECT @received_uncomp3 > @received_uncomp2" returns in first column, following value 1]
RUN SELECT @received_comp3 > 0;
Verify ["SELECT @received_comp3 > 0" returns in first column, following value 1]
RUN SELECT @received_comp2 > 0;
Verify ["SELECT @received_comp2 > 0" returns in first column, following value 1]
RUN SELECT @received_comp3 > 0+@received_comp2;
Verify ["SELECT @received_comp3 > 0+@received_comp2" returns in first column, following value 1]
RUN SELECT (@received_comp2 - @received_comp1) >= (@received_comp3 - @received_comp2);
Verify ["SELECT (@received_comp2 - @received_comp1) >= (@received_comp3 - @received_comp2)" returns in first column, following value 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## 2. Verify compression at client side using LZ4Frame algorithm
RUN use performance_schema

0 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received1,@received_comp1,@received_uncomp1

1 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received2,@received_comp2,@received_uncomp2

1 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received3,@received_comp3,@received_uncomp3

1 rows affected
RUN SELECT @received3 > 0;
Verify ["SELECT @received3 > 0" returns in first column, following value 1]
RUN SELECT @received2 > 0;
Verify ["SELECT @received2 > 0" returns in first column, following value 1]
RUN SELECT @received1 > 0;
Verify ["SELECT @received1 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp3 > 0;
Verify ["SELECT @received_uncomp3 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp2 > 0;
Verify ["SELECT @received_uncomp2 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp3 > @received_uncomp2;
Verify ["SELECT @received_uncomp3 > @received_uncomp2" returns in first column, following value 1]
RUN SELECT @received_comp3 > 0;
Verify ["SELECT @received_comp3 > 0" returns in first column, following value 1]
RUN SELECT @received_comp2 > 0;
Verify ["SELECT @received_comp2 > 0" returns in first column, following value 1]
RUN SELECT @received_comp3 > 0+@received_comp2;
Verify ["SELECT @received_comp3 > 0+@received_comp2" returns in first column, following value 1]
RUN SELECT (@received_comp2 - @received_comp1) >= (@received_comp3 - @received_comp2);
Verify ["SELECT (@received_comp2 - @received_comp1) >= (@received_comp3 - @received_comp2)" returns in first column, following value 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## 3. Verify compression at client side using ZSTD algorithm
RUN use performance_schema

0 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received1,@received_comp1,@received_uncomp1

1 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received2,@received_comp2,@received_uncomp2

1 rows affected
RUN SELECT (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received') ,
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_compressed_payload'),
       (select VARIABLE_VALUE from session_status WHERE VARIABLE_NAME like 'mysqlx_bytes_received_uncompressed_frame')
       INTO @received3,@received_comp3,@received_uncomp3

1 rows affected
RUN SELECT @received3 > 0;
Verify ["SELECT @received3 > 0" returns in first column, following value 1]
RUN SELECT @received2 > 0;
Verify ["SELECT @received2 > 0" returns in first column, following value 1]
RUN SELECT @received1 > 0;
Verify ["SELECT @received1 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp3 > 0;
Verify ["SELECT @received_uncomp3 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp2 > 0;
Verify ["SELECT @received_uncomp2 > 0" returns in first column, following value 1]
RUN SELECT @received_uncomp3 > @received_uncomp2;
Verify ["SELECT @received_uncomp3 > @received_uncomp2" returns in first column, following value 1]
RUN SELECT @received_comp3 > 0;
Verify ["SELECT @received_comp3 > 0" returns in first column, following value 1]
RUN SELECT @received_comp2 > 0;
Verify ["SELECT @received_comp2 > 0" returns in first column, following value 1]
RUN SELECT @received_comp3 > 0+@received_comp2;
Verify ["SELECT @received_comp3 > 0+@received_comp2" returns in first column, following value 1]
RUN SELECT (@received_comp2 - @received_comp1) >= (@received_comp3 - @received_comp2);
Verify ["SELECT (@received_comp2 - @received_comp1) >= (@received_comp3 - @received_comp2)" returns in first column, following value 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
