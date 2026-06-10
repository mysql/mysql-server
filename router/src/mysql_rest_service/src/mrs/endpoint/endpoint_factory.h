/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_ENDPOINT_FACTORY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_ENDPOINT_FACTORY_H_

#include "mrs/database/entry/content_file.h"
#include "mrs/database/entry/content_set.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/database/entry/db_schema.h"
#include "mrs/database/entry/db_service.h"
#include "mrs/rest/entry/app_url_host.h"

#include "mrs/interface/endpoint_base.h"
#include "mrs/interface/handler_factory.h"

namespace mrs {
namespace endpoint {

class EndpointFactory {
 public:
  using DbSchema = database::entry::DbSchema;
  using DbService = database::entry::DbService;
  using UrlHost = rest::entry::AppUrlHost;
  using ContentSet = database::entry::ContentSet;
  using ContentFile = database::entry::ContentFile;
  using DbObject = database::entry::DbObject;
  using EndpointBase = mrs::interface::EndpointBase;

  using HandlerFactory = mrs::interface::HandlerFactory;
  using HandlerFactoryPtr = std::shared_ptr<HandlerFactory>;
  using EndpointBasePtr = std::shared_ptr<EndpointBase>;
  using EndpointConfiguration = mrs::interface::EndpointConfiguration;
  using EndpointConfigurationPtr = std::shared_ptr<EndpointConfiguration>;

 public:
  EndpointFactory(HandlerFactoryPtr handler_factory,
                  EndpointConfigurationPtr configuration)
      : handler_factory_{handler_factory}, configuration_{configuration} {}
  virtual ~EndpointFactory() = default;

  virtual EndpointBasePtr create_object(const ContentSet &set,
                                        EndpointBasePtr parent);
  virtual EndpointBasePtr create_object(const ContentFile &file,
                                        EndpointBasePtr parent);
  virtual EndpointBasePtr create_object(const DbSchema &schema,
                                        EndpointBasePtr parent);
  virtual EndpointBasePtr create_object(const DbObject &object,
                                        EndpointBasePtr parent);
  virtual EndpointBasePtr create_object(const DbService &service,
                                        EndpointBasePtr parent);
  virtual EndpointBasePtr create_object(const UrlHost &host,
                                        EndpointBasePtr parent);

 private:
  HandlerFactoryPtr handler_factory_;
  EndpointConfigurationPtr configuration_;
};

}  // namespace endpoint
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_ENDPOINT_FACTORY_H_ \
        */
