call mtr.add_suppression("keyring_file initialization failure. Please check if the keyring_file_data points to readable keyring file or keyring file can be created in the specified location. The keyring_file will stay unusable until correct path to the keyring file gets provided");
call mtr.add_suppression("Could not create keyring directory The keyring_file will stay unusable until correct path to the keyring directory gets provided");
call mtr.add_suppression("Error while loading keyring content. The keyring might be malformed");
call mtr.add_suppression("\\[ERROR\\] \\[[^]]*\\] \\[[^]]*\\] Plugin keyring_file reported: 'File .*keyring' not found .*");
SET @@global.keyring_file_data= 'MYSQL_TMP_DIR/new_keyring_file';
ERROR HY000: Unknown system variable 'keyring_file_data'
INSTALL PLUGIN keyring_file SONAME 'keyring_file.so';
SET @@global.keyring_file_data= 'MYSQL_TMP_DIR/new_keyring_file';
SET @@global.keyring_file_data= 'MYSQL_TMP_DIR/mysecret_keyring';
UNINSTALL PLUGIN keyring_file;
