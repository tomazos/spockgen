#pragma once

#include <vulkan/vulkan.h>
#include <type_traits>

namespace vkxmltest {

template<typename T, typename U>
constexpr bool checkeq(T a, U b) {
  if constexpr(std::is_same_v<T, const char*>) {
    for (size_t i = 0; true; i++) {
      if (a[i] != b[i])
        return false;
      if (a[i] == 0)
        return true;
    }
  } else
    return a == b;
}

}  // namespace vkxmltest

#define VKXMLTEST_CHECK_CONSTANT(name, value) \
  static_assert(vkxmltest::checkeq(name, value));

#define VKXMLTEST_CHECK_ENUMERATION(enumeration) \
using VKXMLTEST_ENUMERATION_DUMMY_##enumeration = enumeration; \
  static_assert(sizeof(enumeration) == 4);

#define VKXMLTEST_CHECK_ENUMERATOR(enumeration, enumerator) \
    static_assert(std::is_enum_v<enumeration>); \
    static_assert(std::is_same_v<enumeration, decltype(enumerator)>);

#define VKXMLTEST_CHECK_BITMASK(bitmask) \
    static_assert(std::is_same_v<bitmask, VkFlags>);

#define VKXMLTEST_CHECK_BITMASK_REQUIRES(bitmask, enumeration)

#define VKXMLTEST_CHECK_HANDLE(handle) \
  using VKXMLTEST_HANDLE_DUMMY_##handle = handle;

#define VKXMLTEST_CHECK_HANDLE_PARENT(handle, parent) \
  namespace VKXMLTEST_HANDLE_DUMMY_PARENT_##handle { \
     using VKXMLTEST_PARENT_DUMMY_##parent = parent; \
  }

#define VKXMLTEST_CHECK_STRUCT(struct_, is_union) \
    static_assert(is_union ? std::is_union_v<struct_> : std::is_class_v<struct_>);

template<typename T> struct strip_member_ptr;
template<class C, typename T> struct strip_member_ptr<T C::*> {
  using type = T;
};
template<typename T>
using strip_member_ptr_t = typename strip_member_ptr<T>::type;

#define VKXMLTEST_CHECK_STRUCT_MEMBER(struct_, member_name, member_type) \
    static_assert(std::is_same_v<strip_member_ptr_t<decltype(&struct_::member_name)>, member_type>);

#define VKXMLTEST_CHECK_FUNCPOINTER(funcpointer, ...) \
    static_assert(std::is_pointer_v<funcpointer>); \
    static_assert(std::is_function_v<std::remove_pointer_t<funcpointer>>); \
    static_assert(std::is_same_v<funcpointer, __VA_ARGS__>);

#define VKXMLTEST_CHECK_COMMAND(command, ...) \
    static_assert(std::is_pointer_v<PFN_##command>); \
    static_assert(std::is_function_v<std::remove_pointer_t<PFN_##command>>); \
    static_assert(std::is_same_v<PFN_##command, __VA_ARGS__>);

#define VKXMLTEST_MAIN int main() {}
