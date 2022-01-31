CREATE TABLE connections (
  trx_mysql_thread_id BIGINT PRIMARY KEY,
  name VARCHAR(200)
) ENGINE=InnoDB;

DELIMITER //;
CREATE PROCEDURE show_locks ()
BEGIN
  # this is needed because INFORMATION_SCHEMA.INNODB_TRX is refreshed with debounce 100ms
  SELECT SLEEP(1);
  SELECT
    connections.name, object_name, index_name, lock_type, lock_mode, lock_status, lock_data
  FROM connections
  JOIN INFORMATION_SCHEMA.INNODB_TRX USING (trx_mysql_thread_id)
  JOIN performance_schema.data_locks ON (trx_id=engine_transaction_id)
  ORDER BY 1,2,3,4,5,6,7;
END //

CREATE PROCEDURE register_connection (name VARCHAR(200))
BEGIN
  INSERT INTO connections VALUES (CONNECTION_ID(), name);
END //

DELIMITER ;//