call mtr.add_suppression("Error in diagnostics area: MY-001317 - Query execution was interrupted");
CREATE SCHEMA sakila;
USE sakila;
CREATE TABLE inventory (
inventory_id INT NOT NULL AUTO_INCREMENT,
PRIMARY KEY (inventory_id)
);
CREATE TABLE rental (
rental_id INT NOT NULL AUTO_INCREMENT,
inventory_id INT NOT NULL,
PRIMARY KEY (rental_id),
CONSTRAINT fk_rental_inventory FOREIGN KEY (inventory_id) REFERENCES inventory (inventory_id)
);
CREATE TABLE payment (
payment_id INT NOT NULL AUTO_INCREMENT,
rental_id INT DEFAULT NULL,
amount DECIMAL(5,2) NOT NULL,
PRIMARY KEY (payment_id),
CONSTRAINT fk_payment_rental FOREIGN KEY (rental_id) REFERENCES rental (rental_id)
);
RUN START TRANSACTION

0 rows affected
RUN UPDATE sakila.payment SET amount = amount + 1 WHERE payment_id = 5068

0 rows affected
Rows matched: 0  Changed: 0  Warnings: 0
connecting...
active session is now 'second'
RUN SELECT connection_id() AS Value;
RUN OPTIMIZE TABLE sakila.inventory;
switched to session default
RUN KILL $ID

0 rows affected
switched to session second
Table	Op	Msg_type	Msg_text
sakila.inventory	optimize	note	Table does not support optimize, doing recreate + analyze instead
sakila.inventory	optimize	status	Operation failed
0 rows affected
[ASSERT notice warning - ERROR, 3169]
RUN SELECT 1
While executing SELECT 1:
Got expected error: MySQL server has gone away (code 2006)
Server disconnected
closing session second
switched to session default
RUN COMMIT

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
RUN START TRANSACTION

0 rows affected
RUN UPDATE sakila.payment SET amount = amount + 1 WHERE payment_id = 5068

0 rows affected
Rows matched: 0  Changed: 0  Warnings: 0
connecting...
active session is now 'second'
RUN SELECT connection_id() AS Value;
RUN SELECT * FROM sakila.inventory;
switched to session default
RUN KILL $ID

0 rows affected
switched to session second
[ASSERT notice warning - ERROR, 3169]
Server disconnected
closing session second
switched to session default
RUN COMMIT

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP SCHEMA sakila;
