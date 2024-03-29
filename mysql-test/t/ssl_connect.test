-- source include/big_test.inc
# Repeat connect/disconnect

let $i=100;
while ($i)
{
  connect (test_con1,localhost,root,,,,,SSL);
  disconnect test_con1;
  dec $i;
}
echo completed;

##  This test file is for testing encrypted communication only, not other
##  encryption routines that the SSL library happens to provide!
