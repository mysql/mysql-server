#ifndef BOOST_LEAF_CONFIG_TLS_WIN32_HPP_INCLUDED
#define BOOST_LEAF_CONFIG_TLS_WIN32_HPP_INCLUDED

// Copyright 2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This header implements the TLS API specified in tls.hpp using Win32 TLS
// functions, allowing error objects to cross DLL boundaries on Windows. This
// implementation is enabled by defining BOOST_LEAF_CFG_WIN32=2 before including
// any LEAF headers.

#ifndef _WIN32
#   error "This header is only for Windows"
#endif

#include <atomic>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>
#include <windows.h>
#ifdef min
#   undef min
#endif
#ifdef max
#   undef max
#endif

#pragma pack(push, 8)

namespace boost { namespace leaf {

namespace detail
{
    __declspec(noreturn) inline void raise_fail_fast(NTSTATUS status) noexcept
    {
        EXCEPTION_RECORD rec = {};
        rec.ExceptionCode = status;
        rec.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
        RaiseFailFastException(&rec, nullptr, 0);
        BOOST_LEAF_UNREACHABLE;
    }

    template <class T, class... Args>
    T * heap_new(Args && ... args) noexcept
    {
        void * mem = HeapAlloc(GetProcessHeap(), 0, sizeof(T));
        if (!mem)
        {
            raise_fail_fast(STATUS_NO_MEMORY);
            BOOST_LEAF_UNREACHABLE;
        }
        return new (mem) T(static_cast<Args &&>(args)...);
    }

    template <class T>
    void heap_delete(T * p) noexcept
    {
        if (p)
        {
            p->~T();
            BOOL r = HeapFree(GetProcessHeap(), 0, p);
            BOOST_LEAF_ASSERT(r), (void) r;
        }
    }

    class srwlock_shared
    {
        srwlock_shared(srwlock_shared const &) = delete;
        srwlock_shared & operator=(srwlock_shared const &) = delete;

        SRWLOCK & lock_;

    public:

        explicit srwlock_shared(SRWLOCK & lock) noexcept:
            lock_(lock)
        {
            AcquireSRWLockShared(&lock_);
        }

        ~srwlock_shared() noexcept
        {
            ReleaseSRWLockShared(&lock_);
        }
    };

    class srwlock_exclusive
    {
        srwlock_exclusive(srwlock_exclusive const &) = delete;
        srwlock_exclusive & operator=(srwlock_exclusive const &) = delete;

        SRWLOCK & lock_;

    public:

        explicit srwlock_exclusive(SRWLOCK & lock) noexcept:
            lock_(lock)
        {
            AcquireSRWLockExclusive(&lock_);
        }

        ~srwlock_exclusive() noexcept
        {
            ReleaseSRWLockExclusive(&lock_);
        }
    };

    using atomic_unsigned_int = std::atomic<unsigned int>;

    template <int N, int I>
    struct cpp11_hash_step
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static std::uint32_t compute(char const (&str)[N], std::uint32_t hash) noexcept
        {
            return cpp11_hash_step<N, I - 1>::compute(str, (hash ^ static_cast<std::uint32_t>(str[I])) * 16777619u);
        }
    };

    template <int N>
    struct cpp11_hash_step<N, -1>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static std::uint32_t compute(char const (&)[N], std::uint32_t hash) noexcept
        {
            return hash;
        }
    };

    template <int N>
    BOOST_LEAF_ALWAYS_INLINE constexpr std::uint32_t cpp11_hash_string(char const (&str)[N]) noexcept
    {
        return cpp11_hash_step<N, N - 2>::compute(str, 2166136261u); // str[N-2] is the last character before the \0.
    }
} // namespace detail

namespace n
{
    template <class T>
    BOOST_LEAF_ALWAYS_INLINE constexpr std::uint32_t __cdecl h() noexcept
    {
        return detail::cpp11_hash_string(BOOST_LEAF_PRETTY_FUNCTION);
    }
}

namespace detail
{
    template<class T>
    BOOST_LEAF_ALWAYS_INLINE constexpr std::uint32_t type_hash() noexcept
    {
        return n::h<T>();
    }
}

} }

////////////////////////////////////////

namespace boost { namespace leaf {

// Thrown on TLS allocation failure.
class win32_tls_error:
    public std::runtime_error
{
public:
    explicit win32_tls_error(char const * what) noexcept:
        std::runtime_error(what)
    {
    }
};

namespace detail
{
    class tls_slot_index
    {
        tls_slot_index(tls_slot_index const &) = delete;
        tls_slot_index & operator=(tls_slot_index const &) = delete;
        tls_slot_index & operator=(tls_slot_index &&) = delete;

