#
# Variable name : protocol_compression_algorithms
# Scope         : Global only
#
# Global - default
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zlib,zstd,uncompressed
# verify all possible values
SET GLOBAL protocol_compression_algorithms= "zstd";
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zstd
SET GLOBAL protocol_compression_algorithms= "zlib";
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zlib
SET GLOBAL protocol_compression_algorithms= "uncompressed";
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
uncompressed
SET GLOBAL protocol_compression_algorithms= "zstd,zlib";
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zstd,zlib
SET GLOBAL protocol_compression_algorithms= "zstd,uncompressed";
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zstd,uncompressed
SET GLOBAL protocol_compression_algorithms= "zlib,uncompressed";
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zlib,uncompressed
SET GLOBAL protocol_compression_algorithms= "zstd,zlib,uncompressed";
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zstd,zlib,uncompressed
SET PERSIST protocol_compression_algorithms= "zstd";
SET PERSIST protocol_compression_algorithms= default;
# verify scope of variable
SELECT @@local.protocol_compression_algorithms;
ERROR HY000: Variable 'protocol_compression_algorithms' is a GLOBAL variable
SELECT @@session.protocol_compression_algorithms;
ERROR HY000: Variable 'protocol_compression_algorithms' is a GLOBAL variable
# verify invalid values
SET GLOBAL protocol_compression_algorithms= 1;
ERROR 42000: Incorrect argument type to variable 'protocol_compression_algorithms'
SET PERSIST protocol_compression_algorithms= 1;
ERROR 42000: Incorrect argument type to variable 'protocol_compression_algorithms'
SET GLOBAL protocol_compression_algorithms= "xyz";
ERROR 42000: Variable 'protocol_compression_algorithms' can't be set to the value of 'xyz'
SET GLOBAL protocol_compression_algorithms= "uncompressed,xyz";
ERROR 42000: Variable 'protocol_compression_algorithms' can't be set to the value of 'uncompressed,xyz'
SET GLOBAL protocol_compression_algorithms= "";
ERROR 42000: Variable 'protocol_compression_algorithms' can't be set to the value of ''
RESET PERSIST;
# End of tests.
