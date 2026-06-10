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

#include "mrs/endpoint/handler/handler_db_object_script.h"

#include <stdexcept>
#include <string>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/container/generic.h"

#include "helper/http/url.h"
#include "helper/json/jvalue.h"
#include "helper/json/rapid_json_iterator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/media_detector.h"
#include "helper/mysql_numeric_value.h"
#include "mrs/database/helper/bind.h"
#include "mrs/database/query_rest_sp.h"
#include "mrs/database/query_rest_sp_media.h"
#include "mrs/endpoint/content_set_endpoint.h"
#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/routine_utilities.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"
#include "mrs/rest/response_cache.h"
#include "mrs/router_observation_entities.h"
#include "mysql/harness/scoped_callback.h"
#include "router/src/mysql_rest_service/include/collector/mysql_cache_manager.h"

#ifdef HAVE_JIT_EXECUTOR_PLUGIN
#include "database/session.h"
#include "mysqlrouter/jit_executor_common.h"
#include "mysqlrouter/jit_executor_component.h"
#include "mysqlrouter/jit_executor_value.h"
#include "utils/utils_string.h"
#endif

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

namespace {
#ifdef HAVE_JIT_EXECUTOR_PLUGIN
constexpr const int k_default_request_timeout{1000};
#endif
}  // namespace

using namespace std::string_literals;
using namespace helper::json::sql;
using HttpResult = mrs::rest::Handler::HttpResult;
using CachedObject = collector::MysqlCacheManager::CachedObject;
using Url = helper::http::Url;
using Authorization = mrs::rest::Handler::Authorization;
using Request = mrs::rest::RequestContext::Request;
using ResultSets = mrs::database::entry::ResultSets;
using Fields = std::vector<mrs::database::entry::Field>;

class HandlerDbObjectScript::Impl {
 public:
  Impl(HandlerDbObjectScript *owner) : m_owner{owner} { update(); }

  void update() {
    m_service_ep.reset();
    m_file_to_load.clear();

    assert(m_owner->entry_);

    // Let's find the associated service, will be needed to:
    // - Retrieve the JitExecutor associated to the service
    // - Identify the content set, to know the files to be loaded
    auto endpoint = m_owner->endpoint_.lock();
    if (endpoint) {
      auto parent = endpoint->get_parent_ptr();
      std::shared_ptr<DbServiceEndpoint> service_ep;
      while (parent && !service_ep) {
        service_ep = std::dynamic_pointer_cast<DbServiceEndpoint>(parent);
        if (!service_ep) {
          parent = parent->get_parent_ptr();
        } else {
          m_service_ep = std::weak_ptr<DbServiceEndpoint>(service_ep);
        }
      }
    }

    // A script is expected to have an entry on the content_set_has_obj_def
    // table with the information about the object/method to execute and the
    // content set that contains the file to be loaded for such purpose.
    if (!m_owner->entry_->content_set_def.has_value()) {
      throw http::Error(HttpStatusCode::InternalError,
                        "Missing content set object definition for :"s +
                            m_owner->entry_->request_path);
    }

    rapidjson::Document doc;
    doc.Parse((*m_owner->entry_->content_set_def->options).data(),
              (*m_owner->entry_->content_set_def->options).size());
    if (!doc.IsObject()) {
      throw http::Error(
          HttpStatusCode::InternalError,
          "Invalid options for Db Object Content Set Definition  at " +
              m_owner->entry_->request_path);
    }

    if (doc.HasMember("file_to_load") && doc["file_to_load"].IsString()) {
      m_file_to_load = doc["file_to_load"].GetString();
    }
  }

