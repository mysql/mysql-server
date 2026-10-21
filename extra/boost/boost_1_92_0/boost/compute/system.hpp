//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_SYSTEM_HPP
#define BOOST_COMPUTE_SYSTEM_HPP

#include <string>
#include <vector>
#include <cstdlib>

#include <boost/throw_exception.hpp>

#if defined(BOOST_COMPUTE_THREAD_SAFE) 
#  if defined(BOOST_COMPUTE_USE_CPP11)
#    include <mutex>
#    include <thread>
#    include <atomic>
#  else
#    include <boost/thread/mutex.hpp>
#    include <boost/thread/lock_guard.hpp>
#    include <boost/atomic.hpp>
#  endif
#endif 

#include <boost/compute/cl.hpp>
#include <boost/compute/device.hpp>
#include <boost/compute/context.hpp>
#include <boost/compute/platform.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/detail/getenv.hpp>
#include <boost/compute/exception/no_device_found.hpp>
#include <boost/compute/exception/context_error.hpp>

namespace boost {
namespace compute {

/// \class system
/// \brief Provides access to platforms and devices on the system.
///
/// The system class contains a set of static functions which provide access to
/// the OpenCL platforms and compute devices on the host system.
///
/// The default_device() convenience method automatically selects and returns
/// the "best" compute device for the system following a set of heuristics and
/// environment variables. This simplifies setup of the OpenCL enviornment.
///
/// \see platform, device, context
class system
{
public:
    /// Returns the default compute device for the system.
    ///
    /// The default device is selected based on a set of heuristics and can be
    /// influenced using one of the following environment variables:
    ///
    /// \li \c BOOST_COMPUTE_DEFAULT_DEVICE -
    ///        name of the compute device (e.g. "GTX TITAN")
    /// \li \c BOOST_COMPUTE_DEFAULT_DEVICE_TYPE
    ///        type of the compute device (e.g. "GPU" or "CPU")
    /// \li \c BOOST_COMPUTE_DEFAULT_PLATFORM -
    ///        name of the platform (e.g. "NVIDIA CUDA")
    /// \li \c BOOST_COMPUTE_DEFAULT_VENDOR -
    ///        name of the device vendor (e.g. "NVIDIA")
    /// \li \c BOOST_COMPUTE_DEFAULT_ENFORCE -
    ///        If this is set to "1", then throw a no_device_found() exception
    ///        if any of the above environment variables is set, but a matching
    ///        device was not found.
    ///
    /// The default device is determined once on the first time this function
    /// is called. Calling this function multiple times will always result in
    /// the same device being returned.
    ///
    /// If no OpenCL device is found on the system, a no_device_found exception
    /// is thrown.
    ///
    /// For example, to print the name of the default compute device on the
    /// system:
    /// \code
    /// // get the default compute device
    /// boost::compute::device device = boost::compute::system::default_device();
    ///
    /// // print the name of the device
    /// std::cout << "default device: " << device.name() << std::endl;
    /// \endcode
    static device default_device()
    {
        return init_default_device();
    }

    /// Returns the device with \p name.
    ///
    /// \throws no_device_found if no device with \p name is found.
    static device find_device(const std::string &name)
    {
        const std::vector<device> devices = system::devices();
        for(size_t i = 0; i < devices.size(); i++){
            const device& device = devices[i];

            if(device.name() == name){
                return device;
            }
        }

        BOOST_THROW_EXCEPTION(no_device_found());
    }

    /// Returns a vector containing all of the compute devices on
    /// the system.
    ///
    /// For example, to print out the name of each OpenCL-capable device
    /// available on the system:
    /// \code
    /// for(const auto &device : boost::compute::system::devices()){
    ///     std::cout << device.name() << std::endl;
    /// }
    /// \endcode
    static std::vector<device> devices()
    {
        std::vector<device> devices;

        const std::vector<platform> platforms = system::platforms();
        for(size_t i = 0; i < platforms.size(); i++){
            const std::vector<device> platform_devices = platforms[i].devices();

            devices.insert(
                devices.end(), platform_devices.begin(), platform_devices.end()
            );
        }

        return devices;
    }

