--disable_query_log
CREATE TABLE log_spam (a INT, b CHAR(100) CHARSET utf8mb4) ENGINE=InnoDB;

CREATE TABLE log_spam_controller (
    id INT NOT NULL PRIMARY KEY,
    status ENUM('Active','Stopping','Stopped')) ENGINE=InnoDB;

INSERT INTO log_spam_controller (id, status) VALUES (1, 'Stopped');

DELIMITER |;
CREATE PROCEDURE log_spam_init()
BEGIN
    DECLARE i INT DEFAULT 1;
    WHILE (i <= 1000) DO
        INSERT INTO log_spam values (i, REPEAT('a',100));
        SET i = i + 1;
    END WHILE;
END|

CREATE PROCEDURE log_spammer()
BEGIN
    DECLARE i INT DEFAULT 1;
    DECLARE stop INT DEFAULT 0;
    UPDATE log_spam_controller SET status = 'Active' WHERE id = 1;
    WHILE (stop = 0) DO
        UPDATE log_spam SET b = RPAD(i, 100, 'x');
        SET i = i + 1;
        SELECT IF(status = 'Active',0,1) FROM log_spam_controller WHERE id = 1 INTO stop;
    END WHILE;
    UPDATE log_spam_controller SET status = 'Stopped' WHERE id = 1;
END|

CREATE PROCEDURE log_spam(IN iterations INT)
BEGIN
    DECLARE i INT DEFAULT 1;
    DECLARE stop INT DEFAULT 0;
    UPDATE log_spam_controller SET status = 'Active' WHERE id = 1;
    WHILE (i < iterations) DO
        UPDATE log_spam SET b = RPAD(i, 100, 'x');
        SET i = i + 1;
    END WHILE;
    UPDATE log_spam_controller SET status = 'Stopped' WHERE id = 1;
END|
DELIMITER ;|

CALL log_spam_init();
DROP PROCEDURE log_spam_init;

--enable_query_log
