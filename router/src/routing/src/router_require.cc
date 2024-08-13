/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "router_require.h"

#include <cstring>  // strlen

#define RAPIDJSON_HAS_STDSTRING 1  // enable std::string support

#include "my_rapidjson_size_t.h"  // before rapidjson.h

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include "classic_connection_base.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

stdx::expected<void, classic_protocol::message::server::Error>
RouterRequire::enforce(Channel &client_channel, Attributes attrs) {
  // as stdx::expected(unexpect, ...) is 'explicit', return {} can't be used.
  using ret_type =
      stdx::expected<void, classic_protocol::message::server::Error>;

  bool subject_is_required = attrs.subject.has_value();

  bool issuer_is_required = attrs.issuer.has_value();

  bool x509_is_required =
      (attrs.x509 && *attrs.x509) || issuer_is_required || subject_is_required;

  bool ssl_is_required = (attrs.ssl && *attrs.ssl) || x509_is_required;

  if (ssl_is_required && (client_channel.ssl() == nullptr)) {
    return ret_type{stdx::unexpect, 1045, "Access denied (required: ssl)",
                    "28000"};
  }

  if (x509_is_required) {
    auto *client_ssl = client_channel.ssl();

    if (X509_V_OK != SSL_get_verify_result(client_ssl)) {
      return ret_type{stdx::unexpect, 1045,
                      "Access denied (required: x509 invalid)", "28000"};
    }

    struct X509Deleter {
      void operator()(X509 *x509) { X509_free(x509); }
    };

    using X509Cert = std::unique_ptr<X509, X509Deleter>;

    if (auto client_x509 = X509Cert(SSL_get_peer_certificate(client_ssl))) {
      if (subject_is_required) {
        std::string subject_name;
        subject_name.resize(512);  // reserve a bit of memory.

        X509_NAME_oneline(X509_get_subject_name(client_x509.get()),
                          subject_name.data(), subject_name.size());

        // resize to what is actually used.
        subject_name.resize(strlen(subject_name.c_str()));

        if (subject_name != *attrs.subject) {
          return ret_type{stdx::unexpect, 1045,
                          "Access denied (required: x509-subject mismatch)",
                          "28000"};
        }
      }

      if (issuer_is_required) {
        std::string issuer_name;
        issuer_name.resize(512);  // reserve a bit of memory.

        X509_NAME_oneline(X509_get_issuer_name(client_x509.get()),
                          issuer_name.data(), issuer_name.size());

        // resize to what is actually used.
        issuer_name.resize(strlen(issuer_name.c_str()));

        if (issuer_name != *attrs.issuer) {
          return ret_type{stdx::unexpect, 1045,
                          "Access denied (required: x509-issuer mismatch)",
                          "28000"};
        }
      }
    } else {
      return ret_type{stdx::unexpect, 1045, "Access denied (required: x509)",
                      "28000"};
    }
  }

  return {};
}

namespace {
/**
 * capture the user-attributes.
 *
 * Expects a resultset similar to that of:
 *
 * @code
 * SELECT attribute
 *   FROM information_schema.user_attributes
 *  WHERE CONCAT(user, '@', host) = CURRENT_USER()
 * @endcode
 *
 * - 1 columns (column-names are ignored)
 * - 1 row
 */
class SelectUserAttributesHandler : public QuerySender::Handler {
 public:
  explicit SelectUserAttributesHandler(RouterRequireFetcher::Result &result)
      : result_(result) {}

  void on_column_count(uint64_t count) override {
    if (count != 1) {
      failed({kTooManyColumns, "Invalid Resultset", "HY000"});
    }
  }

  static std::optional<RouterRequire::Attributes> parse_router_require(
      std::string_view json_doc) {
    rapidjson::Document doc;

    doc.Parse(json_doc.data(), json_doc.size());
    if (doc.HasParseError()) return std::nullopt;
    if (!doc.IsObject()) return std::nullopt;

    auto router_require_it = doc.FindMember("router_require");
    // no requirements
    if (router_require_it == doc.MemberEnd()) return std::nullopt;

    // if router_require exists, it MUST be an object. Otherwise fail auth.

    auto &router_require = router_require_it->value;
    if (!router_require.IsObject()) return std::nullopt;

    RouterRequire::Attributes required{};
    for (auto it = router_require.MemberBegin();
         it != router_require.MemberEnd(); ++it) {
      // name should be a string.
      if (!it->name.IsString()) return std::nullopt;

      std::string_view name(it->name.GetString(), it->name.GetStringLength());
      auto &val = it->value;

      if (name == "ssl") {
        // if "ssl" exists in "router_require", it MUST be a bool.
        if (!val.IsBool()) return std::nullopt;

        required.ssl = val.GetBool();
      } else if (name == "x509") {
        // if "x509" exists in "router_require", it MUST be a bool.
        if (!val.IsBool()) return std::nullopt;

        required.x509 = val.GetBool();
      } else if (name == "subject") {
        // if "subject" exists in "router_require", it MUST be a string.
        if (!val.IsString()) return std::nullopt;

        required.subject = val.GetString();
      } else if (name == "issuer") {
        // if "issuer" exists in "router_require", it MUST be a string.
        if (!val.IsString()) return std::nullopt;

        required.issuer = val.GetString();
      } else {
        // unknown, required option.
        return std::nullopt;
      }
    }

    return required;
  }

  void on_row(const classic_protocol::message::server::Row &row) override {
    if (failed().error_code() != 0) return;

    if (row_count_ != 0) {
      failed({kTooManyRows, "Too many rows", "HY000"});
    }

    ++row_count_;

    auto it = row.begin();  // row[0]

    if (!(*it).has_value()) return;  // NULL, no requirements.

    auto parse_res = parse_router_require(it->value());
    if (!parse_res) {
      result_ = stdx::unexpected(classic_protocol::message::server::Error{
          kAccessDenied, "Access denied", "28000"});
      return;
    }

    result_ = *parse_res;
  }

  void on_row_end(const classic_protocol::message::server::Eof &msg
                  [[maybe_unused]]) override {
    auto err = failed();

    if (err.error_code() == 0) return;

    // error, shouldn't happen. Log it.
    log_debug("fetching user-attrs failed: %s", err.message().c_str());

    result_ = stdx::unexpected(err);
  }

  void on_error(const classic_protocol::message::server::Error &err) override {
    if (err.error_code() == 1109) return;  // unknown table (before 8.0.21)

    // error, shouldn't happen. Log it.
    log_debug("fetching user-attrs failed: %s", err.message().c_str());

    result_ = stdx::unexpected(err);
  }

  void failed(classic_protocol::message::server::Error msg) {
    err_ = std::move(msg);
  }

  const classic_protocol::message::server::Error &failed() { return err_; }

 private:
  uint64_t row_count_{};

  static constexpr const uint16_t kTooManyRows{1234};
  static constexpr const uint16_t kTooManyColumns{1234};
  static constexpr const uint16_t kAccessDenied{1045};

  classic_protocol::message::server::Error err_{};

  std::string user_attrs_;

  RouterRequireFetcher::Result &result_;
};
}  // namespace

void RouterRequireFetcher::push_processor(
    MysqlRoutingClassicConnectionBase *connection, Result &fetcher_result) {
  connection->push_processor(std::make_unique<QuerySender>(
      connection,
      "SELECT attribute FROM information_schema.user_attributes WHERE "
      "CONCAT(user, '@', host) = CURRENT_USER()",
      std::make_unique<SelectUserAttributesHandler>(fetcher_result)));
}
