These are the instructions on how to generate test files for the CRL tests
using openSSL.

If you have root access on the system
=====================================

1. Make sure you have the right validity periods in CA.pl and openssl.cnf
2. Create a new certification authority : CA.pl -newca
3. Copy demoCA/cacert.pem to crl-ca-cert.pem
4. Create one server certificate request : CA.pl -newreq
5. Sign the server certificate request : CA.pl -signCA
6. Copy demoCA/newcert.pem to crl-server-cert.pem
7. Remove the key from server's certificate key while copying it :
     openssl rsa -in newkey.pem -out crl-server-key.pem
8. Create one client certificate request : CA.pl -newreq
9. Sign the client certificate request : CA.pl -signCA
10. Copy demoCA/newcert.pem to crl-client-cert.pem
11. Remove the key from client's certificate key while copying it :
     openssl rsa -in newkey.pem -out crl-client-key.pem
12. Create one to-be-revoked client certificate request : CA.pl -newreq
13. Sign the to-be-revoked client certificate request : CA.pl -signCA
14. Copy demoCA/newcert.pem to crl-client-cert-revoked.pem
15. Remove the key from the to-be-revoked client's certificate
  key while copying it :
     openssl rsa -in newkey.pem -out crl-client-key-revoked.pem
16. Revoke the crl-client-invalid-cert.pem :
     openssl ca -revoke crl-client-invalid-cert.pem
17. Generate a CRL file :
     openssl ca -gencrl -crldays=3650 -out crl-client-revoked.crl
18. Clean up all the files in the crldir directory
19. Copy the CRL file into it :
     cp crl-client-revoked.crl `openssl crl -in crl-client-revoked.crl -noout -hash`.r0


If you are using your own CA
============================

Prepare directory
-----------------

1. mkdir new_crlcerts && cd new_crlcerts
2. mkdir crldir
3. mkdir private

Generate CA and 3 set of certificates
-------------------------------------

4. Generate CA
openssl genrsa 2048 > crl-ca-key.pem
openssl req -new -x509 -nodes -days 3650 -key crl-ca-key.pem -out crl-ca-cert.pem

5. Generate Server certificate
openssl req -newkey rsa:2048 -days 3600 -nodes -keyout crl-server-key.pem -out crl-server-req.pem
openssl rsa -in crl-server-key.pem -out crl-server-key.pem
openssl x509 -req -in crl-server-req.pem -days 3600 -CA crl-ca-cert.pem -CAkey crl-ca-key.pem -set_serial 01 -out crl-server-cert.pem

6. Generate Client certificate
openssl req -newkey rsa:2048 -days 3600 -nodes -keyout crl-client-key.pem -out crl-client-req.pem
openssl rsa -in crl-client-key.pem -out crl-client-key.pem
openssl x509 -req -in crl-client-req.pem -days 3600 -CA crl-ca-cert.pem -CAkey crl-ca-key.pem -set_serial 02 -out crl-client-cert.pem

7. Generate Client certificate that will be revoked later
openssl req -newkey rsa:2048 -days 3600 -nodes -keyout crl-client-revoked-key.pem -out crl-client-revoked-req.pem
openssl rsa -in crl-client-revoked-key.pem -out crl-client-revoked-key.pem
openssl x509 -req -in crl-client-revoked-req.pem -days 3600 -CA crl-ca-cert.pem -CAkey crl-ca-key.pem -set_serial 03 -out crl-client-revoked-cert.pem

Prepare for certificate revocation
----------------------------------

8. cp crl-ca-cert.pem cacert.pem
9. cp crl-ca-key.pem private/cakey.pem
10. touch index.txt
11. echo 1000 > crlnumber
12. copy global openssl.cnf to current working dirctory
13. Open local copy of openssl.cnf and in [CA_default] section
    - Update dir to point to current working directory
    - Update certs to point to $dir and not $dir/certs

Revoke a certificate and create crl file
----------------------------------------

14. openssl ca -config openssl.cnf -revoke crl-client-revoked-cert.pem
15. openssl ca -config openssl.cnf -gencrl -crldays 3600 -out crl-client-revoked.crl
16. cp crl-client-revoked.crl `openssl crl -in crl-client-revoked.pem -noout -hash`.r0

Replace existing certs
----------------------
17. Replace following files in <src>/mysql-test/std_data/ with files generated above
    crl-ca-cert.pem
    crl-client-cert.pem
    crl-client-key.pem
    crl-client-revoked-cert.pem
    crl-client-revoked-key.pem
    crl-client-revoked.crl
    crl-server-cert.pem
    crl-server-key.pem

18. Remove file in <src>/mysql-test/std_data/crldir
19. Copy file generated in step 16 above to <src>/mysql-test/std_data/crldir
20. You may now remove new_crls directory
