
# Remove the test grant before the component unregisters the privilege.
--exec $MYSQL --execute="REVOKE MYSQL_REST_SERVICE_ADMIN ON *.* FROM CURRENT_USER()"
UNINSTALL COMPONENT "file://component_mysql_rest_service";
