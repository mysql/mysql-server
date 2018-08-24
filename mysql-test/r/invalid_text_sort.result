DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (
tx TEXT CHARACTER SET utf8mb3 NULL,
pk INTEGER AUTO_INCREMENT,
PRIMARY KEY (pk)
) ENGINE=myisam;
Warnings:
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES
(
'valid-string',
NULL
)
,
(
UNHEX('FF'),
NULL
)
;
Warnings:
Warning	1366	Incorrect string value: '\xFF' for column 'tx' at row 2
SELECT pk,OCTET_LENGTH(tx),LENGTH(tx) FROM t1;
pk	OCTET_LENGTH(tx)	LENGTH(tx)
1	12	12
2	0	0
SELECT tx FROM t1 ORDER BY tx DESC;
tx
valid-string

DROP TABLE t1;
