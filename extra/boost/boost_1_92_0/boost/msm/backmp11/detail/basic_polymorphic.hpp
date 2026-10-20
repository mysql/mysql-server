// Copyright 2026 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_DETAIL_BASIC_POLYMORPHIC_VALUE_HPP
#define BOOST_MSM_BACKMP11_DETAIL_BASIC_POLYMORPHIC_VALUE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace boost::msm::backmp11::detail
{

// Basic polymorphic value with small object optimization.
// Similar to std::polymorphic (C++26).

struct control_block
{
    using copy_construct_fn_t = void(*)(void* dest, const void* src);
    using move_construct_fn_t = void(*)(void* dest, void* src) noexcept;
    using delete_fn_t = void(*)(void* obj) noexcept;

    inline void copy(void* dest, const void* src) const
    {
        // No copy function implies trivially copyable.
        if (is_inline && !copy_construct_fn)
        {
            std::memcpy(dest, src, size);
        }
        else
        {
            copy_construct_fn(dest, src);
        }
    }

    inline void move(void* dest, void* src) const noexcept
    {
        if (is_inline)
        {
            // No move function implies trivially movable.
            if (!move_construct_fn)
            {
                std::memcpy(dest, src, size);
            }
            else
            {
                move_construct_fn(dest, src);
            }
        }
        else
        {
            // dest and src are void** here — pointers to the m_ptr field.
            *static_cast<void**>(dest) = *static_cast<void**>(src);
            *static_cast<void**>(src) = nullptr;
        }
    }

    inline void destroy(void* obj) const noexcept
    {
        if (delete_fn && obj)
        {
            delete_fn(obj);
        }
    }

    copy_construct_fn_t copy_construct_fn{nullptr};
    move_construct_fn_t move_construct_fn{nullptr};
    delete_fn_t delete_fn{nullptr};
    uint8_t size{0};
    bool is_inline{false};
};

template <typename T, bool IsInline>
struct create_control_block;
template <>
struct create_control_block<void, false>
{
    static inline const control_block instance{};
};
template <>
struct create_control_block<void, true>
{
    static inline const control_block instance{};
};
template <typename T>
struct create_control_block<T, /*IsInline=*/false>
{
    static_assert(std::is_copy_constructible_v<T>,
                  "T must be copy constructible");

    static inline const control_block instance = []() constexpr {
        control_block self{};

        self.copy_construct_fn = [](void* dest, const void* src) {
            // src is &other.m_ptr (void* const*), dest is &m_ptr (void**).
            const T* typed_src = *static_cast<const T* const*>(src);
            *static_cast<T**>(dest) = new T(*typed_src);
        };
        self.delete_fn = [](void* ptr) noexcept {
            delete static_cast<T*>(ptr);
        };

        return self;
    }();
};

template <typename T, bool IsTriviallyCopyable>
struct inline_control_bock;
template <typename T>
struct inline_control_bock<T, /*IsTriviallyCopyable=*/false>
{
    static inline const control_block instance = []() constexpr {
        control_block self{};

        self.size = sizeof(T);
        self.is_inline = true;
        self.copy_construct_fn = [](void* dest, const void* src) {
            new (dest) T(*static_cast<const T*>(src));
        };
        if constexpr (!std::is_trivially_move_constructible_v<T>)
        {
            self.move_construct_fn = [](void* dest, void* src) noexcept {
                new (dest) T(std::move(*static_cast<T*>(src)));
            };
        }
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            self.delete_fn = [](void* ptr) noexcept {
                static_cast<T*>(ptr)->~T();
            };
        }

        return self;
    }();
};
template <typename T>
struct inline_control_bock<T, /*IsTriviallyCopyAble=*/true>
{
    static inline const control_block instance{nullptr, nullptr, nullptr,
                                               sizeof(T), true};
};

template <typename T>
struct create_control_block<T, /*IsInline=*/true>
{
    static_assert(std::is_copy_constructible_v<T>,
                  "T must be copy constructible");

    static inline const control_block& instance =
        inline_control_bock<T, std::is_trivially_copyable_v<T>>::instance;
};

template <typename T, bool IsInline>
const control_block& control_block_v =
    create_control_block<T, IsInline>::instance;

template <size_t BufferSize = 64 - sizeof(control_block*),
          size_t BufferAlignment = alignof(void*)>
class basic_polymorphic_base
{
    static_assert(BufferSize <= 255,
                  "BufferSize must not be bigger than 255 Bytes");

  public:
    bool is_inline() const
    {
        return m_control_block->is_inline;
    }

    void* get() const noexcept
    {
        if (m_control_block->is_inline)
        {
            return const_cast<void*>(reinterpret_cast<const void*>(&m_buffer));
        }
        else
        {
            return m_ptr;
        }
    }

