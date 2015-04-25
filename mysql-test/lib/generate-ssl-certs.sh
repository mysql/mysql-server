#/bin/sh -xe

# simply run me from mysql-test/
cd std_data/

# boilerplace for "openssl ca" and /etc/ssl/openssl.cnf
rm -rf demoCA
mkdir demoCA demoCA/private demoCA/newcerts
touch demoCA/index.txt
echo 01 > demoCA/serial

# CA certificate, self-signed
openssl req -x509 -newkey rsa:2048 -keyout demoCA/private/cakey.pem -out cacert.pem -days 7300 -nodes -subj '/CN=cacert/C=FI/ST=Helsinki/L=Helsinki/O=MariaDB' -text

# server certificate signing request and private key. Note the very long subject (for MDEV-7859)
openssl req -newkey rsa:1024 -keyout server-key.pem -out demoCA/server-req.pem -days 7300 -nodes -subj '/CN=localhost/C=FI/ST=state or province within country, in other certificates in this file it is the same as L/L=location, usually an address but often ambiguously used/OU=organizational unit name, a division name within an organization/O=organization name, typically a company name'
# convert the key to yassl compatible format
openssl rsa -in server-key.pem -out server-key.pem
# sign the server certificate with CA certificate
openssl ca -days 7300 -batch -cert cacert.pem -policy policy_anything -out server-cert.pem -infiles demoCA/server-req.pem

openssl req -newkey rsa:8192 -keyout server8k-key.pem -out demoCA/server8k-req.pem -days 7300 -nodes -subj '/CN=server8k/C=FI/ST=Helsinki/L=Helsinki/O=MariaDB'
openssl rsa -in server8k-key.pem -out server8k-key.pem
openssl ca -days 7300 -batch -cert cacert.pem -policy policy_anything -out server8k-cert.pem -infiles demoCA/server8k-req.pem

openssl req -newkey rsa:1024 -keyout client-key.pem -out demoCA/client-req.pem -days 7300 -nodes -subj '/CN=client/C=FI/ST=Helsinki/L=Helsinki/O=MariaDB'
openssl rsa -in client-key.pem -out client-key.pem
openssl ca -days 7300 -batch -cert cacert.pem -policy policy_anything -out client-cert.pem -infiles demoCA/client-req.pem

rm -rf demoCA
