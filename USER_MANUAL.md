# USER MANUAL - BOOLEAN data type in MySQL
## System requirements
This installation guide is made with Linux systems in mind. The system used by the developers is Ubuntu 20.0. More or other steps might be needed for Windows or MacOS systems.

The build system used is cmake and make.
## Installation
Clone the repo with SSH or HTTPS
```
#SSH
git clone git@github.com:mbremyk/mysql-server.git
#HTTPS
git clone https://github.com/mbremyk/mysql-server.git
```
Switch to branch boolean 
```
cd mysql-server
git checkout boolean
```
Create and enter build directory
```
mkdir build
cd build
```
Build the project
```
cmake ..
make
```
### Testing
You should now be able to run tests. To run tests move to the mysql-test-directory
```
cd mysql-test
```
and run the mysql test suite
```
./mysql-test-run
./mysql-test-run <name of test>
./mysql-test-run --suite=<suite>
```
New test in this project is `type_boolean.test`. You can also run the main suite with `--suite=main`
### Configuration
If you want to run MySQL with the regular CLI, you need to set up a config file and prepare a data directory. Navigate to where you want your database and create a directory.
```
mkdir mysql
cd mysql
```
Create a data directory
```
mkdir db
```
#### <b>Config file</b>
Edit the [config file](db.cnf) to include the full path to the previously created data directory and relevant files. Any variables needed to be changed should be in angle brackets (<>) with an example after. Move the config file in the previously created mysql-directory.
```
mv db.cnf <path/to/directory/mysql>
```
### Initialization
Return to the build directory
```
cd <path/to/mysql-server/build>
```
and run 
```
./runtime_output_directory/mysqld --defaults-file=<path/to/mysql/db.cnf> --initialize-insecure
```
## Running the database
Run the database with
```
./runtime_output_directory/mysqld --defaults-file=<path/to/mysql/db.cnf>
```
Open another terminal and run
```
./runtime_output_directory/mysql --defaults-file=<path/to/mysql/db.cnf> -uroot
```
## Run queries against the database
Create and use a database with 
```SQL
CREATE DATABASE test;
USE test;
```
Create a BOOLEAN table with
```SQL
CREATE TABLE t (b BOOLEAN);
```
Show that the created table has a BOOLEAN column with
```SQL
SHOW CREATE TABLE t;
```
Insert values into the table with 
```SQL
INSERT INTO t VALUES (TRUE), (FALSE), (NULL);
```
## Shut down the database
Run
```
shutdown;
```
to shut down the database.\
Run 
```
quit;
```
to close the MySQL CLI.