## Script xplugin_drop_user.inc
#  Checks if X Plugin test-user was created. If so, then
#  it is going to delete the user.
#
## Usage
#
# --source ../include/xplugin_drop_user.inc
#

if ($xplugin_create_user)
{
  --disable_query_log
  DROP USER 'x_root'@'localhost';
  --enable_query_log
  --let xplugin_create_user=0
}
