##### suite/funcs_1/include/create_user_no_super.inc
#
# $new_user must contain the name (with @<host> if necessary)
#           of the user to be created

--error 0,1396
eval DROP USER $new_user;
eval CREATE USER $new_user identified by 'PWD';

eval GRANT ALL ON *.* TO $new_user WITH GRANT OPTION;
eval REVOKE SUPER ON *.* FROM $new_user;
