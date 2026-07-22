if (!$install_component_extras)
{
  --die Please source the `cmrs/initialize.inc` before using this file.
}

--replace_regex / SET .*/ SET .../
--eval INSTALL COMPONENT "file://component_mysql_rest_service" SET $install_component_extras
# Lifecycle UDFs in the suite run as the default MTR user.
--exec $MYSQL --execute="GRANT MYSQL_REST_SERVICE_ADMIN ON *.* TO CURRENT_USER()"
