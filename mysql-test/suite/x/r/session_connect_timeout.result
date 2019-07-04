call mtr.add_suppression("1: Error creating session for connection from .*");
# restart: --debug=d,client_accept_timeout
Application terminated with expected error: Read operation failed because of a timeout (code 2500)
ok
# restart: --debug=d,resolve_timeout
Application terminated with expected error: Read operation failed because of a timeout (code 2500)
ok
# restart: --debug=d,authentication_timeout
Application terminated with expected error: Read operation failed because of a timeout (code 2500)
ok
# restart: 
