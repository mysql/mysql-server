/*
  Copyright (c) 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "bootstrap_mrs_schema_creator.h"

#include "mrs_metadata_schema.h"
#include "mysql/harness/logging/logging.h"
#include "mysqld_error.h"
#include "mysqlrouter/cluster_aware_session.h"
#include "scope_guard.h"

#include <iostream>

IMPORT_LOG_FUNCTIONS()

BootstrapMrsSchemaCreator::BootstrapMrsSchemaCreator(
    mysqlrouter::MySQLSession *session, const mysqlrouter::URI &target_uri,
    const std::string &target_socket, const int connect_timeout)
    : session_(session),
      target_uri_(target_uri),
      target_socket_(target_socket),
      connect_timeout_(connect_timeout) {
  assert(session_);
}

void BootstrapMrsSchemaCreator::try_create() {
  acquire_schema_lock();

  Scope_guard _([&]() { release_schema_lock(); });

  const auto mrs_md_version = get_current_metadata_version();
  if (!mrs_md_version) {
    // the metadata schema doesn't exist, try to create it
    try {
      create_schema();
    } catch (const std::exception &e) {
      log_debug("Failed creating MRS metadata schema '%s'; cleaning up...",
                e.what());
      try {
        session_->execute(
            "DROP SCHEMA IF EXISTS `mysql_rest_service_metadata`");
      } catch (const std::exception &ee) {
        log_debug(
            "Failed trying to remove 'mysql_rest_service_metadata' schema: %s",
            ee.what());
      }
      throw;
    }
    return;
  }

  // The LOCK is free but the version is 0.0.0, some failed previous run of
  // create metadata? Bail out recommending using Shell to fix it.
  if (*mrs_md_version == mrs::database::MrsSchemaVersion{0, 0, 0}) {
    std::cout << "MRS metadata version is: "
              << mrs::database::MrsSchemaVersion{0, 0, 0}.str()
              << " but the MRS_METADATA_LOCK is available. The MRS metadata "
                 "schema appears to be invalid. Use MySQLShell to fix it.\n";
    throw std::runtime_error("Invalid MRS metadata");
  }

  std::string compat_versions;
  for (const auto &v : mrs::database::kCompatibleMrsMetadataVersions) {
    if (!compat_versions.empty()) compat_versions += ", ";
    compat_versions += v.str();
  }

  std::cout << "Requested to create MRS metadata schema, ";
  if (mrs_md_version->is_compatible(
          mrs::database::kCompatibleMrsMetadataVersions)) {
    std::cout << "schema with compatible version '" << mrs_md_version->str()
              << "' already exists.\n";
    std::cout << "The compatible versions are: " << compat_versions << "\n";
  } else {
    std::cout << "schema with incompatible version '" << mrs_md_version->str()
              << "' already exists.\n";
    std::cout << "The compatible versions are: " << compat_versions << "\n";

    throw std::runtime_error("Invalid MRS metadata");
  }
}

namespace {
std::optional<mysqlrouter::MetadataSchemaVersion> get_cluster_md_version(
    mysqlrouter::MySQLSession *session) {
  try {
    return mysqlrouter::get_metadata_schema_version(session);
  } catch (const mysqlrouter::metadata_missing &) {
    return std::nullopt;
  }
}
}  // namespace

void BootstrapMrsSchemaCreator::run() {
  // we are expected to fallback to primary instance in case of errors that may
  // result from Secondary node being used by user in URI

  // check if we deal with a cluster
  const auto cluster_md_version = get_cluster_md_version(session_);

  // if not just do a regular attempt
  if (!cluster_md_version) {
    return try_create();
  }

  // otherwise create a ClusterAwareDecorator to do the potential fallback
  auto md_backend =
      mysqlrouter::create_metadata(*cluster_md_version, session_, {});

  if (md_backend->fetch_current_instance_type() ==
      mysqlrouter::InstanceType::ReadReplica) {
    throw std::runtime_error(
        "Bootstrapping using the Read Replica Instance address is not "
        "supported");
  }

  ClusterAwareDecorator cluster_aware(
      *md_backend, target_uri_.username, target_uri_.password, target_uri_.host,
      target_uri_.port, target_socket_, connect_timeout_);

  cluster_aware.failover_on_failure<void>([&]() { return try_create(); });
}

void BootstrapMrsSchemaCreator::acquire_schema_lock() {
  log_debug("Acquiring MRS_METADATA_LOCK");

  try {
    auto result =
        session_->query_one("SELECT GET_LOCK('MRS_METADATA_LOCK', 1)");
    if (!result || result->size() != 1) {
      throw std::runtime_error("Unexpected query result");
    }
    if ((strcmp((*result)[0], "1") == 0)) {
      log_debug("Successfully acquired MRS_METADATA_LOCK");
      return;
    }
    throw std::runtime_error(
        "The lock is taken. Make sure there is no "
        "other proccess creating or upgrading the MRS metadata.");
  } catch (std::exception &e) {
    std::cout << "Error acquiring MRS_METADATA_LOCK: " << e.what() << ".\n";
    throw;
  }
}

void BootstrapMrsSchemaCreator::release_schema_lock() {
  try {
    session_->execute("SELECT RELEASE_LOCK('MRS_METADATA_LOCK')");
  } catch (std::exception &e) {
    log_error("Error releasing MRS_METADATA_LOCK: %s", e.what());
    std::cout << "Error releasing MRS_METADATA_LOCK: " << e.what() << ".\n";
  }
}

void BootstrapMrsSchemaCreator::create_schema() {
  std::cout << "Creating MRS metadata (version '"
            << mrs::database::kCurrentMrsMetadataVersion.str() << "')...\n";
  const char **query_ptr;
  for (query_ptr = &mrs_metadata_schema[0]; *query_ptr != nullptr;
       query_ptr++) {
    const std::string query{*query_ptr};
    session_->execute(query);
  }
  std::cout << "Successfully created MRS metadata.\n";
}

std::optional<mrs::database::MrsSchemaVersion>
BootstrapMrsSchemaCreator::get_current_metadata_version() {
  try {
    mrs::database::QueryVersion q;
    return q.query_version(session_);
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    if (e.code() == ER_BAD_DB_ERROR) {
      // fallback to return
    } else {
      std::cout << "MRS metadata query returned error: " << e.code() << " "
                << e.what()
                << ". The MRS metadata schema appears to be invalid. Use "
                   "MySQLShell to fix it.\n";
      throw std::runtime_error("Invalid MRS metadata");
    }
  }
  // no metadata schema
  return std::nullopt;
}