        DWORD idx_;

    public:

        BOOST_LEAF_ALWAYS_INLINE tls_slot_index():
            idx_(TlsAlloc())
        {
            if (idx_ == TLS_OUT_OF_INDEXES)
                throw_exception_(win32_tls_error("TLS_OUT_OF_INDEXES"));
        }

        BOOST_LEAF_ALWAYS_INLINE ~tls_slot_index() noexcept
        {
            if (idx_ == TLS_OUT_OF_INDEXES)
                return;
            BOOL r = TlsFree(idx_);
            BOOST_LEAF_ASSERT(r), (void) r;
        }

        BOOST_LEAF_ALWAYS_INLINE tls_slot_index(tls_slot_index && other) noexcept:
            idx_(other.idx_)
        {
            other.idx_ = TLS_OUT_OF_INDEXES;
        }

        BOOST_LEAF_ALWAYS_INLINE DWORD value() const noexcept
        {
            BOOST_LEAF_ASSERT(idx_ != TLS_OUT_OF_INDEXES);
            return idx_;
        }
    }; // class tls_slot_index

    template <int InitialCapacity>
    class tls_bucket
    {
        tls_bucket(tls_bucket const &) = delete;
        tls_bucket & operator=(tls_bucket const &) = delete;

        struct tls_entry
        {
            std::uint32_t type_hash;
            tls_slot_index idx;
        };

        tls_entry * data_;
        int size_;
        int capacity_;

        void grow()
        {
            int new_capacity = capacity_ ? capacity_ * 2 : InitialCapacity;
            void * mem = HeapAlloc(GetProcessHeap(), 0, new_capacity * sizeof(tls_entry));
            if (!mem)
                throw_exception_(std::bad_alloc());
            tls_entry * new_data = static_cast<tls_entry *>(mem);
            if (data_)
            {
                for (int i = 0, size = size_; i != size; ++i)
                {
                    new (new_data + i) tls_entry(std::move(data_[i]));
                    data_[i].~tls_entry();
                }
                BOOL r = HeapFree(GetProcessHeap(), 0, data_);
                BOOST_LEAF_ASSERT(r), (void) r;
            }
            data_ = new_data;
            capacity_ = new_capacity;
        }

    public:

        tls_bucket() noexcept:
            data_(nullptr),
            size_(0),
            capacity_(0)
        {
        }

        ~tls_bucket() noexcept
        {
            for (int i = 0, size = size_; i != size; ++i)
                data_[i].~tls_entry();
            if (data_)
            {
                BOOL r = HeapFree(GetProcessHeap(), 0, data_);
                BOOST_LEAF_ASSERT(r), (void) r;
            }
        }

        tls_slot_index const * check(std::uint32_t type_hash) const noexcept
        {
            for (int i = 0, size = size_; i != size; ++i)
                if (data_[i].type_hash == type_hash)
                    return &data_[i].idx;
            return nullptr;
        }

        tls_slot_index const & get(std::uint32_t type_hash)
        {
            if (tls_slot_index const * p = check(type_hash))
                return *p;
            if (size_ == capacity_)
                grow();
            tls_entry * e = new (data_ + size_) tls_entry{type_hash};
            ++size_;
            return e->idx;
        }
    }; // class tls_bucket

    template <int BucketCount, int InitialBucketCapacity>
    class tls_hash_map
    {
        static_assert((BucketCount & (BucketCount - 1)) == 0, "BucketCount must be a power of 2");

        tls_hash_map(tls_hash_map const &) = delete;
        tls_hash_map & operator=(tls_hash_map const &) = delete;

        tls_bucket<InitialBucketCapacity> buckets_[BucketCount];

        static int bucket_index(std::uint32_t type_hash) noexcept
        {
            return static_cast<int>(type_hash & (BucketCount - 1));
        }

    public:

        tls_hash_map() noexcept
        {
        }

        tls_slot_index const * check(std::uint32_t type_hash) const noexcept
        {
            return buckets_[bucket_index(type_hash)].check(type_hash);
        }

        tls_slot_index const & get(std::uint32_t type_hash)
        {
            return buckets_[bucket_index(type_hash)].get(type_hash);
        }
    };

    class slot_map
    {
        slot_map(slot_map const &) = delete;
        slot_map & operator=(slot_map const &) = delete;

