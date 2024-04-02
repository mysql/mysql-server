/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTITIES_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTITIES_H_

#include "mrs/observability/entities_manager.h"

enum EntitiesCounters {
  kEntityCounterHttpRequestGet = 1,
  kEntityCounterHttpRequestPost,
  kEntityCounterHttpRequestPut,
  kEntityCounterHttpRequestDelete,
  kEntityCounterHttpRequestOptions,
  kEntityCounterHttpConnectionsReused,
  kEntityCounterHttpConnectionsCreated,
  kEntityCounterHttpConnectionsClosed,
  kEntityCounterMySQLConnectionsReused,
  kEntityCounterMySQLConnectionsCreated,
  kEntityCounterMySQLConnectionsClosed,
  kEntityCounterMySQLConnectionsActive,
  kEntityCounterMySQLQueries,
  kEntityCounterMySQLChangeUser,
  kEntityCounterMySQLPrepare,
  kEntityCounterMySQLPrepareExecute,
  kEntityCounterMySQLPrepareRemove,
  kEntityCounterRestReturnedItems,
  kEntityCounterRestAffectedItems,
  kEntityCounterUpdatesObjects,
  kEntityCounterUpdatesFiles,
  kEntityCounterUpdatesAuthentications,
  kEntityCounterLast
};

enum EntriesAvarages {
  kEntityAverageHttpRequestGetDocumentAverageSize = 1,
  kEntityAverageHttpRequestPostDocumentAverageSize,
  kEntityAverageHttpRequestPutDocumentAverageSize,
  kEntityAverageLast
};

namespace mrs {

void initialize_entities(observability::EntitiesManager *manager);

}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTITIES_H_
