/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "services_required.h"
#include <mysql/service_plugin_registry.h>

SERVICE_TYPE(registry) *Registry_service::h_registry = nullptr;
my_service<SERVICE_TYPE(mysql_udf_metadata)> *Udf_metadata::h_service = nullptr;
my_service<SERVICE_TYPE(mysql_string_converter)>
    *Character_set_converter::h_service = nullptr;
my_service<SERVICE_TYPE(udf_registration)> *Udf_registration::h_service =
    nullptr;

const char *Error_capture::s_message = "";
std::string Error_capture::get_last_error() {
  std::string err = s_message;
  s_message = "";
  return err;
}

bool Registry_service::acquire() {
  if (Registry_service::h_registry == nullptr)
    Registry_service::h_registry = mysql_plugin_registry_acquire();
  if (Registry_service::h_registry == nullptr) {
    s_message = "Could not acquire the plugin registry service";
    return true;
  }
  return false;
}

void Registry_service::release() {
  if (Registry_service::h_registry)
    mysql_plugin_registry_release(Registry_service::h_registry);
  Registry_service::h_registry = nullptr;
}

SERVICE_TYPE(registry) * Registry_service::get() {
  return Registry_service::h_registry;
}

bool Udf_registration::acquire() {
  if (Udf_registration::h_service == nullptr) {
    try {
      Udf_registration::h_service =
          new my_service<SERVICE_TYPE(udf_registration)>(
              "udf_registration", Registry_service::get());
      if (!Udf_registration::h_service->is_valid()) throw std::exception();
    } catch (...) {
      s_message = "Could not acquire the udf_registration service.";
      return true;
    }
  }
  return false;
}

void Udf_registration::release() {
  delete Udf_registration::h_service;
  Udf_registration::h_service = nullptr;
}

bool Udf_registration::add(const char *func_name, enum Item_result return_type,
                           Udf_func_any func, Udf_func_init init_func,
                           Udf_func_deinit deinit_func) {
  return (*h_service)
      ->udf_register(func_name, return_type, func, init_func, deinit_func);
}

bool Udf_registration::remove(const char *name, int *was_present) {
  return (*h_service)->udf_unregister(name, was_present);
}

bool Character_set_converter::acquire() {
  if (Character_set_converter::h_service == nullptr) {
    try {
      Character_set_converter::h_service =
          new my_service<SERVICE_TYPE(mysql_string_converter)>(
              "mysql_string_converter", Registry_service::get());
      if (!Character_set_converter::h_service->is_valid())
        throw std::exception();
    } catch (...) {
      s_message = "Could not acquire the mysql_string_converter service.";
      return true;
    }
  }
  return false;
}

void Character_set_converter::release() {
  delete Character_set_converter::h_service;
  Character_set_converter::h_service = nullptr;
}

SERVICE_TYPE(mysql_string_converter) * Character_set_converter::get() {
  return *(Character_set_converter::h_service);
}

/**
  Converts the buffer from one character set to another.
  - It first acquires the opaque handle from the input buffer and input charset
  - It then acquires the output buffer in the expected character set.

  This method uses string component services to do the conversion.

  @param [in] out_charset_name  Character set name in which out buffer is
                                expected
  @param [in] in_charset_name  Character set name of the input buffer
  @param [in] in_buffer  Input buffer to be converted
  @param [in] out_buffer_length Max size that output buffer can return
  @param [out] out_buffer Output buffer which is converted in the charset
                          specified
  @retval false Buffer is converted into the charset
  @retval true  Otherwise
*/
bool Character_set_converter::convert(const std::string &out_charset_name,
                                      const std::string &in_charset_name,
                                      const std::string &in_buffer,
                                      size_t out_buffer_length,
                                      char *out_buffer) {
  if (!h_service->is_valid()) return true;
  my_h_string out_string = nullptr;
  const my_service<SERVICE_TYPE(mysql_string_factory)> h_string_factory(
      "mysql_string_factory", Registry_service::get());
  if (h_string_factory.is_valid() && h_string_factory->create(&out_string)) {
    s_message = "Create string failed.";
    return true;
  } else {
    static char msg_buf[256];
    h_string_factory->destroy(out_string);
    if ((*h_service)
            ->convert_from_buffer(&out_string, in_buffer.c_str(),
                                  in_buffer.length(),
                                  in_charset_name.c_str())) {
      h_string_factory->destroy(out_string);
      snprintf(msg_buf, sizeof(msg_buf) - 1,
               "Failed to retrieve the buffer in charset %s",
               in_charset_name.c_str());
      s_message = msg_buf;
      return true;
    }
    if ((*h_service)
            ->convert_to_buffer(out_string, out_buffer, out_buffer_length,
                                out_charset_name.c_str())) {
      h_string_factory->destroy(out_string);
      snprintf(msg_buf, sizeof(msg_buf) - 1,
               "Failed to convert the buffer in charset %s",
               out_charset_name.c_str());
      s_message = msg_buf;
      return true;
    }
  }
  h_string_factory->destroy(out_string);
  return false;
}

bool Udf_metadata::acquire() {
  if (Udf_metadata::h_service == nullptr) {
    try {
      Udf_metadata::h_service =
          new my_service<SERVICE_TYPE(mysql_udf_metadata)>(
              "mysql_udf_metadata", Registry_service::get());
      if (!Udf_metadata::h_service->is_valid()) throw std::exception();
    } catch (...) {
      s_message = "Could not acquire the UDF extension service";
      return true;
    }
  }
  return false;
}

void Udf_metadata::release() {
  delete Udf_metadata::h_service;
  Udf_metadata::h_service = nullptr;
}

SERVICE_TYPE(mysql_udf_metadata) * Udf_metadata::get() {
  return *(Udf_metadata::h_service);
}