        int refcount_;
        HANDLE const mapping_;
        tls_slot_index const error_id_slot_;
        mutable SRWLOCK lock_ = SRWLOCK_INIT;
        atomic_unsigned_int error_id_storage_;
        tls_hash_map<256, 4> hm_;

    public:

        // The constructor for error_id_slot_ may throw, but this constructor is intentionally noexcept.
        // While running out of TLS slots is not a reason to terminate the process, it is required that
        // there is a TLS slot available at least to store the error_id.
        explicit slot_map(HANDLE mapping) noexcept:
            refcount_(1),
            mapping_(mapping),
            error_id_storage_(1)
        {
            BOOST_LEAF_ASSERT(mapping != INVALID_HANDLE_VALUE);
        }

        ~slot_map() noexcept
        {
            BOOL r = CloseHandle(mapping_);
            BOOST_LEAF_ASSERT(r), (void) r;
        }

        BOOST_LEAF_ALWAYS_INLINE void add_ref() noexcept
        {
            BOOST_LEAF_ASSERT(refcount_ >= 1);
            ++refcount_;
        }

        BOOST_LEAF_ALWAYS_INLINE void release() noexcept
        {
            --refcount_;
            BOOST_LEAF_ASSERT(refcount_ >= 0);
            if (refcount_ == 0)
                heap_delete(this);
        }

        BOOST_LEAF_ALWAYS_INLINE DWORD error_id_slot() const noexcept
        {
            return error_id_slot_.value();
        }

        BOOST_LEAF_ALWAYS_INLINE atomic_unsigned_int & error_id_storage() noexcept
        {
            return error_id_storage_;
        }

        DWORD check(std::uint32_t type_hash) const noexcept
        {
            srwlock_shared lock(lock_);
            if (tls_slot_index const * p = hm_.check(type_hash))
                return p->value();
            return TLS_OUT_OF_INDEXES;
        }

        DWORD get(std::uint32_t type_hash)
        {
            srwlock_exclusive lock(lock_);
            DWORD idx = hm_.get(type_hash).value();
            BOOST_LEAF_ASSERT(idx != TLS_OUT_OF_INDEXES);
            return idx;
        }
    }; // class slot_map

    class module_state
    {
        module_state(module_state const &) = delete;
        module_state & operator=(module_state const &) = delete;

        static constexpr unsigned tls_failure_create_mapping = 0x01;
        static constexpr unsigned tls_failure_map_view = 0x02;

        void * hinstance_;
        unsigned tls_failures_;
        slot_map * sm_;

    public:

        constexpr module_state() noexcept:
            hinstance_(nullptr),
            tls_failures_(0),
            sm_(nullptr)
        {
        }

        BOOST_LEAF_ALWAYS_INLINE slot_map & sm() const noexcept
        {
            BOOST_LEAF_ASSERT(hinstance_);
            BOOST_LEAF_ASSERT(!(tls_failures_ & tls_failure_create_mapping));
            BOOST_LEAF_ASSERT(!(tls_failures_ & tls_failure_map_view));
            BOOST_LEAF_ASSERT(sm_);
            return *sm_;
        }

        BOOST_LEAF_ALWAYS_INLINE void update(PVOID hinstDLL, DWORD dwReason) noexcept
        {
            if (dwReason == DLL_PROCESS_ATTACH)
            {
                hinstance_ = hinstDLL;
                char name[32] = "Local\\boost_leaf_";
                {
                    constexpr static char const hex[] = "0123456789ABCDEF";
                    DWORD pid = GetCurrentProcessId();
                    for (int i = 7; i >= 0; --i)
                    {
                        name[17 + i] = hex[pid & 0xf];
                        pid >>= 4;
                    }
                    name[25] = '\0';
                }
                HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(slot_map *), name);
                DWORD mapping_status = GetLastError();
                if (!mapping)
                {
                    tls_failures_ |= tls_failure_create_mapping;
                    return;
                }
                BOOST_LEAF_ASSERT(mapping_status == ERROR_ALREADY_EXISTS || mapping_status == ERROR_SUCCESS);
                bool is_first_module = (mapping_status == ERROR_SUCCESS);
                slot_map * * mapped_ptr = static_cast<slot_map * *>(MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(slot_map *)));
                if (!mapped_ptr)
                {
                    tls_failures_ |= tls_failure_map_view;
                    BOOL r = CloseHandle(mapping);
                    BOOST_LEAF_ASSERT(r), (void) r;
                    return;
                }
                if (is_first_module)
                    sm_ = *mapped_ptr = heap_new<slot_map>(mapping);
                else
                {
                    sm_ = *mapped_ptr;
                    sm_->add_ref();
                    BOOL r = CloseHandle(mapping);
                    BOOST_LEAF_ASSERT(r), (void) r;
                }
                UnmapViewOfFile(mapped_ptr);
            }
            else if (dwReason == DLL_PROCESS_DETACH)
            {
                BOOST_LEAF_ASSERT(sm_ || tls_failures_);
                if (sm_)
                {
                    sm_->release();
                    sm_ = nullptr;
                }
            }
        }
    }; // class module_state

