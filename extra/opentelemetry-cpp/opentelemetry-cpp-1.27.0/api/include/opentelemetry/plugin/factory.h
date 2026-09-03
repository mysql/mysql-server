// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/plugin/detail/utility.h"  // IWYU pragma: export
#include "opentelemetry/plugin/tracer.h"
#include "opentelemetry/version.h"

// Always print a warning

#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC warning "opentelemetry/plugin/ is deprecated, and will be removed. Do not use."
#elif defined(_MSC_VER)
#  pragma message( \
      "[WARNING]: opentelemetry/plugin/ is deprecated, and will be removed. Do not use.")
#endif

// And fail in deprecated-free builds

#ifdef OPENTELEMETRY_NO_DEPRECATED_CODE
#  error "header <opentelemetry/plugin/> is deprecated."
#endif

OPENTELEMETRY_BEGIN_NAMESPACE
namespace plugin
{
/**
 * Factory creates OpenTelemetry objects from configuration strings.
 */
class Factory final
{
public:
  class FactoryImpl
  {
  public:
    FactoryImpl()                                   = default;
    FactoryImpl(const FactoryImpl &)                = default;
    FactoryImpl(FactoryImpl &&) noexcept            = default;
    FactoryImpl &operator=(const FactoryImpl &)     = default;
    FactoryImpl &operator=(FactoryImpl &&) noexcept = default;
    virtual ~FactoryImpl()                          = default;

    virtual nostd::unique_ptr<TracerHandle> MakeTracerHandle(
        nostd::string_view tracer_config,
        nostd::unique_ptr<char[]> &error_message) const noexcept = 0;
  };

  Factory(std::shared_ptr<DynamicLibraryHandle> library_handle,
          std::unique_ptr<FactoryImpl> &&factory_impl) noexcept
      : library_handle_{std::move(library_handle)}, factory_impl_{std::move(factory_impl)}
  {}

  /**
   * Construct a tracer from a configuration string.
   * @param tracer_config a representation of the tracer's config as a string.
   * @param error_message on failure this will contain an error message.
   * @return a Tracer on success or nullptr on failure.
   */
  std::shared_ptr<trace::Tracer> MakeTracer(nostd::string_view tracer_config,
                                            std::string &error_message) const noexcept
  {
    nostd::unique_ptr<char[]> plugin_error_message;
    auto tracer_handle = factory_impl_->MakeTracerHandle(tracer_config, plugin_error_message);
    if (tracer_handle == nullptr)
    {
      detail::CopyErrorMessage(plugin_error_message.get(), error_message);
      return nullptr;
    }
    return std::shared_ptr<trace::Tracer>{new (std::nothrow)
                                              Tracer{library_handle_, std::move(tracer_handle)}};
  }

private:
  // Note: The order is important here.
  //
  // It's undefined behavior to close the library while a loaded FactoryImpl is still active.
  std::shared_ptr<DynamicLibraryHandle> library_handle_;
  std::unique_ptr<FactoryImpl> factory_impl_;
};
}  // namespace plugin
OPENTELEMETRY_END_NAMESPACE
