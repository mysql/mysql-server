#
# Bug #19471516 SERVER CRASHES WHEN EXECUTING ALTER TABLE ADD
# FOREIGN KEY
#
CREATE TABLE `parent` (`parent_id` INT, PRIMARY KEY (`parent_id`));
CREATE TABLE `child1` (`id` INT ,`child1_fk1` INT, `child1_fk2` INT,
PRIMARY KEY (`id`));
CREATE TABLE `child2` (`id` INT, `child2_fk1` INT, `child2_fk2` INT,
PRIMARY KEY (`id`));
CREATE TABLE `child3` (`id` INT , `child3_fk1` INT, PRIMARY KEY (`id`));
ALTER TABLE `child1` ADD FOREIGN KEY (`child1_fk1`) REFERENCES `parent`
(`parent_id`);
ALTER TABLE `child1` ADD FOREIGN KEY (`child1_fk1`) REFERENCES
`parent` (`parent_id`);
ALTER TABLE `child1` ADD FOREIGN KEY (`child1_fk2`) REFERENCES `parent`
(`parent_id`);
ALTER TABLE `child2` ADD FOREIGN KEY (`child2_fk1`) REFERENCES `parent`
(`parent_id`);
ALTER TABLE `child2` ADD FOREIGN KEY (`child2_fk2`) REFERENCES `parent`
(`parent_id`);
ALTER TABLE `child3` ADD FOREIGN KEY (`child3_fk1`) REFERENCES `parent`
(`parent_id`);
ALTER TABLE `child1` ADD FOREIGN KEY (`child1_fk2`) REFERENCES
`parent` (`parent_id`);
ALTER TABLE `child2` ADD FOREIGN KEY (`child2_fk1`) REFERENCES
`parent` (`parent_id`);
ALTER TABLE `child2` ADD FOREIGN KEY (`child2_fk2`) REFERENCES
`parent` (`parent_id`);
ALTER TABLE `child3` ADD FOREIGN KEY (`child3_fk1`) REFERENCES
`parent` (`parent_id`);
drop table child3, child2, child1, parent;
