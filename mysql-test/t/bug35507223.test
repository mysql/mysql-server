CREATE TABLE v0(c1 INT);
# Expect an error
# ERROR: 1052 (23000): Column 'c1' in field list is ambiguous
--error ER_NON_UNIQ_ERROR
CREATE TABLE IF NOT EXISTS v2 ( CHECK ( c1 IN ( SELECT DISTINCT * FROM ( v0 a3 ) CROSS JOIN ( v0 ) ON c1 WHERE c1 )  )  )  TABLE v0 ;
DROP TABLE v0;
