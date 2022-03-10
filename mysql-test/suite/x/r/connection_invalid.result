
## Test verifies invalid cross connections
#
# 1. Verify failure at cross connecting classic-client to x-server
# 2. Verify failure at cross connecting x-client to classic-server
#
# Where classic server is old mysql protocol that usually resides
# on tcp port 3306.
#
# Where x-server is the x-protocol that usually resides on tcp port
# 33060.
#

#
# 1. Verify failure at cross connecting classic-client to x-server
ERROR 2007 (HY000): Protocol mismatch; server version = 11, client version = 10

#
# 2. Verify failure at cross connecting x-client to classic-server
Application terminated with expected error: MySQL server has gone away (code 2006)
ok
