--source include/have_debug.inc

SET SESSION debug= '+d,skip_dd_table_access_check';
#
# 1. Extract the I_S version from the current database.
#
# The current mysql.dd_properties.properties has the format:
#
#   DD_version=99999;IS_version=99999;PS_version=99999;
#
# The query below extracts the IS_version property value:
#
let $IS_version= `SELECT is_version FROM
  (SELECT
    LOCATE('IS_VERSION=', properties) AS pos1,         # find the start of "IS_version=..." substring
    (SELECT SUBSTR(properties, pos1)) AS tail,         # extract the "IS_version=..." substring
    (SELECT LOCATE(';', tail)) AS pos2,                # find the 1st ";" delimiter inside "IS_version=..."
    (SELECT SUBSTR(tail, 12, pos2 - 12)) AS is_version # extract the result ("12" is for the length of "IS_version=")
   FROM mysql.dd_properties) AS a`;
SET SESSION debug= '-d,skip_dd_table_access_check';

#
# 2. Convert the current server binary version into the 99999 format.
#
#
#
let $server_version= `SELECT CONCAT(v1, LPAD(v2, 2, '0'), LPAD(v3, 2, '0')) FROM
  (SELECT
    LOCATE('.', @@version) AS dot1,
    (SELECT SUBSTR(@@version, dot1 + 1)) AS tail1,
    (SELECT LOCATE('.', tail1)) AS dot2,
    (SELECT SUBSTR(tail1, dot2 + 1)) AS tail2,
    (SELECT LOCATE('-', tail2)) AS dash1,
    (SELECT SUBSTR(@@version, 1, dot1 - 1)) AS v1,
    (SELECT SUBSTR(tail1, 1, dot2 - 1)) AS v2,
    (SELECT SUBSTR(tail2, 1, dash1 - 1)) AS v3) a`;

# IMPORTANT! IMPORTANT! IMPORTANT!
#
# If the test below fails with different result values, please
# check if two values are same: "$IS_version" and "$server_version".
#
# 1. The I_S version stored in the database is "$IS_version"..
# 2. The current I_S version in the server's binary is "$server_version".
# --echo $IS_version
# --echo $server_version

# If #1 is equal to #2, then everything is fine and
# you are welcome to update the .result file with the new result.
#
# Otherwise, if #1 and #2 are different, it seems like
# this is a time to increment the IS_property in the DD mysql.properties
# table first!
# To force that update, please check the dd::info_schema::IS_DD_VERSION
# constant in the dd/info_schema/metadata.h file.
# The expected constant value is "$IS_version".
# If so, please update it with the "$server_version" value, and only then
# update the .result file.
# Otherwise, please investigate the divergence.
#
# IMPORTANT! IMPORTANT! IMPORTANT!
--echo # See .test file if this test fails
SELECT * FROM INFORMATION_SCHEMA.keywords;
