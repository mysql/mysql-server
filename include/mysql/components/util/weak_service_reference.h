/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef WEAK_REQUIRES_SERVICE_GUARD
#define WEAK_REQUIRES_SERVICE_GUARD

#include <atomic>
#include <cassert>
#include <functional>
#include <string>
#include "mysql/components/my_service.h"
#include "mysql/components/service.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/dynamic_loader_service_notification.h"
#include "mysql/components/services/registry.h"
/**
  @brief A utility class to implement a delayed service reference

  This class allows a component to have a "weak" reference to a component
  service.

  @par Operation in default mode (keep references = true)
  In its default mode (keep references) when the weak reference is initialized
  the class checks if the service required already has implementations. If it
  does, the class takes a reference to the default one, calls the supplied
  function and keeps the reference. If there's no implementation of the service
  the class registers a listener to the
  dynamic_loader_services_loaded_notification broadcast service by implementing
  a dynamic_loader_services_loaded_notification service that, when called by the
  dynamic loader, will take a reference to the desired service, call the
  function supplied and keep the reference until deinit. And then it sets a flag
  preventing any further calls to the function.

  @par
  At deinit time, if there's an active reference, deinit calls the supplied
  function and passes it as a parameter. And then releases the reference.
  Otherwise, no call of the supplied deinit function is done.
  It also unregisters the dynamic_loader_services_loaded_notification callback,
  if registered.

  @par Operation in do-not-keep-references mode.
  When the weak reference is initialized the class checks if the service
  required already has implementations. If it does, then the class takes a
  reference to the default implementation, calls the supplied init function and
  <b>releases</b> the reference. It then proceeds to <b>unconditioanlly</b>
  register a listener to the dynamic_loader_services_loaded_notification
  broadcast service by implementing a
  dynamic_loader_services_loaded_notification service that, when called by the
  dynamic loader, will take a reference to the desired service, call the
  function supplied and then release the reference.
  Every time a new implementation is registered, the notification callback will
  be called so the weak reference can re-register itself again.

  @par
  At deinit time, deinit tries to acquire the required service and, if
  successful, calls the supplied deinit function and passes it as a parameter.
  Note that if the service implementation has been undefined in the meanwhile no
  call of the supplied deinit function is done.

  @warning
  Do not use the do-not-keep-references mode for anyting but the server!
  It is only justified for the server component (aka the bootstrap component)
  because if it doesn't use it no one will be able to unload the component
  implementing the service once captured by the bootstrap component.
  Yes, unloading service implementation component would be impossible, but
  that's a desired side effect since there is state that needs to be
  destroyed properly before the service implementation can be unloaded.

  Normal usage pattern is that the @ref weak_service_reference::init() is called
  during component initialization.

  And @ref weak_service_reference::deinit() is called during the component
  deinitialization.


  @warning Please pass the _no_lock registry variants to the deinit() call! It's
  because component deinit function is called while the registry lock is held.
  So trying to take the lock again (which is what the normal registry functions
  do) is going to lead to a deadlock!

  One can expect that the function argument is called either at init() time or
  asyncronously, possibly from anoher thread, when an implementation of a
  service is registered.

  Typical usage:
  @code
  #include "mysql/components/util/weak_service_reference.h"
  ...
  #include "mysql/components/services/foo.h"
  #include "mysql/components/services/registry.h"
  ...

  REQUIRES_SERVICE_PLACEHOLDER(registry_registration);
  REQUIRES_SERVICE_PLACEHOLDER_AS(registry, mysql_service_registry_no_lock);
  REQUIRES_SERVICE_PLACEHOLDER_AS(registry_registration,
                                mysql_service_registration_no_lock);

  const std::string c_name(component_foo), s_name("foo");
  typedef weak_service_reference<SERVICE_TYPE(foo), c_name, s_name>
    weak_foo_service;


  BEGIN_COMPONENT_REQUIRES(component_foo)
  ...
    REQUIRES_SERVICE(registry_registration),
    REQUIRES_SERVICE_IMPLEMENTATION_AS(registry_registration,
                                       mysql_minimal_chassis_no_lock,
                                       mysql_service_registration_no_lock),
    REQUIRES_SERVICE_IMPLEMENTATION_AS(registry, mysql_minimal_chassis_no_lock,
                                       mysql_service_registry_no_lock),
  ...
  END_COMPONENT_REQUIRES();

  bool component_init() {
    ...
    if (weak_foo_service::init(SERVICE_PLACEHOLDER(registry),
  SERVICE_PLACEHOLDER(registry_registration),
      [&](SERVICE_TYPE(foo) * foo_svc) {
        return 0 != foo_svc->define(12);
      }))
      return 1;
    ...
  }

  bool component_deinit() {
    ...
    if (weak_option::deinit(
      mysql_service_registry_no_lock, mysql_service_registration_no_lock,
      [&](SERVICE_TYPE(foo) * foo_svc) {
        return 0 != foo_svc->undefine(12);
      }))
      return 1;
  @endcode

  @tparam Service This is the type of the service to be called. E.g.
  SERVICE_TYPE(foo)
  @tparam container The name of the "container". Usually a component name.
     It has to be a rvalue ref since you would need a distinct set of the
     static members of the template class for every service/component combo.
  @tparam service_name The name of the service to try to call.
     It has to be a rvalue ref since you would need a distinct set of the
     static members of the template class for every service/component combo.
*/
template <typename Service, const std::string &container,
          const std::string &service_name>
