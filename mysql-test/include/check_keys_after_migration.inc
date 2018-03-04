--echo # now master key is migrated
--echo # access to this table should pass
SELECT * FROM T;

let $wl9769_destination_key1 = `SELECT hex(keyring_key_fetch('$key_name1'))`;
let $wl9769_destination_key2 = `SELECT hex(keyring_key_fetch('$key_name2'))`;
let $wl9769_destination_key3 = `SELECT hex(keyring_key_fetch('$key_name3'))`;

let $wl9769_destination_key1_type = `SELECT keyring_key_type_fetch('$key_name1')`;
let $wl9769_destination_key2_type = `SELECT keyring_key_type_fetch('$key_name2')`;
let $wl9769_destination_key3_type = `SELECT keyring_key_type_fetch('$key_name3')`;

let $wl9769_destination_key1_len = `SELECT keyring_key_length_fetch('$key_name1')`;
let $wl9769_destination_key2_len = `SELECT keyring_key_length_fetch('$key_name2')`;
let $wl9769_destination_key3_len = `SELECT keyring_key_length_fetch('$key_name3')`;

--echo check if migration took place correctly
--replace_result $wl9769_destination_key1 <wl9769_destination_key1> $wl9769_source_key1 <wl9769_source_key1>
eval SELECT '$wl9769_destination_key1' = '$wl9769_source_key1';
--replace_result $wl9769_destination_key2 <wl9769_destination_key2> $wl9769_source_key2 <wl9769_source_key2>
eval SELECT '$wl9769_destination_key2' = '$wl9769_source_key2';
--replace_result $wl9769_destination_key3 <wl9769_destination_key3> $wl9769_source_key3 <wl9769_source_key3>
eval SELECT '$wl9769_destination_key3' = '$wl9769_source_key3';

eval SELECT '$wl9769_destination_key1_type' = '$wl9769_source_key1_type';
eval SELECT '$wl9769_destination_key2_type' = '$wl9769_source_key2_type';
eval SELECT '$wl9769_destination_key3_type' = '$wl9769_source_key3_type';

eval SELECT '$wl9769_destination_key1_len' = '$wl9769_source_key1_len';
eval SELECT '$wl9769_destination_key2_len' = '$wl9769_source_key2_len';
eval SELECT '$wl9769_destination_key3_len' = '$wl9769_source_key3_len';

--replace_regex /[0-9]*-[0-9]*-[0-9]* [0-9]*:[0-9]*:[0-9]*.[0-9]*/key1_timestamp/
eval SELECT keyring_key_remove('$key_name1');
--replace_regex /[0-9]*-[0-9]*-[0-9]* [0-9]*:[0-9]*:[0-9]*.[0-9]*/key2_timestamp/
eval SELECT keyring_key_remove('$key_name2');
--replace_regex /[0-9]*-[0-9]*-[0-9]* [0-9]*:[0-9]*:[0-9]*.[0-9]*/key3_timestamp/
eval SELECT keyring_key_remove('$key_name3');