  protected:
    template <typename U>
    using IsInline = std::bool_constant<
        sizeof(U) <= BufferSize && alignof(U) <= BufferAlignment &&
        // Force heap allocation if inline move would not be noexcept,
        // so that basic_polymorphic itself can always be noexcept moved.
        std::is_nothrow_move_constructible_v<U>>;

    template <typename U>
    static constexpr bool is_nothrow_constructible_v =
        std::is_nothrow_copy_constructible_v<U> && IsInline<U>::value;

    template <typename U, typename... Args>
    static constexpr bool is_nothrow_constructible_with_args_v =
        std::is_nothrow_constructible_v<U, Args...> && IsInline<U>::value;

    basic_polymorphic_base() noexcept = default;

    basic_polymorphic_base(const basic_polymorphic_base& other)
        : m_control_block(other.m_control_block)
    {
        m_control_block->copy(&m_ptr, &other.m_ptr);
    }

    basic_polymorphic_base& operator=(const basic_polymorphic_base& other)
    {
        if (this != &other)
        {
            destroy();
            m_control_block = other.m_control_block;
            m_control_block->copy(&m_ptr, &other.m_ptr);
        }
        return *this;
    }

    basic_polymorphic_base(basic_polymorphic_base&& other) noexcept
        : m_control_block(other.m_control_block)
    {
        m_control_block->move(&m_ptr, &other.m_ptr);
    }

    basic_polymorphic_base& operator=(basic_polymorphic_base&& other) noexcept
    {
        if (this != &other)
        {
            destroy();
            m_control_block = other.m_control_block;
            m_control_block->move(&m_ptr, &other.m_ptr);
        }
        return *this;
    }

    ~basic_polymorphic_base()
    {
        destroy();
    }

    template <typename U>
    explicit basic_polymorphic_base(const U& obj) noexcept(
        is_nothrow_constructible_v<U>)
        : m_control_block(&control_block_v<U, IsInline<U>::value>)
    {
        if constexpr (IsInline<U>::value)
        {
            if constexpr (std::is_trivially_copyable_v<U>)
            {
                std::memcpy(&m_buffer, &obj, sizeof(U));
            }
            else
            {
                new (&m_buffer) U(obj);
            }
        }
        else
        {
            m_ptr = new U(obj);
        }
    }

    template <typename U, typename... Args>
    explicit basic_polymorphic_base(
        std::in_place_type_t<U>,
        Args&&... args) noexcept(is_nothrow_constructible_with_args_v<U,
                                                                      Args...>)
        : m_control_block(&control_block_v<U, IsInline<U>::value>)
    {
        if constexpr (IsInline<U>::value)
        {
            new (&m_buffer) U(std::forward<Args>(args)...);
        }
        else
        {
            m_ptr = new U(std::forward<Args>(args)...);
        }
    }

  private:
    void destroy()
    {
        m_control_block->destroy(get());;
        if (!m_control_block->is_inline)
        {
            m_ptr = nullptr;
        }
    }

    const control_block* m_control_block{&control_block_v<void, true>};
    union {
        void* m_ptr;
        alignas(BufferAlignment) std::array<std::byte, BufferSize> m_buffer{};
    };
};

template <typename T,
          size_t BufferSize = 64 - sizeof(control_block*),
          size_t BufferAlignment = alignof(void*)>
class basic_polymorphic
    : public basic_polymorphic_base<BufferSize, BufferAlignment>
{
    using base = basic_polymorphic_base<BufferSize, BufferAlignment>;

  public:
    basic_polymorphic() noexcept = default;
    basic_polymorphic(const basic_polymorphic& other) = default;
    basic_polymorphic& operator=(const basic_polymorphic& other) = default;
    basic_polymorphic(basic_polymorphic&& other) noexcept = default;
    basic_polymorphic& operator=(basic_polymorphic&& other) noexcept = default;
    ~basic_polymorphic() = default;

    template <typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
    explicit basic_polymorphic(const U& obj) noexcept(
        base::template is_nothrow_constructible_v<U>)
        : base(obj)
    {
    }

    template <typename U, typename... Args,
              typename = std::enable_if_t<std::is_base_of_v<T, U>>>
    explicit basic_polymorphic(
        std::in_place_type_t<U> in_place,
        Args&&... args) noexcept(base::
                                     template is_nothrow_constructible_with_args_v<
                                         U, Args...>)
        : base(in_place, std::forward<Args>(args)...)
    {
    }

    template <typename U, typename... Args>
    static basic_polymorphic make(Args&&... args)
    {
        return basic_polymorphic{std::in_place_type<U>,
                                 std::forward<Args>(args)...};
    }

    template <typename U>
    static basic_polymorphic make(const U& obj)
    {
        return basic_polymorphic{obj};
    }

    T* get() const noexcept
    {
        return static_cast<T*>(base::get());
    }

    T& operator*() const noexcept
    {
        return *get();
    }

    T* operator->() const noexcept
    {
        return get();
    }
};

} // namespace boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_BASIC_POLYMORPHIC_VALUE_HPP
