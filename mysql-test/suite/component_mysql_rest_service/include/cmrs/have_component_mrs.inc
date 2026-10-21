# Including this file will reserve 10 TCP ports for the Component MRS suite.
#

if(!$MTR_COMPONENT_MRS_PORT_OFFSET)
{
  --die MTR must set this variable, when this file is "sourced" to the test.
}