    template<int = 0>
    struct module
    {
        static module_state state;
    };

    template<int N>
    module_state module<N>::state;

    BOOST_LEAF_ALWAYS_INLINE unsigned generate_next_error_id() noexcept
    {
        static atomic_unsigned_int & counter = module<>::state.sm().error_id_storage();
        unsigned id = (counter += 4);
        BOOST_LEAF_ASSERT((id&3) == 1);
        return id;
    }

    inline void NTAPI tls_callback(PVOID hinstDLL, DWORD dwReason, PVOID) noexcept
    {
        module<>::state.update(hinstDLL, dwReason);
    }

#ifdef _MSC_VER
#   pragma section(".CRT$XLB", long, read)
#   pragma data_seg(push, ".CRT$XLB")

extern "C" __declspec(selectany) PIMAGE_TLS_CALLBACK boost_leaf_tls_callback = tls_callback;

#   pragma data_seg(pop)
#   ifdef _WIN64
#       pragma comment(linker, "/INCLUDE:boost_leaf_tls_callback")
#   else
#       pragma comment(linker, "/INCLUDE:_boost_leaf_tls_callback")
#   endif
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wattributes"

extern "C" __attribute__((used, selectany)) PIMAGE_TLS_CALLBACK boost_leaf_tls_callback __attribute__((section(".CRT$XLB"))) = tls_callback;

#   pragma GCC diagnostic pop
#else
#   error Unknown compiler, unable to define .CRT$XLB section
#endif
} // namespace detail

} } // namespace boost::leaf

////////////////////////////////////////

namespace boost { namespace leaf {

namespace tls
{
    BOOST_LEAF_ALWAYS_INLINE unsigned generate_next_error_id() noexcept
    {
        return detail::generate_next_error_id();
    }

    BOOST_LEAF_ALWAYS_INLINE void write_current_error_id(unsigned x) noexcept
    {
        using namespace detail;
        DWORD slot = module<>::state.sm().error_id_slot();
        BOOL r = TlsSetValue(slot, reinterpret_cast<void *>(static_cast<std::uintptr_t>(x)));
        BOOST_LEAF_ASSERT(r), (void) r;
    }

    BOOST_LEAF_ALWAYS_INLINE unsigned read_current_error_id() noexcept
    {
        using namespace detail;
        DWORD slot = module<>::state.sm().error_id_slot();
        LPVOID value = TlsGetValue(slot);
        BOOST_LEAF_ASSERT(GetLastError() == ERROR_SUCCESS);
        return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(value));
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE void reserve_ptr()
    {
        using namespace detail;
        thread_local DWORD const cached_slot = module<>::state.sm().get(type_hash<T>());
        BOOST_LEAF_ASSERT(cached_slot != TLS_OUT_OF_INDEXES), (void) cached_slot;
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE void write_ptr(T * p) noexcept
    {
        using namespace detail;
        thread_local DWORD const cached_slot = module<>::state.sm().check(type_hash<T>());
        DWORD slot = cached_slot;
        BOOST_LEAF_ASSERT(slot != TLS_OUT_OF_INDEXES);
        BOOL r = TlsSetValue(slot, p);
        BOOST_LEAF_ASSERT(r), (void) r;
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE T * read_ptr() noexcept
    {
        using namespace detail;
        thread_local DWORD cached_slot = TLS_OUT_OF_INDEXES;
        if (cached_slot == TLS_OUT_OF_INDEXES)
            cached_slot = module<>::state.sm().check(type_hash<T>());
        DWORD slot = cached_slot;
        if (slot == TLS_OUT_OF_INDEXES)
            return nullptr;
        LPVOID value = TlsGetValue(slot);
        BOOST_LEAF_ASSERT(GetLastError() == ERROR_SUCCESS);
        return static_cast<T *>(value);
    }
} // namespace tls

} } // namespace boost::leaf

#pragma pack(pop)

#endif // #ifndef BOOST_LEAF_CONFIG_TLS_WIN32_HPP_INCLUDED
