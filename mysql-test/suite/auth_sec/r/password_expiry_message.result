#
# Bug#33822602: Improve MY-010928 your password has expired
#               error on the server to show the user
#
# Create test user and expire the password
CREATE USER u33822602 IDENTIFIED BY 'some_password' PASSWORD EXPIRE;
# Attempt to connect with expired password should fail
# if client cannot handle sandbox mode
mysql: [Warning] Using a password on the command line interface can be insecure.
Please use --connect-expired-password option or invoke mysql in interactive mode.
DROP USER u33822602;
# Check error log for the presence of error message related to password expiry
Pattern "Password for the account 'u33822602'@'localhost' has expired. To log in, either change it using a client that supports expired passwords or send the change request to an administrator." found