class weak_service_reference {
  /**
     A single instance of the class to hold (and initialize) some data
     at init time.
  */
  inline static weak_service_reference<Service, container, service_name> *hton{
      nullptr};

  /**
    We need to store a reference to the registry since the init callback needs
    it.
  */
  inline static SERVICE_TYPE(registry) * registry{nullptr};
  /**
     A flag stating if the callback service implementation listening
     for new implementations of the service has been registered.
   */
  inline static bool callback_registered{false};

  /**
    true when the weak reference class is to keep the reference
    acquired for reuse until deinit is called.
  */
  inline static bool keep_active_reference{true};

  /**
    A flag if the init callback function has been called.
    This is to prevent multiple calls to the init callback.
    Ideally we'd unregister the callback altogether, but the callback
    is called while a reference to it is held, so it can't unregister
    itself due to an active reference.
    Hence we raise the flag to prevent further action and deregister
    at deinit()
  */
  std::atomic<bool> function_called{false};
  /**
    The init callback reference.
  */
  const std::function<bool(Service *)> function;

  /**
    A service_loaded listener implementation name of the following format:
    dynamic_loader_services_loaded_notification.&lt;conainer_name&gt;_&lt;service_name&gt;
  */
  std::string listener_name;

  my_service<Service> service_reference;

  /**
    @brief A private constructor for the hton

    @param func_arg The function to call when there's an implementation.
    active reference until deinit.
  */
  weak_service_reference(std::function<bool(Service *)> &func_arg)
      : function(func_arg) {
    listener_name =
        std::string("dynamic_loader_services_loaded_notification.") +
        container + std::string("_") + service_name;
  }

  /**
    @brief Helper function to take a reference to the service needed
     and call the init callback function.

    @retval true failure
    @retval false success
  */
  static bool call_function() {
    if (keep_active_reference) {
      if (!hton->service_reference.is_valid())
        hton->service_reference.acquire(service_name.c_str(), registry);

      if (hton->service_reference.is_valid()) {
        if (hton->function(hton->service_reference)) return true;
        hton->function_called = true;
      }
    } else {
      my_service<Service> svc(service_name.c_str(), registry);
      if (svc.is_valid()) {
        if (hton->function(svc)) return true;
        hton->function_called = true;
      }
    }
    return false;
  }

  /**
    @brief An implementation for the
     dynamic_loader_services_loaded_notification::notify service method

    This is called by the dynamic loader when a new service implementation
    is registered.
  */
  static DEFINE_BOOL_METHOD(notify,
                            (const char **services, unsigned int count)) try {
    if (!keep_active_reference || !hton->function_called) {
      for (unsigned idx = 0; idx < count; idx++) {
        std::string svc(services[idx]);
        if (svc.length() > service_name.length() &&
            svc[service_name.length()] == '.' && 0 == svc.find(service_name))
          return call_function() ? 1 : 0;
      }
    }
    return 0;
  } catch (...) {
    return 1;
  }

