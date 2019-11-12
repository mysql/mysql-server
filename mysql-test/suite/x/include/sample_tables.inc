
CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;
DELIMITER //;

CREATE PROCEDURE recreate_tables()
BEGIN
  CREATE TABLE IF NOT EXISTS xtable (phrase VARCHAR(30), prio INTEGER);
  TRUNCATE TABLE xtable;
  INSERT INTO xtable (phrase, prio) VALUES ('Alice has a cat', 1);
  INSERT INTO xtable (phrase, prio) VALUES ('Bob has a dog 1', 2);
  INSERT INTO xtable (phrase, prio) VALUES ('Peppa has a teddy', 1);
  INSERT INTO xtable (phrase, prio) VALUES ('Bob has a dog 2', 3);
  INSERT INTO xtable (phrase, prio) VALUES ('Row to delete', 4);

  CREATE TABLE IF NOT EXISTS xtable_with_pk (p_id INTEGER AUTO_INCREMENT PRIMARY KEY, description varchar(40));
  TRUNCATE TABLE xtable_with_pk;
  INSERT INTO xtable_with_pk (description) VALUES ('MyRow');
END //

DELIMITER ;//