  virtual ~Impl() = default;

#ifdef HAVE_JIT_EXECUTOR_PLUGIN
  shcore::Argument_list get_parameters(std::string_view body,
                                       const Fields &fields) {
    std::vector<shcore::Value> parameters;
    if (!body.empty()) {
      auto params = shcore::Value::parse(body);

      if (params.get_type() != shcore::Map) {
        throw http::Error(HttpStatusCode::BadRequest,
                          "Invalid parameters format.");
      }

      auto params_map = params.as_map();

      // Validation for invalid parameters
      std::vector<std::string> invalid_params;
      for (const auto &el : (*params_map)) {
        auto key = el.first;
        const database::entry::Field *param;
        if (!mysql_harness::utility::container::get_ptr_if(
                fields, [key](auto &v) { return v.name == key; }, &param)) {
          invalid_params.push_back(key);
        }
      }

      if (!invalid_params.empty()) {
        std::vector<std::string> allowed_params;
        for (const auto &it : fields) {
          allowed_params.push_back(it.name);
        }

        std::string allowed_str{"The function accepts no parameters."};
        if (!allowed_params.empty()) {
          allowed_str =
              "Allowed: " + shcore::str_join(allowed_params, ", ") + ".";
        }
        auto invalid_str = shcore::str_join(invalid_params, ", ");

        throw http::Error(
            HttpStatusCode::BadRequest,
            "Not allowed parameter:"s + invalid_str + ". " + allowed_str);
      }

      for (auto &el : fields) {
        // TODO(rennox): should we handle modeOut or modeInOut parameters??
        if (el.mode == mrs::database::entry::Field::Mode::modeIn) {
          if (params_map->has_key(el.name)) {
            parameters.emplace_back(params_map->at(el.name));
          } else {
            parameters.emplace_back();
          }
        }
      }
    }

    return parameters;
  }

  // A specific timeout can be overriden by as a db_object option
  int get_timeout(std::string_view options) {
    int result = k_default_request_timeout;

    if (!options.empty()) {
      rapidjson::Document doc;
      doc.Parse(options.data(), options.size());
      if (doc.IsObject()) {
        if (doc.HasMember("timeout") && doc["timeout"].IsInt()) {
          result = doc["timeout"].GetInt();

          if (result <= 0) {
            result = k_default_request_timeout;
          }
        }
      }
    }

    return result;
  }
#endif

  const std::string &entry_script() const { return m_file_to_load; }
  std::shared_ptr<DbServiceEndpoint> service_endpoint() {
    return m_service_ep.lock();
  }

 private:
  HandlerDbObjectScript *m_owner;
  std::weak_ptr<DbServiceEndpoint> m_service_ep;
  std::string m_file_to_load;
};

HttpResult HandlerDbObjectScript::handle_script(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  auto &input_buffer = ctxt->request->get_input_buffer();
  auto size = input_buffer.length();
  auto document = input_buffer.pop_front(size);
  return handle_script(ctxt, document);
}