 public:
  /**
  @brief Initialize the weak reference class

  @param reg_arg A reference to the registry service implementation
  @param reg_reg_arg A reference to the registry_registration service
   implementation
  @param func_arg A function to be called when an implementation of the service
     is available. Typically used to initialize some state, e.g. allocate
  instance handles or register some features in registries.
  @param keep_active_reference_arg True if weak_reference is to keep an active
  reference until deinit.

  This is typically called by the component initialization.
  If there's already an implementation of the service required a reference to it
  is obtained and is passed to the function callback from the argument.

  If no implementations are available a listener for new implementation
  registration (an implementation of the
  dynamic_loader_services_loaded_notifications service) is registered into the
  registry and the function returns.

  @note Pass the "normal" references to the registry and the registry
  registration services here. init() is called without any locks being held to
  the registry.

  @retval true Failure
  @retval false Success
  */
  static bool init(SERVICE_TYPE(registry) * reg_arg,
                   SERVICE_TYPE(registry_registration) * reg_reg_arg,
                   std::function<bool(Service *)> func_arg,
                   bool keep_active_reference_arg = true) {
    registry = reg_arg;
    keep_active_reference = keep_active_reference_arg;
    assert(hton == nullptr);
    hton =
        new weak_service_reference<Service, container, service_name>(func_arg);
    if (call_function()) return true;

    if (!hton->function_called || !keep_active_reference) {
      static BEGIN_SERVICE_IMPLEMENTATION(
          x, dynamic_loader_services_loaded_notification) notify
      END_SERVICE_IMPLEMENTATION();
      if (reg_reg_arg->register_service(
              hton->listener_name.c_str(),
              (my_h_service) const_cast<void *>(
                  (const void *)&SERVICE_IMPLEMENTATION(
                      x, dynamic_loader_services_loaded_notification))))
        return true;
      callback_registered = true;
    }
    return false;
  }

  /**
    @brief Deinitializes a weak reference caller class

    If the init callback was called it will try to acquire a reference to the
    service and call the deinit callback if the reference is acquired.

    Then it will deregister the dynamic_loader_services_loaded_notification
    implementation, if it's been registered by init().

    And it will then proceed to delete the state in hton and reset the class.

    @param registry_arg A reference to the registry service implementation
    @param registry_registration_arg A reference to the registry_registration
    service implementation
    @param deinit_func_arg A (deinit) function to call if an implementation of
    the service required is definied. One typically reverses the action taken by
    the registration callback here, e.g. diposes of state, deregisters features
    etc.

    @retval true failure
    @retval false success
  */
  static bool deinit(SERVICE_TYPE(registry) * registry_arg,
                     SERVICE_TYPE(registry_registration) *
                         registry_registration_arg,
                     std::function<bool(Service *)> deinit_func_arg) {
    // the server may exit before init was called
    if (hton == nullptr) return false;

    if (keep_active_reference) {
      if (hton->function_called) {
        assert(hton->service_reference.is_valid());
        if (deinit_func_arg(hton->service_reference)) return true;
      }
      /*
        We need to release explicitly here becase it was acquired with the
        registry at init but we need to release it with the registry argument
        supplied.
      */
      if (hton->service_reference.is_valid()) {
        my_service<Service> svc(hton->service_reference, registry_arg);
        svc.release();
        hton->service_reference.untie();
      }
    } else {
      if (hton->function_called) {
        my_service<Service> svc(service_name.c_str(), registry_arg);
        if (svc.is_valid() && deinit_func_arg(svc)) return true;
      }
    }
    if (callback_registered &&
        registry_registration_arg->unregister(hton->listener_name.c_str()))
      return true;
    delete hton;
    hton = nullptr;
    registry = nullptr;
    callback_registered = false;
    return false;
  }
};

#endif /* WEAK_REQUIRES_SERVICE_GUARD */
