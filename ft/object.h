#pragma once

#include <type_traits>
#include <utility>

#include "ft/error.h"
#include "ft/freetype.h"

namespace ft {

template <typename T, FT_Error (*deleter)(T)>
class object {
 public:
  static_assert(std::is_pointer_v<T>);
  using type = T;
  using inner = typename std::remove_pointer_t<type>;

  object() : object_(nullptr) {}

  template <typename Creator, typename... Args>
  object(Creator creator, Args&&... args) : object_(nullptr) {
    throw_on_error(creator(std::forward<Args>(args)..., &object_));
  }

  explicit object(type object) : object_(object) {}

  object(object&& other) : object_(other.object_) { other.object_ = nullptr; }

  object& operator=(object&& other) {
    clear();
    object_ = other.object_;
    other.object_ = nullptr;
  }

  ~object() { clear(); }

  operator type() const { return object_; }

  type operator->() const { return object_; }

  type* ptr() { return &object_; }

 private:
  void clear() {
    if (object_) {
      throw_on_error(deleter(object_));
      object_ = nullptr;
    }
  }

  type object_;

  object(const object&) = delete;
  object& operator=(const object&) = delete;
};

}  // namespace ft
