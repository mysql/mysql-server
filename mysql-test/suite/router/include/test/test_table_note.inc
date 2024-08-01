--disable_result_log
--disable_query_log

CREATE TABLE IF NOT EXISTS `test`.`note` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `user_id` BINARY(16) NOT NULL,
  `title` VARCHAR(255) NOT NULL,
  `create_date` DATETIME NOT NULL DEFAULT now(),
  `last_update` DATETIME NOT NULL DEFAULT now(),
  `pinned` BIT(1) NOT NULL DEFAULT 0,
  `locked_down` BIT(1) NOT NULL DEFAULT 0,
  `shared` BIT(1) NOT NULL DEFAULT 0 COMMENT 'The shared column is automatically updated by AFTER INSERT / DELETE triggers on user_has_note.',
  `content` VARCHAR(2000) NULL,
  `tags` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `created_by` (`user_id` ASC) VISIBLE,
  INDEX `last_update` (`last_update` ASC) VISIBLE,
  INDEX `create_date` (`create_date` ASC) VISIBLE,
  FULLTEXT INDEX `content_ft` (`content`) VISIBLE)
ENGINE = InnoDB;

use mysql_rest_service_metadata;
START TRANSACTION;
if ($mrs_schema_version == 2)
{
  INSERT INTO `db_object` (`id`,`db_schema_id`,`name`,`request_path`,`enabled`,`object_type`,`crud_operations`,`format`,`items_per_page`,`media_type`,`auto_detect_media_type`,`requires_auth`,`auth_stored_procedure`,`row_user_ownership_enforced`,`row_user_ownership_column`,`options`,`details`,`comments`) VALUES (1,@schema_id,'note','/note',1,'TABLE','CREATE,READ,UPDATE,DELETE','FEED',NULL,NULL,0,1,'',1,'user_id',NULL,NULL,'');
  INSERT INTO `object` (`id`,`db_object_id`,`name`,`kind`,`position`,`sdk_options`,`comments`) VALUES (2,1,'MyServiceMrsNotesNote','RESULT',0,NULL,NULL);
}

if ($mrs_schema_version == 3)
{
  INSERT INTO `db_object` (`id`,`db_schema_id`,`name`,`request_path`,`enabled`,`object_type`,`crud_operations`,`format`,`items_per_page`,`media_type`,`auto_detect_media_type`,`requires_auth`,`auth_stored_procedure`, `options`,`details`,`comments`) VALUES (1,@schema_id,'note','/note',1,'TABLE','CREATE,READ,UPDATE,DELETE','FEED',NULL,NULL,0,1,'',NULL,NULL,'');
  INSERT INTO `object` (`id`,`db_object_id`,`name`,`kind`,`position`,`sdk_options`,`comments`, `row_ownership_field_id`,`options`) VALUES (2,1,'MyServiceMrsNotesNote','RESULT',0,NULL,NULL, 9, '{"duality_view_insert":true, "duality_view_update":true, "duality_view_delete":true}');
}

#INSERT INTO `object_field` #(`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES #(3,2,NULL,?,'userHasNote',1001,NULL,0,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (4,2,NULL,NULL,'title',3,'{\"name\": \"title\", \"comment\": \"\", \"datatype\": \"varchar(255)\", \"not_null\": true, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (5,2,NULL,NULL,'id',1,'{\"name\": \"id\", \"comment\": \"\", \"datatype\": \"int unsigned\", \"not_null\": true, \"is_unique\": false, \"is_primary\": true, \"is_generated\": false, \"id_generation\": \"auto_inc\"}',1,1,1,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (6,2,NULL,NULL,'lastUpdate',5,'{\"name\": \"last_update\", \"comment\": \"\", \"datatype\": \"datetime\", \"not_null\": true, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
#INSERT INTO `object_field` #(`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES #(7,2,NULL,?,'user',102,NULL,0,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (8,2,NULL,NULL,'pinned',6,'{\"name\": \"pinned\", \"comment\": \"\", \"datatype\": \"bit(1)\", \"not_null\": true, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (9,2,NULL,NULL,'userId',2,'{\"name\": \"user_id\", \"comment\": \"\", \"datatype\": \"binary(16)\", \"not_null\": true, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (10,2,NULL,NULL,'shared',8,'{\"name\": \"shared\", \"comment\": \"The shared column is automatically updated by AFTER INSERT / DELETE triggers on user_has_note.\", \"datatype\": \"bit(1)\", \"not_null\": true, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (11,2,NULL,NULL,'tags',10,'{\"name\": \"tags\", \"comment\": \"\", \"datatype\": \"json\", \"not_null\": false, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (12,2,NULL,NULL,'createDate',4,'{\"name\": \"create_date\", \"comment\": \"\", \"datatype\": \"datetime\", \"not_null\": true, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (13,2,NULL,NULL,'lockedDown',7,'{\"name\": \"locked_down\", \"comment\": \"\", \"datatype\": \"bit(1)\", \"not_null\": true, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
INSERT INTO `object_field` (`id`,`object_id`,`parent_reference_id`,`represents_reference_id`,`name`,`position`,`db_column`,`enabled`,`allow_filtering`,`allow_sorting`,`no_check`,`no_update`,`sdk_options`,`comments`) VALUES (14,2,NULL,NULL,'content',9,'{\"name\": \"content\", \"comment\": \"\", \"datatype\": \"varchar(2000)\", \"not_null\": false, \"is_unique\": false, \"is_primary\": false, \"is_generated\": false, \"id_generation\": null}',1,1,0,0,0,NULL,NULL);
COMMIT;
use test;
--enable_result_log
--enable_query_log