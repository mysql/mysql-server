#
# Setups and checks common for all tests in this suite
#
if ($MTR_MEM)
{
  die Cant run these tests with filesystem in RAM;
}