    /// Returns the number of compute devices on the system.
    static size_t device_count()
    {
        size_t count = 0;

        const std::vector<platform> platforms = system::platforms();
        for(size_t i = 0; i < platforms.size(); i++){
            count += platforms[i].device_count();
        }

        return count;
    }

    /// Returns the default context for the system.
    ///
    /// The default context is created for the default device on the system
    /// (as returned by default_device()).
    ///
    /// The default context is created once on the first time this function is
    /// called. Calling this function multiple times will always result in the
    /// same context object being returned.
    static context default_context()
    {
        return init_default_context();
    }

    /// Returns the default command queue for the system.
    ///
    /// If user-provided command queue is given, the system-wide default context
    /// and default device will be set up appropriately so that the default queue
    /// matches the default context and device.
    ///
    /// If the OpenCL context and device associated with user-provided command queue 
    /// does not match the default context and device that have already been set,
    /// a set_default_queue_error exception is thrown. For example:
    ///
    /// \snippet test/test_attach_user_queue_error.cpp queue_mismatch
    ///
    /// The default queue is created once on the first time this function is
    /// called. Calling this function multiple times will always result in the
    /// same command queue object being returned.
    static command_queue& default_queue(const command_queue &user_queue = command_queue())
    {
        return init_default_queue(user_queue);
    }

    /// Blocks until all outstanding computations on the default
    /// command queue are complete.
    ///
    /// This is equivalent to:
    /// \code
    /// system::default_queue().finish();
    /// \endcode
    static void finish()
    {
        default_queue().finish();
    }

    /// Returns a vector containing each of the OpenCL platforms on the system.
    ///
    /// For example, to print out the name of each OpenCL platform present on
    /// the system:
    /// \code
    /// for(const auto &platform : boost::compute::system::platforms()){
    ///     std::cout << platform.name() << std::endl;
    /// }
    /// \endcode
    static std::vector<platform> platforms()
    {
        cl_uint count = 0;
        clGetPlatformIDs(0, 0, &count);

        std::vector<platform> platforms;
        if(count > 0)
        {
            std::vector<cl_platform_id> platform_ids(count);
            clGetPlatformIDs(count, &platform_ids[0], 0);

            for(size_t i = 0; i < platform_ids.size(); i++){
                platforms.push_back(platform(platform_ids[i]));
            }
        }
        return platforms;
    }

    /// Returns the number of compute platforms on the system.
    static size_t platform_count()
    {
        cl_uint count = 0;
        clGetPlatformIDs(0, 0, &count);
        return static_cast<size_t>(count);
    }

private:
    /// \internal_
    static device find_default_device()
    {
        // get a list of all devices on the system
        const std::vector<device> devices_ = devices();
        if(devices_.empty()){
            BOOST_THROW_EXCEPTION(no_device_found());
        }

        // check for device from environment variable
        const char *name     = detail::getenv("BOOST_COMPUTE_DEFAULT_DEVICE");
        const char *type     = detail::getenv("BOOST_COMPUTE_DEFAULT_DEVICE_TYPE");
        const char *platform = detail::getenv("BOOST_COMPUTE_DEFAULT_PLATFORM");
        const char *vendor   = detail::getenv("BOOST_COMPUTE_DEFAULT_VENDOR");
        const char *enforce  = detail::getenv("BOOST_COMPUTE_DEFAULT_ENFORCE");

        if(name || type || platform || vendor){
            for(size_t i = 0; i < devices_.size(); i++){
                const device& device = devices_[i];
                if (name && !matches(device.name(), name))
                    continue;

                if (type && matches(std::string("GPU"), type))
                    if (!(device.type() & device::gpu))
                        continue;

                if (type && matches(std::string("CPU"), type))
                    if (!(device.type() & device::cpu))
                        continue;

                if (platform && !matches(device.platform().name(), platform))
                    continue;

                if (vendor && !matches(device.vendor(), vendor))
                    continue;

                return device;
            }

            if(enforce && enforce[0] == '1')
                BOOST_THROW_EXCEPTION(no_device_found());
        }

        // find the first gpu device
        for(size_t i = 0; i < devices_.size(); i++){
            const device& device = devices_[i];

            if(device.type() & device::gpu){
                return device;
            }
        }

        // find the first cpu device
        for(size_t i = 0; i < devices_.size(); i++){
            const device& device = devices_[i];

            if(device.type() & device::cpu){
                return device;
            }
        }

        // return the first device found
        return devices_[0];
    }

