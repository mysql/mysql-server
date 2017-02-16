#
# VARBINARY column types
#

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

let $create_definition =
  v0 VARBINARY(0) $col_opts,
  v1 VARBINARY(1) $col_opts,
  v64 VARBINARY(64) $col_opts,
  v65000 VARBINARY(65000) $col_opts 
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = VARBINARY types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  --let $create_definition = v VARBINARY(65532) $col_opts
  --let $table_name = t2
  --source create_table.inc
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t2;

  # Valid values 

  INSERT INTO t1 (v0,v1,v64,v65000) VALUES ('','','','');
  INSERT INTO t1 (v0,v1,v64,v65000) VALUES ('','y','Once there, double check that an article doesn\'t already exist','Here is a list of recommended books on MariaDB and MySQL. We\'ve provided links to Amazon.com here for convenience, but they can be found at many other bookstores, both online and off.

  If you want to have your favorite MySQL / MariaDB book listed here, please leave a comment.
  For developers who want to code on MariaDB or MySQL

      * Understanding MySQL Internals by Sasha Pachev, former MySQL developer at MySQL AB.
            o This is the only book we know about that describes the internals of MariaDB / MySQL. A must have for anyone who wants to understand and develop on MariaDB!
            o Not all topics are covered and some parts are slightly outdated, but still the best book on this topic. 
      * MySQL 5.1 Plugin Development by Sergei Golubchik and Andrew Hutchings
            o A must read for anyone wanting to write a plugin for MariaDB, written by the Sergei who designed the plugin interface for MySQL and MariaDB! 

  For MariaDB / MySQL end users

      * MariaDB Crash Course by Ben Forta
            o First MariaDB book!
            o For people who want to learn SQL and the basics of MariaDB.
            o Now shipping. Purchase at Amazon.com or your favorite bookseller. 

      * SQL-99 Complete, Really by Peter Gulutzan & Trudy Pelzer.
            o Everything you wanted to know about the SQL 99 standard. Excellent reference book!
            o Free to read in the Knowledgebase! 

      * MySQL (4th Edition) by Paul DuBois
            o The \'default\' book to read if you wont to learn to use MySQL / MariaDB. 

      * MySQL Cookbook by Paul DuBois
            o A lot of examples of how to use MySQL. As with all of Paul\'s books, it\'s worth its weight in gold and even enjoyable reading for such a \'dry\' subject. 

      * High Performance MySQL, Second Edition, By Baron Schwartz, Peter Zaitsev, Vadim Tkachenko, Jeremy D. Zawodny, Arjen Lentz, Derek J. Balling, et al.
            o \"High Performance MySQL is the definitive guide to building fast, reliable systems with MySQL. Written by noted experts with years of real-world experience building very large systems, this book covers every aspect of MySQL performance in detail, and focuses on robustness, security, and data integrity. Learn advanced techniques in depth so you can bring out MySQL\'s full power.\" (From the book description at O\'Reilly) 

      * MySQL Admin Cookbook
            o A quick step-by-step guide for MySQL users and database administrators to tackle real-world challenges with MySQL configuration and administration 

      * MySQL 5.0 Certification Study Guide, By Paul DuBois, Stefan Hinz, Carsten Pedersen
            o This is the official guide to cover the passing of the two MySQL Certification examinations. It is valid till version 5.0 of the server, so while it misses all the features available in MySQL 5.1 and greater (including MariaDB 5.1 and greater), it provides a good basic understanding of MySQL for the end-user. ');

  --sorted_result
  SELECT HEX(v0), HEX(v1), HEX(v64), HEX(v65000) FROM t1;

  # Invalid values

  INSERT INTO t1 (v0,v1,v64,v65000) VALUES ('y', 'yy', REPEAT('c',65), REPEAT('abcdefghi ',6501));
  INSERT INTO t1 (v0,v1,v64,v65000) SELECT v65000, v65000, v65000, CONCAT(v65000,v1) FROM t1;

  --sorted_result
  SELECT HEX(v0), HEX(v1), HEX(v64), LENGTH(HEX(v65000)) FROM t1;

  --let $alter_definition = ADD COLUMN v65536 VARBINARY(65536) $col_opts
  --source alter_table.inc
  if ($mysql_errname)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE or BLOB types
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
      --replace_column 3 # 4 # 5 #
      SHOW COLUMNS IN t1;
  }

  DROP TABLE t1, t2;
}


