--source include/not_windows.inc
#
# Case-1: Single-Byte characters
#

# Max length for schema name is 64 characters. 
CREATE SCHEMA `aaaaaaaabbbbbbbbccccccccddddddddeeeeeeeeffffffffgggggggghhhhhhhh`; 
DROP SCHEMA `aaaaaaaabbbbbbbbccccccccddddddddeeeeeeeeffffffffgggggggghhhhhhhh`;

--error ER_TOO_LONG_IDENT
CREATE SCHEMA `aaaaaaaabbbbbbbbccccccccddddddddeeeeeeeeffffffffgggggggghhhhhhhhx`;

#
# Case-2: Multi-Byte characters
#

# The utf8mb4 char '𠜱' expands to '@003f', and using 55 of them makes path 257 (255 + 2) chars long.
CREATE SCHEMA `𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱`; 
DROP SCHEMA `𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱`; 

# An attempt to use more than 55 of such chars makes `stat` system call in UNIX-like systems to fail 
# with `ENAMETOOLONG`. 
--replace_regex /OS errno.*/OS error)/
--error 13
CREATE SCHEMA `𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱𠜱a`;

