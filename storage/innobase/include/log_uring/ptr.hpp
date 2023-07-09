#pragma once

#include <memory>

template<typename T> using ptr = std::shared_ptr<T>;

template<typename T> using wptr = std::weak_ptr<T>;

template<typename T> using uptr = std::unique_ptr<T>;

template<typename T, typename... TArgs> inline ptr<T> cs_new(TArgs &&...args) {
  return std::make_shared<T>(std::forward<TArgs>(args)...);
}