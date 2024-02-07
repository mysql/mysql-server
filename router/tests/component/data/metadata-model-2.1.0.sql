/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
  Metadata Schema Version Change Log
  ----------------------------------
  * 1.0.1 - Initial version with support for InnoDB Clusters based on Group
            Replication technology.
  * 2.0.0 - Added support for ReplicaSets and a public interface for
            components like MySQL Router. Cleanup and removal of unused items.
  * 2.1.0 - Added support for ClusterSets

  Description
  -----------
  All the tables that contain information about the topology of MySQL servers
  in a MySQL InnoDB Cluster setup are stored in the
  mysql_innodb_cluster_metadata database.

  Consumers of the metadata (i.e. MySQL Router) are required to query the
  public interface items (VIEWs and STORED PROCEDUREs) instead of querying the
  actual metadata tables. The public interface items are prefixed with the
  corresponding major version of the metadata, e.g. 'v2_clusters'.
*/

CREATE DATABASE IF NOT EXISTS mysql_innodb_cluster_metadata DEFAULT CHARACTER SET utf8mb4;
USE mysql_innodb_cluster_metadata;

SET names utf8mb4;

--  Metadata Schema Version
--  -----------------------

/*
  View that holds the current version of the metadata schema.

  PLEASE NOTE:
  During the upgrade process of the metadata schema the schema_version is
  set to 0, 0, 0. This behavior is used for other components to detect a
  schema upgrade process and hold back metadata refreshes during upgrades.
*/
DROP VIEW IF EXISTS schema_version;
CREATE SQL SECURITY INVOKER VIEW schema_version (major, minor, patch) AS SELECT 2, 1, 0;


--  GR Cluster Tables
--  -----------------

