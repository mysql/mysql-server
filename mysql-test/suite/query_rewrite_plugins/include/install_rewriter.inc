disable_query_log;

CREATE DATABASE IF NOT EXISTS query_rewrite;

CREATE TABLE IF NOT EXISTS query_rewrite.rewrite_rules (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  pattern VARCHAR(5000) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  pattern_database VARCHAR(20) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin,
  replacement VARCHAR(5000) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  enabled ENUM('YES', 'NO') CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL
    DEFAULT 'YES',
  message VARCHAR(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin
) DEFAULT CHARSET = utf8mb4 ENGINE = INNODB;

eval INSTALL PLUGIN rewriter SONAME '$REWRITER';
eval CREATE FUNCTION load_rewrite_rules RETURNS STRING SONAME '$REWRITER';

DELIMITER //;

CREATE PROCEDURE query_rewrite.flush_rewrite_rules()
BEGIN
  DECLARE message_text VARCHAR(100);
  COMMIT;
  SELECT load_rewrite_rules() INTO message_text;
  IF NOT message_text IS NULL THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = message_text;
  END IF;
END //

DELIMITER ;//

enable_query_log;
echo # Query rewrite plugin was installed.;