HttpResult HandlerDbObjectScript::handle_script(
    [[maybe_unused]] rest::RequestContext *ctxt,
    const std::vector<uint8_t> &document [[maybe_unused]]) {
#ifdef HAVE_JIT_EXECUTOR_PLUGIN
  if (m_impl->entry_script().empty()) {
    throw http::Error(HttpStatusCode::InternalError,
                      "Missing file to load for  " + entry_->request_path);
  }

  auto cached_response = [](const std::shared_ptr<mrs::CacheEntry> &entry) {
    if (entry->media_type.has_value()) {
      return HttpResult(entry->data, *entry->media_type);
    } else {
      return HttpResult(std::string(entry->data));
    }
  };

  std::string_view body{reinterpret_cast<const char *>(document.data()),
                        document.size()};

  if (response_cache_) {
    auto entry =
        response_cache_->lookup_routine(get_endpoint_url(endpoint_), body);
    if (entry) {
      return cached_response(entry);
    }
  }

  // Process the parameters...
  auto parameters =
      m_impl->get_parameters(body, entry_->fields.parameters.fields);

  auto service_ep = m_impl->service_endpoint();
  if (!service_ep) {
    throw http::Error(HttpStatusCode::NotFound,
                      "No longer available :"s + entry_->request_path);
  }

  auto result_type = jit_executor::ResultType::Json;
  if (entry_->format == DbObject::Format::formatMedia &&
      (entry_->media_type.has_value() && *entry_->media_type == "text/html")) {
    result_type = jit_executor::ResultType::Raw;
  }

  while (true) {
    try {
      int timeout = m_impl->get_timeout(entry_->options.value_or(""));
      auto context = service_ep->get_scripting_context();

      if (!context) {
        throw std::runtime_error(
            "Unable to satisfy the request, no scripting contexts available.");
      }

      HandlerDbObjectTable::CachedSession session;
      std::shared_ptr<shcore::polyglot::database::Session> be_session;

      mysql_harness::ScopedCallback reset_session([&be_session]() {
        // This reset ensures the session is left in a usable state
        if (be_session) {
          be_session->reset();
        }
      });

      auto result = context->get()->execute(
          m_impl->entry_script(), entry_->content_set_def->class_name,
          entry_->content_set_def->name, parameters, timeout, result_type,
          {// Get Session Callback
           [&, this](
               bool read_only) -> std::shared_ptr<jit_executor::db::ISession> {
             collector::MySQLConnection session_type =
                 collector::MySQLConnection::kMySQLConnectionUserdataRO;
             if (!read_only) {
               session_type =
                   collector::MySQLConnection::kMySQLConnectionUserdataRW;
             }

             session = get_session(ctxt, session_type);
             session->connection_id();

             be_session = std::make_shared<shcore::polyglot::database::Session>(
                 session.get()->get_handle());

             return be_session;
           },
           // Get current MRS User ID Callback
           [&, ctxt]() -> std::optional<std::string> {
             if (ctxt->session) {
               return ctxt->session->user.user_id.to_string();
             }

             return {};
           },
           // Get Content Set
           [&](const std::string &module_class_name) {
             return service_ep->get_content_set_path(module_class_name);
           },
           // Timeout Callback: to be executed in case the script timeout is
           // reached...
           [&]() {
             if (!session.empty()) {
               std::string q =
                   "KILL QUERY " + std::to_string(session->connection_id());

               auto params = session->get_connection_parameters();
               try {
                 auto killer_session = cache_->clone_instance(params);

                 killer_session->execute(q);
               } catch (const std::exception &e) {
                 log_warning("Error killing connection at %s: %s",
                             params.conn_opts.destination.str().c_str(),
                             e.what());
               }
             }
           }});

      if (response_cache_) {
        auto entry = response_cache_->create_routine_entry(
            get_endpoint_url(endpoint_), body, result,
            entry_->media_type.value_or(""));

        if (entry) {
          return cached_response(entry);
        }
      }

      // Builds the response when no Cache is used
      auto response = HttpResult(std::move(result));
      if (entry_->media_type.has_value()) {
        response.type_text = *entry_->media_type;
      }
      return response;
    } catch (const jit_executor::TimeoutError &) {
      throw http::Error(HttpStatusCode::RequestTimeout);
    } catch (const jit_executor::MemoryError &error) {
      log_debug("%s", error.what());
      // NO-OP: a retry will be done
    } catch (const std::exception &error) {
      throw http::Error(HttpStatusCode::InternalError, error.what());
    }
  }
#else
  throw http::Error(HttpStatusCode::NotImplemented);
#endif
}

HttpResult HandlerDbObjectScript::handle_delete(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HandlerDbObjectScript::HandlerDbObjectScript(
    std::weak_ptr<DbObjectEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager,
    mrs::GtidManager *gtid_manager, collector::MysqlCacheManager *cache,
    mrs::ResponseCache *response_cache)
    : HandlerDbObjectTable{endpoint, auth_manager, gtid_manager, cache,
                           response_cache} {
  { m_impl = std::make_shared<Impl>(this); }
}

HttpResult HandlerDbObjectScript::handle_put(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  return handle_script(ctxt);
}

HttpResult HandlerDbObjectScript::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  return handle_script(ctxt, document);
}

HttpResult HandlerDbObjectScript::handle_get(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  return handle_script(ctxt);
}

uint32_t HandlerDbObjectScript::get_access_rights() const {
  return HandlerDbObjectTable::get_access_rights() &
         (mrs::database::entry::Operation::valueCreate |
          mrs::database::entry::Operation::valueRead |
          mrs::database::entry::Operation::valueUpdate);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