    /// \internal_
    static bool matches(const std::string &str, const std::string &pattern)
    {
        return str.find(pattern) != std::string::npos;
    }

    /// \internal_
    static device init_default_device(const device &user_device = device())
    {
        static device default_device;

#ifdef BOOST_COMPUTE_THREAD_SAFE
    #ifdef BOOST_COMPUTE_USE_CPP11
        using namespace std;
    #else 
        using namespace boost;
    #endif 
        static atomic<bool> is_init;
        static mutex init_mutex;

        bool is_init_value = is_init.load(memory_order_consume);
        if (!is_init_value)
        {
            lock_guard<mutex> lock(init_mutex);
            is_init_value = is_init.load(memory_order_consume);
            if (!is_init_value)
            {
                default_device = user_device.get() ? 
                    user_device : find_default_device();

                is_init.store(true, memory_order_release);
            }
        }
#else // BOOST_COMPUTE_THREAD_SAFE
        if (!default_device.get())
        {
            default_device = user_device.get() ? 
                user_device : find_default_device();
        }
#endif // BOOST_COMPUTE_THREAD_SAFE   
        return default_device;
    }

    /// \internal_
    static context init_default_context(const context &user_context = context())
    {
        static context default_context;

#ifdef BOOST_COMPUTE_THREAD_SAFE
    #ifdef BOOST_COMPUTE_USE_CPP11
        using namespace std;
    #else 
        using namespace boost;
    #endif 
        static atomic<bool> is_init;
        static mutex init_mutex;

        bool is_init_value = is_init.load(memory_order_consume);
        if (!is_init_value)
        {
            lock_guard<mutex> lock(init_mutex);
            is_init_value = is_init.load(memory_order_consume);
            if (!is_init_value)
            {
                default_context = user_context.get() ? 
                    user_context : context(default_device());

                is_init.store(true, memory_order_release);
            }
        }
#else // BOOST_COMPUTE_THREAD_SAFE
        if (!default_context.get())
        {
            default_context = user_context.get() ?
                user_context : context(default_device());
        }
#endif // BOOST_COMPUTE_THREAD_SAFE
        return default_context;
    }

    /// \internal_
    static void init_default_device_and_context(const command_queue &user_queue)
    {
        device user_device = user_queue.get_device();
        context user_context = user_queue.get_context();

        if ( (user_device != init_default_device(user_device)) ||
             (user_context != init_default_context(user_context)) )
        {
            // Try invoking default_queue() before anything else
            BOOST_THROW_EXCEPTION(set_default_queue_error());  
        }
    }
    
    /// \internal_
    static command_queue& init_default_queue(const command_queue &user_queue = command_queue())
    {
        static command_queue default_queue;

#ifdef BOOST_COMPUTE_THREAD_SAFE
    #ifdef BOOST_COMPUTE_USE_CPP11
        using namespace std;
    #else 
        using namespace boost;
    #endif 
        static atomic<bool> is_init;
        static mutex init_mutex;

        bool is_init_value = is_init.load(memory_order_consume);
        if (!is_init_value)
        {
            lock_guard<mutex> lock(init_mutex);
            is_init_value = is_init.load(memory_order_consume);
            if (!is_init_value)
            {
                if (user_queue.get())
                    init_default_device_and_context(user_queue);

                default_queue = user_queue.get() ? 
                    user_queue : 
                    command_queue(default_context(), default_device());

                is_init.store(true, memory_order_release);
            }
        }
#else // BOOST_COMPUTE_THREAD_SAFE
        if (!default_queue.get())
        {
            if (user_queue.get())
                init_default_device_and_context(user_queue);

            default_queue = user_queue.get() ? 
                user_queue :
                command_queue(default_context(), default_device());
        }
#endif // BOOST_COMPUTE_THREAD_SAFE
        else
        {
            BOOST_ASSERT_MSG(user_queue.get() == 0, 
                "Default command queue has already been set.");
        }
        return default_queue;
    }
};

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_SYSTEM_HPP
