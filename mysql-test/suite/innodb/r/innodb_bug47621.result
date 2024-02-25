CREATE TABLE bug47621 (salesperson INT) ENGINE=InnoDB;
ALTER TABLE bug47621 CHANGE salesperson sales_acct_id INT;
create index orgs on bug47621(sales_acct_id);
ALTER TABLE bug47621 CHANGE sales_acct_id salesperson INT;
drop table bug47621;
CREATE TABLE bug47621_sale (
salesperson INT,
PRIMARY KEY(salesperson)) engine = innodb;
CREATE TABLE bug47621_shirt(
id SMALLINT,
owner INT,
FOREIGN KEY(owner)
references bug47621_sale(salesperson) ON DELETE RESTRICT)
engine = innodb;
insert into bug47621_sale values(9);
insert into bug47621_shirt values(1, 9);
ALTER TABLE bug47621_shirt CHANGE id new_id INT;
drop table bug47621_shirt;
ALTER TABLE bug47621_sale CHANGE salesperson sales_acct_id INT;
ALTER TABLE bug47621_sale ADD INDEX idx (sales_acct_id);
drop table bug47621_sale;