/*
  ClusterSets, or multiple clusters that replicate each other.
*/
CREATE TABLE IF NOT EXISTS clustersets (
  /*
    Unique ID of a clusterset record.
  */
  `clusterset_id` VARCHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci,

  /*
    Unique data domain name for the clusterset.
   */
  `domain_name` VARCHAR(63) NOT NULL,

  /*
    Global clusterset options set by the user from the Shell.
   */
  `options` JSON,

  /*
    Global attributes set by the Shell.
   */
  `attributes` JSON,

  /*
    Clusterset wide options meant for Router, set by the Shell.
   */
  `router_options` JSON,

  /*
    Topology type between clusters.
   */
  `topology_type` ENUM('SINGLE-PRIMARY-STAR'),

  PRIMARY KEY (`clusterset_id`)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;


/*
  Basic information about clusters in general.

  Both InnoDB clusters and replicasets are represented as clusters in
  this table, with different cluster_type values.
*/
CREATE TABLE IF NOT EXISTS clusters (
  /*
    unique ID used to distinguish the cluster from other clusters
  */
  `cluster_id` CHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci,
  /*
    user specified name for the cluster. cluster_name must be unique
  */
  `cluster_name` VARCHAR(63) NOT NULL,
  /*
    Brief description of the cluster.
  */
  `description` TEXT,
  /*
    Cluster options explicitly set by the user from the Shell.
   */
  `options` JSON,
   /*
    Contain attributes assigned to each cluster.
    The attributes can be used to tag the clusters with custom attributes.
    {
      group_replication_group_name: "254616cc-fb47-11e5-aac5"
    }
   */
  `attributes` JSON,
  /*
   Whether this is a GR or AR "cluster" (that is, an InnoDB cluster or an
   Async ReplicaSet).
   */
  `cluster_type` ENUM('gr', 'ar') NOT NULL,
  /*
    Specifies the type of topology of a cluster as last known by the shell.
    This is just a snapshot of the GR configuration and should be automatically
    refreshed by the shell.
  */
  `primary_mode` ENUM('pm', 'mm') NOT NULL DEFAULT 'pm',

  /*
    Default options for Routers.
   */
  `router_options` JSON,

  /*
    Id of the clusterset this cluster belongs to, if any.
   */
  `clusterset_id` VARCHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,

  PRIMARY KEY(cluster_id),
  UNIQUE KEY(cluster_name)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;


/*
  Managed MySQL server instances and basic information about them.
*/
CREATE TABLE IF NOT EXISTS instances (
  /*
    The ID of the server instance and is a unique identifier of the server
    instance.
  */
  `instance_id` INT UNSIGNED AUTO_INCREMENT,
  /*
    Cluster ID that the server belongs to.
  */
  `cluster_id` CHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,
  /*
    network address of the host of the instance.
    Taken from @@report_host or @@hostname
  */
  `address` VARCHAR(265) CHARACTER SET ascii COLLATE ascii_general_ci,
  /*
    MySQL generated server_uuid for the instance
  */
  `mysql_server_uuid` CHAR(36) NOT NULL,
  /*
    Unique, user specified name for the server.
    Default is address:port (must fit at least 255 chars of address + port)
   */
  `instance_name` VARCHAR(265) NOT NULL,
  /*
    A JSON document with the addresses available for the server instance. The
    protocols and addresses are further described in the Protocol section below.
    {
      mysqlClassic: "host.foo.com:3306",
      mysqlX: "host.foo.com:33060",
      grLocal: "host.foo.com:49213"
    }
  */
  `addresses` JSON NOT NULL,
  /*
    Contain attributes assigned to the server and is a JSON data type with
    key-value pair. The attributes can be used to tag the servers with custom
    attributes.
  */
  `attributes` JSON,
  /*
    An optional brief description of the group.
  */
  `description` TEXT,

  PRIMARY KEY(instance_id, cluster_id),
  FOREIGN KEY (cluster_id) REFERENCES clusters(cluster_id)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;


--  AR ReplicaSet Tables
--  ---------------------

/*
  A "view" of the topology of a replicaset at a given point in time.
  Every time the topology of the replicaset changes (added and removed instances
  and failovers), a new record is added here (and in async_cluster_members).

  The most current view will be the one with the highest view_id.

  This table maintains a history of replicaset configurations, but older
  records may get deleted.
 */
CREATE TABLE IF NOT EXISTS async_cluster_views (
  `cluster_id` CHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,

  `view_id` INT UNSIGNED NOT NULL,

  /*
    The type of the glboal topology between clusters.
    Changing the topology_type is not currently supported, so this value
    must always be copied when a view changes.
   */
  `topology_type` ENUM('SINGLE-PRIMARY-TREE'),

  /*
    What caused the view change. Possible values are pre-defined by the shell.
   */
  `view_change_reason` VARCHAR(32) NOT NULL,

  /*
    Timestamp of the change.
   */
  `view_change_time` TIMESTAMP(6) NOT NULL,

  /*
    Information about the cause for a view change.
   */
  `view_change_info` JSON NOT NULL,

  `attributes` JSON NOT NULL,

  PRIMARY KEY (cluster_id, view_id),
  INDEX (view_id),

  FOREIGN KEY (cluster_id)
    REFERENCES clusters (cluster_id)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;


/*
  The instances that are part of a given view, along with their replication
  master. The PRIMARY will have a NULL master.
*/
CREATE TABLE IF NOT EXISTS async_cluster_members (
  `cluster_id` CHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,

  `view_id` INT UNSIGNED NOT NULL,

  /*
    Reference to an instance in the instances table.
    This reference may become invalid for older views, for instances that are
    removed from the replicaset.
  */
  `instance_id` INT UNSIGNED NOT NULL,

  /*
    id of the master of this instance. NULL if this is the PRIMARY.
   */
  `master_instance_id` INT UNSIGNED,

  /*
    TRUE if this is the PRIMARY of the replicaset.
   */
  `primary_master` BOOL NOT NULL,

  `attributes` JSON NOT NULL,

  PRIMARY KEY (cluster_id, view_id, instance_id),
  INDEX (view_id),
  INDEX (instance_id),

  FOREIGN KEY (cluster_id, view_id)
      REFERENCES async_cluster_views (cluster_id, view_id)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;



--  ClusterSet Tables
--  -----------------

/*
  A "view" of the topology of a clusterset at a given point in time.
  Every time the topology of the clusterset changes (added and removed clusters
  and failovers), a new record is added here (and in clusterset_members).

  The most current view will be the one with the highest view_id.

  This table maintains a history of clusterset configurations, but older
  records may get deleted.
 */
CREATE TABLE IF NOT EXISTS clusterset_views (
  `clusterset_id` VARCHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,

  `view_id` BIGINT UNSIGNED NOT NULL,

  /*
    The type of the global topology between clusters.
    Changing the topology_type is not currently supported, so this value
    must always be copied when a view changes.
   */
  `topology_type` ENUM('SINGLE-PRIMARY-TREE'),

  /*
    What caused the view change. Possible values are pre-defined by the shell.
   */
  `view_change_reason` VARCHAR(32) NOT NULL,

  /*
    Timestamp of the change.
   */
  `view_change_time` TIMESTAMP(6) NOT NULL,

  /*
    Information about the cause for a view change.
   */
  `view_change_info` JSON NOT NULL,

  `attributes` JSON NOT NULL,

  PRIMARY KEY (clusterset_id, view_id),
  INDEX (view_id),

  FOREIGN KEY (clusterset_id)
    REFERENCES clustersets (clusterset_id)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;


/*
  The instances that are part of a given view, along with their replication
  master. The PRIMARY will have a NULL master.
*/
CREATE TABLE IF NOT EXISTS clusterset_members (
  `clusterset_id` VARCHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,

  `view_id` BIGINT UNSIGNED NOT NULL,

  /*
    Reference to a cluster in the clusters table.
    This reference may become invalid for older views, for clusters that are
    removed from the clusterset.
  */
  `cluster_id` VARCHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,

  /*
    id of the master cluster of this cluster. NULL if this is the PRIMARY.
   */
  `master_cluster_id` VARCHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci,

  /*
    TRUE if this is the PRIMARY of the clusterset.
   */
  `primary_cluster` BOOL NOT NULL,

  `attributes` JSON NOT NULL,

  PRIMARY KEY (clusterset_id, view_id, cluster_id),
  INDEX (view_id),
  INDEX (cluster_id),

  FOREIGN KEY (clusterset_id, view_id)
      REFERENCES clusterset_views (clusterset_id, view_id)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;



/*
  This table contain a list of all router instances that are tracked by the
  cluster.
*/
CREATE TABLE IF NOT EXISTS routers (
  /*
    The ID of the router instance and is a unique identifier of the server
    instance.
  */
  `router_id` INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
  /*
    A user specified name for an instance of the router.
    Should default to address:port, where port is the RW port for classic
    protocol.
   */
  `router_name` VARCHAR(265) NOT NULL,
  /*
    The product name of the routing component, e.g. 'MySQL Router'
   */
  `product_name` VARCHAR(128) NOT NULL,
  /*
    network address of the host the Router is running on.
  */
  `address` VARCHAR(256) CHARACTER SET ascii COLLATE ascii_general_ci,
  /*
    The version of the router instance. Updated on bootstrap and each startup
    of the router instance. Format: x.y.z, 3 digits for each component.
    Managed by Router.
   */
  `version` VARCHAR(12) DEFAULT NULL,
  /*
    A timestamp updated by the router every hour with the current time. This
    timestamp is used to detect routers that are no longer used or stalled.
    Managed by Router.
   */
  `last_check_in` TIMESTAMP NULL DEFAULT NULL,
  /*
    Router specific custom attributes.
    Managed by Router.
   */
  `attributes` JSON,
  /*
     The ID of the cluster this router instance is routing to.
     (implicit foreign key to avoid trouble when deleting a cluster).
      For backwards compatibility, if NULL, assumes there's a single cluster
      in the metadata.
   */
  `cluster_id` CHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci
    DEFAULT NULL,
  /*
    Router instance specific configuration options.
    Managed by Shell.
   */
  `options` JSON DEFAULT NULL,
  /*
    Id of the ClusterSet this cluster belongs to, if any.
   */
  `clusterset_id` VARCHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,

  UNIQUE KEY (address, router_name)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;

/*
  This table contains a list of all REST user accounts that are granted
  access to the routers REST interface. These are managed by the shell
  per cluster.
*/
CREATE TABLE IF NOT EXISTS router_rest_accounts (
  /*
     The ID of the cluster this router account applies to.
     (implicit foreign key to avoid trouble when deleting a cluster).
   */
  `cluster_id` CHAR(36) CHARACTER SET ascii COLLATE ascii_general_ci
    NOT NULL,
  /*
    The name of the user account.
  */
  `user` VARCHAR(256) NOT NULL,
  /*
    The authentication method used.
  */
  `authentication_method` VARCHAR(64) NOT NULL DEFAULT 'modular_crypt_format',
  /*
    The authentication string of the user account. The password is stored
    hashed, salted, multiple-rounds.
  */
  `authentication_string` TEXT CHARACTER SET ascii COLLATE ascii_general_ci
    DEFAULT NULL,
  /*
    A short description of the user account.
  */
  `description` VARCHAR(255) DEFAULT NULL,
  /*
    Stores the users privileges in a JSON document. Example:
    {readPriv: "Y", updatePriv: "N"}
  */
  `privileges` JSON,
  /*
    Additional attributes for the user account
  */
  `attributes` JSON DEFAULT NULL,

  PRIMARY KEY (cluster_id, user)
) CHARSET = utf8mb4, ROW_FORMAT = DYNAMIC;

--  Public Interface Views
--  ----------------------

/*
   These views will remain backwards compatible even if the internal schema
   change. No existing columns will have their types changed or removed,
   although new columns may be added in the future.
 */

DROP VIEW IF EXISTS v2_clusters;
CREATE SQL SECURITY INVOKER VIEW v2_clusters AS
    SELECT
          c.cluster_type,
          c.primary_mode,
          c.cluster_id,
          c.cluster_name,
          c.router_options
    FROM clusters c;

DROP VIEW IF EXISTS v2_gr_clusters;
CREATE SQL SECURITY INVOKER VIEW v2_gr_clusters AS
    SELECT
          c.cluster_type,
          c.primary_mode,
          c.cluster_id as cluster_id,
          c.cluster_name as cluster_name,
          c.attributes->>'$.group_replication_group_name' as group_name,
          c.attributes,
          c.options,
          c.router_options,
          c.description as description,
          c.clusterset_id
    FROM clusters c
    WHERE c.cluster_type = 'gr';

DROP VIEW IF EXISTS v2_instances;
CREATE SQL SECURITY INVOKER VIEW v2_instances AS
  SELECT i.instance_id,
         i.cluster_id,
         i.instance_name as label,
         i.mysql_server_uuid,
         i.address,
         i.addresses->>'$.mysqlClassic' as endpoint,
         i.addresses->>'$.mysqlX' as xendpoint,
         i.attributes
  FROM instances i;

DROP VIEW IF EXISTS v2_ar_clusters;
CREATE SQL SECURITY INVOKER VIEW v2_ar_clusters AS
    SELECT
          acv.view_id,
          c.cluster_type,
          c.primary_mode,
          acv.topology_type as async_topology_type,
          c.cluster_id,
          c.cluster_name,
          c.attributes,
          c.options,
          c.router_options,
          c.description,
          c.clusterset_id
    FROM clusters c
    JOIN async_cluster_views acv
      ON c.cluster_id = acv.cluster_id
    WHERE acv.view_id = (SELECT max(view_id)
        FROM async_cluster_views
        WHERE c.cluster_id = cluster_id)
      AND c.cluster_type = 'ar';

DROP VIEW IF EXISTS v2_ar_members;
CREATE SQL SECURITY INVOKER VIEW v2_ar_members AS
    SELECT
          acm.view_id,
          i.cluster_id,
          i.instance_id,
          i.instance_name as label,
          i.mysql_server_uuid as member_id,
          IF(acm.primary_master, 'PRIMARY', 'SECONDARY') as member_role,
          acm.master_instance_id as master_instance_id,
          mi.mysql_server_uuid as master_member_id
    FROM instances i
    LEFT JOIN async_cluster_members acm
      ON acm.cluster_id = i.cluster_id AND acm.instance_id = i.instance_id
    LEFT JOIN instances mi
      ON mi.instance_id = acm.master_instance_id
    WHERE acm.view_id = (SELECT max(view_id)
      FROM async_cluster_views WHERE i.cluster_id = cluster_id);

DROP VIEW IF EXISTS v2_cs_clustersets;
CREATE SQL SECURITY INVOKER VIEW v2_cs_clustersets AS
    SELECT
          acv.view_id,
          acv.topology_type as async_topology_type,
          c.clusterset_id,
          c.domain_name,
          c.attributes,
          c.options,
          c.router_options
    FROM clustersets c
    JOIN clusterset_views acv
      ON c.clusterset_id = acv.clusterset_id
    WHERE acv.view_id = (SELECT max(view_id)
        FROM clusterset_views
        WHERE c.clusterset_id = clusterset_id);

DROP VIEW IF EXISTS v2_cs_members;
CREATE SQL SECURITY INVOKER VIEW v2_cs_members AS
    SELECT
          (SELECT max(clusterset_views.view_id)
            FROM clusterset_views
            WHERE c.clusterset_id = clusterset_views.clusterset_id) view_id,
          c.clusterset_id,
          c.cluster_id,
          c.cluster_name,
          IF(csm.primary_cluster, 'PRIMARY', IF(csm.primary_cluster IS NULL, NULL, 'REPLICA')) AS member_role,
          csm.master_cluster_id,
          (csm.view_id IS NULL) AS invalidated
    FROM clusters c
    LEFT JOIN clusterset_members csm
      ON csm.clusterset_id = c.clusterset_id
          AND csm.cluster_id = c.cluster_id
          AND csm.view_id = (SELECT max(csm2.view_id)
                          FROM clusterset_members csm2
                          WHERE csm2.clusterset_id = c.clusterset_id)
    WHERE c.clusterset_id IS NOT NULL;

/*
  Returns information about the InnoDB cluster or replicaset the server
  being queried is part of.
 */
DROP VIEW IF EXISTS v2_this_instance;
CREATE SQL SECURITY INVOKER VIEW v2_this_instance AS
  SELECT i.cluster_id,
          i.instance_id,
          c.cluster_name,
          c.cluster_type
  FROM v2_instances i
  JOIN clusters c ON i.cluster_id = c.cluster_id
  WHERE i.mysql_server_uuid = (SELECT convert(variable_value using utf8mb4) COLLATE utf8mb4_general_ci
      FROM performance_schema.global_variables
      WHERE variable_name = 'server_uuid');

/*
  List of registered router instances. New routers will do inserts into this
  VIEW during bootstrap. They will also update the version, last_check_in and
  attributes field during runtime.
 */
DROP VIEW IF EXISTS v2_routers;
CREATE SQL SECURITY INVOKER VIEW v2_routers AS
  SELECT r.router_id,
         r.cluster_id,
         r.router_name,
         r.product_name,
         r.address,
         r.version,
         r.last_check_in,
         r.attributes,
         r.options,
         r.clusterset_id
  FROM routers r;

/*
  List of REST user accounts that are granted access to the routers REST
  interface. These are managed by the shell per cluster and consumed by the
  router.
*/
DROP VIEW IF EXISTS v2_router_rest_accounts;
CREATE SQL SECURITY INVOKER VIEW v2_router_rest_accounts AS
  SELECT a.cluster_id,
          a.user,
          a.authentication_method,
          a.authentication_string,
          a.description,
          a.privileges,
          a.attributes
  FROM router_rest_accounts a;


--  Public Interface Routines
--  -------------------------
DELIMITER //

DROP PROCEDURE IF EXISTS _v2_begin_cs_change//
CREATE PROCEDURE _v2_begin_cs_change(
  cs_id VARCHAR(36),
  operation VARCHAR(32),
  is_failover INT,
  OUT csvid BIGINT UNSIGNED)
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  DECLARE last_csvid BIGINT UNSIGNED;

  SELECT MAX(view_id) INTO last_csvid
    FROM mysql_innodb_cluster_metadata.clusterset_views
    WHERE clusterset_id = cs_id;

  # The view_id is (internally) composed by 2 sub-counters:
  # - left counter (leftmost 32 bits): incremented by failover operations.
  # - right counter (rightmost 32 bits): incremented by all other operations.
  # IMPORTANT NOTE: This additional failover sub-counter is required to ensure
  # the view_id is incremented even if the previous view information was lost
  # (not replicated to any slave) due to the primary failure, avoiding the
  # same view_id value to be reused (by incrementing an outdated value).

  SET csvid = last_csvid;
  IF is_failover THEN
    SET csvid = csvid + 4294967296; # + 0x100000000
  ELSE
    # Increment rightmost counter for other operations.
    SET csvid = csvid + 1;
  END IF;

  # create a new view of the clusterset
  INSERT INTO mysql_innodb_cluster_metadata.clusterset_views
    (clusterset_id, view_id, topology_type, view_change_reason,
      view_change_time, view_change_info,
       attributes
    ) SELECT
       clusterset_id, csvid, topology_type, operation,
       NOW(6), JSON_OBJECT('user', USER(), 'source', @@server_uuid ),
       attributes
       FROM mysql_innodb_cluster_metadata.clusterset_views
       WHERE clusterset_id = cs_id AND view_id = last_csvid;

  # copy the current member list
  INSERT INTO mysql_innodb_cluster_metadata.clusterset_members
    (clusterset_id, view_id, cluster_id, master_cluster_id,
      primary_cluster, attributes
    ) SELECT clusterset_id, csvid, cluster_id, master_cluster_id,
          primary_cluster, attributes
      FROM mysql_innodb_cluster_metadata.clusterset_members
      WHERE clusterset_id = cs_id AND view_id = last_csvid;
END //


DROP PROCEDURE IF EXISTS v2_cs_created//
CREATE PROCEDURE v2_cs_created(
  domain_name VARCHAR(63),
  cluster_id VARCHAR(36),
  attributes JSON,
  OUT cs_id VARCHAR(36))
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  DECLARE csvid BIGINT UNSIGNED;
  SELECT uuid() INTO cs_id;

  SET csvid = 1;

  INSERT INTO mysql_innodb_cluster_metadata.clustersets
    (clusterset_id, domain_name, options, attributes, router_options)
  VALUES (cs_id, domain_name, '{}', '{}', '{}');

  INSERT INTO mysql_innodb_cluster_metadata.clusterset_views (
    clusterset_id, view_id, topology_type,
    view_change_reason, view_change_time, view_change_info,
    attributes
  ) VALUES (cs_id, csvid, 'SINGLE-PRIMARY-TREE', 'CREATE', NOW(6),
    JSON_OBJECT('user', USER(),
      'source', @@server_uuid ),
    attributes
  );

  INSERT INTO mysql_innodb_cluster_metadata.clusterset_members
    (clusterset_id, view_id, cluster_id, master_cluster_id, primary_cluster,
      attributes
    ) VALUES (cs_id, csvid, cluster_id, NULL, 1,
      attributes
    );

  # update clusterset_id in the seed cluster record
  UPDATE mysql_innodb_cluster_metadata.clusters c
    SET c.clusterset_id = cs_id
    WHERE c.cluster_id = cluster_id;
END//


DROP PROCEDURE IF EXISTS v2_cs_member_added//
CREATE PROCEDURE v2_cs_member_added(
  cs_id VARCHAR(36),
  cluster_id VARCHAR(36),
  master_cluster_id VARCHAR(36),
  attributes JSON)
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  DECLARE csvid BIGINT UNSIGNED;

  CALL _v2_begin_cs_change(cs_id, 'ADD_CLUSTER', 0, csvid);

  # add the new member
  INSERT INTO mysql_innodb_cluster_metadata.clusterset_members
    (clusterset_id, view_id, cluster_id, master_cluster_id, primary_cluster,
      attributes
    ) VALUES (cs_id, csvid, cluster_id, IF(master_cluster_id='',NULL,master_cluster_id), 0,
      attributes
    );

  # update clusterset_id in the cluster record
  UPDATE mysql_innodb_cluster_metadata.clusters c
    SET c.clusterset_id = cs_id
    WHERE c.cluster_id = cluster_id;
END//



DROP PROCEDURE IF EXISTS v2_cs_member_rejoined//
CREATE PROCEDURE v2_cs_member_rejoined(
  cs_id VARCHAR(36),
  cluster_id VARCHAR(36),
  master_cluster_id VARCHAR(36),
  attributes JSON)
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  DECLARE csvid BIGINT UNSIGNED;

  CALL _v2_begin_cs_change(cs_id, 'REJOIN_CLUSTER', 0, csvid);

  # add the rejoining member
  INSERT INTO mysql_innodb_cluster_metadata.clusterset_members
    (clusterset_id, view_id, cluster_id, master_cluster_id, primary_cluster,
      attributes
    ) VALUES (cs_id, csvid, cluster_id, IF(master_cluster_id='',NULL,master_cluster_id), 0,
      attributes
    );
END//



DROP PROCEDURE IF EXISTS v2_cs_member_removed//
CREATE PROCEDURE v2_cs_member_removed(
  cs_id VARCHAR(36),
  cluster_id VARCHAR(36))
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  DECLARE csvid BIGINT UNSIGNED;

  CALL _v2_begin_cs_change(cs_id, 'REMOVE_CLUSTER', 0, csvid);

  # delete the removed cluster from the copied list in the transaction
  DELETE FROM mysql_innodb_cluster_metadata.clusterset_members
    WHERE mysql_innodb_cluster_metadata.clusterset_members.clusterset_id = cs_id
      AND mysql_innodb_cluster_metadata.clusterset_members.cluster_id = cluster_id
      AND mysql_innodb_cluster_metadata.clusterset_members.view_id = csvid;

  # remove the cluster_id column from the clusters table
  UPDATE mysql_innodb_cluster_metadata.clusters c
    SET c.clusterset_id = NULL
    WHERE c.cluster_id = cluster_id;

END//



DROP PROCEDURE IF EXISTS v2_cs_primary_changed//
CREATE PROCEDURE v2_cs_primary_changed(
  cs_id VARCHAR(36),
  primary_cluster_id VARCHAR(36),
  primary_attributes JSON)
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  DECLARE csvid BIGINT UNSIGNED;

  CALL _v2_begin_cs_change(cs_id, 'SWITCH_PRIMARY', 0, csvid);

  # update record for the promoted instance
  UPDATE mysql_innodb_cluster_metadata.clusterset_members
    SET master_cluster_id = NULL,
      primary_cluster = 1,
      attributes = JSON_MERGE_PATCH(primary_attributes, attributes)
    WHERE clusterset_id = cs_id AND view_id = csvid AND cluster_id = primary_cluster_id;

  # update records for the replica instances
  UPDATE mysql_innodb_cluster_metadata.clusterset_members
    SET master_cluster_id = primary_cluster_id,
      primary_cluster = 0,
      attributes = JSON_MERGE_PATCH(primary_attributes, attributes)
    WHERE clusterset_id = cs_id AND view_id = csvid AND cluster_id <> primary_cluster_id;
END //



DROP PROCEDURE IF EXISTS v2_cs_primary_force_changed//
CREATE PROCEDURE v2_cs_primary_force_changed(
  cs_id VARCHAR(36),
  primary_cluster_id VARCHAR(36),
  primary_attributes JSON)
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  DECLARE csvid BIGINT UNSIGNED;

  CALL _v2_begin_cs_change(cs_id, 'FORCE_PRIMARY', 1, csvid);

  # update record for the promoted instance
  UPDATE mysql_innodb_cluster_metadata.clusterset_members
    SET master_cluster_id = NULL,
      primary_cluster = 1,
      attributes = JSON_MERGE_PATCH(primary_attributes, attributes)
    WHERE clusterset_id = cs_id AND view_id = csvid AND cluster_id = primary_cluster_id;

  # update records for the replica instances
  UPDATE mysql_innodb_cluster_metadata.clusterset_members
    SET master_cluster_id = primary_cluster_id,
      primary_cluster = 0,
      attributes = JSON_MERGE_PATCH(primary_attributes, attributes)
    WHERE clusterset_id = cs_id AND view_id = csvid AND cluster_id <> primary_cluster_id;
END //



DROP PROCEDURE IF EXISTS v2_cs_add_invalidated_member//
CREATE PROCEDURE v2_cs_add_invalidated_member(
  cs_id VARCHAR(36),
  cluster_id VARCHAR(36))
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  # this must be called as part of another cs change record (e.g. primary switch)

  # delete view record for the invalidated cluster
  DELETE FROM mysql_innodb_cluster_metadata.clusterset_members
  WHERE mysql_innodb_cluster_metadata.clusterset_members.clusterset_id = cs_id
    AND mysql_innodb_cluster_metadata.clusterset_members.cluster_id = cluster_id
    AND mysql_innodb_cluster_metadata.clusterset_members.view_id = (SELECT MAX(view_id)
        FROM mysql_innodb_cluster_metadata.clusterset_views
        WHERE clusterset_id = cs_id);
END //


DROP PROCEDURE IF EXISTS v2_set_global_router_option//
CREATE PROCEDURE v2_set_global_router_option(
  id VARCHAR(36),
  option_name VARCHAR(100),
  option_value JSON)
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  if option_value IS NULL then
      UPDATE mysql_innodb_cluster_metadata.clustersets
        SET router_options = JSON_REMOVE(router_options, concat('$.', option_name))
        WHERE clusterset_id = id;
  else
      UPDATE mysql_innodb_cluster_metadata.clustersets
        SET router_options = JSON_SET(IFNULL(router_options, '{}'), concat('$.', option_name), option_value)
        WHERE clusterset_id = id;
  end if;
END //


DROP PROCEDURE IF EXISTS v2_set_routing_option//
CREATE PROCEDURE v2_set_routing_option(
  router VARCHAR(512),
  clusterset_id VARCHAR(36),
  option_name VARCHAR(100),
  option_value JSON)
SQL SECURITY INVOKER
MODIFIES SQL DATA
BEGIN
  DECLARE router_address VARCHAR(512);
  DECLARE router_name VARCHAR(512);
  DECLARE errmsg VARCHAR(600);
  SET router_address = SUBSTRING_INDEX(router, '::', 1);
  SET router_name = '';
  if router_address <> router then
    SET router_name = SUBSTRING_INDEX(router, '::', -1);
  end if;

  if 0 = (select count(*) from mysql_innodb_cluster_metadata.routers r
          where r.clusterset_id = clusterset_id and
          r.address = router_address and r.router_name = router_name) then
    SET errmsg = concat('Router ', QUOTE(router), ' is not part of this ClusterSet');
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = errmsg;
  end if;

  if option_value IS NULL then
      UPDATE mysql_innodb_cluster_metadata.routers r
        SET r.options = JSON_REMOVE(r.options, concat('$.', option_name))
        WHERE r.clusterset_id = clusterset_id AND r.address = router_address AND r.router_name = router_name;
  else
      UPDATE mysql_innodb_cluster_metadata.routers r
        SET r.options = JSON_SET(IFNULL(r.options, '{}'), concat('$.', option_name), option_value)
        WHERE r.clusterset_id = clusterset_id AND r.address = router_address AND r.router_name = router_name;
  end if;
END //

DELIMITER ;


/*
  List of router options per router in a ClusterSet.
  Merges global defaults if router specific values are not set.
 */
DROP VIEW IF EXISTS v2_cs_router_options;
CREATE SQL SECURITY INVOKER VIEW v2_cs_router_options AS
  SELECT
        r.router_id,
        CONCAT(r.address, '::', r.router_name) as router_label,
        r.clusterset_id,
        JSON_MERGE_PATCH((SELECT COALESCE(router_options, '{}')
                          FROM mysql_innodb_cluster_metadata.clustersets
                          WHERE clusterset_id = r.clusterset_id),
                        COALESCE(r.options, '{}')) as router_options
  FROM mysql_innodb_cluster_metadata.routers r;
