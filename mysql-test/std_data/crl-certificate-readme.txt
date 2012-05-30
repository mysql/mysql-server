These are the instructions on how to generate test files for the CRL tests
using openSSL.

1. Make sure you have the right validity periods in CA.pl and openssl.cnf
2. Create a new certification authority : CA.pl -newca
3. Copy demoCA/cacert.pem to crl-ca-cert.pem
4. Create one server certificate request : CA.pl -newreq
5. Sign the server certificate request : CA.pl -signreq
6. Copy demoCA/newcert.pem to crl-server-cert.pem
7. Remove the key from server's certificate key while copying it :
     openssl rsa -in newkey.pem -out crl-server-key.pem
8. Create one client certificate request : CA.pl -newreq
9. Sign the client certificate request : CA.pl -signreq
10. Copy demoCA/newcert.pem to crl-client-cert.pem
11. Remove the key from client's certificate key while copying it :
     openssl rsa -in newkey.pem -out crl-client-key.pem
12. Create one to-be-revoked client certificate request : CA.pl -newreq
13. Sign the to-be-revoked client certificate request : CA.pl -signreq
14. Copy demoCA/newcert.pem to crl-client-invalid-cert.pem
15. Remove the key from the to-be-revoked client's certificate
  key while copying it :
     openssl rsa -in newkey.pem -out crl-client-invalid-key.pem
16. Revoke the crl-client-invalid-cert.pem :
     openssl ca -revoke crl-client-invalid-key.pem
17. Generate a CRL file :
     openssl ca -gencrl -crldays=3650 -out crl-client-revoked.crl
18. Clean up all the files in the crldir directory
19. Copy the CA certificate into it :
     cp crl-ca-cert.pem `openssl -in crl-ca-cert.pem -noout -hash`.0
20. Copy the CRL file into it :
     cp crl-client-revoked.crl `openssl -in crl-ca-cert.pem -noout -hash`.r0
