#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace spk {

static_assert(std::is_same_v<VkDeviceSize, uint64_t>);

using bool32_t = VkBool32;

class version {
 public:
  version(uint32_t v) : v(v) {}
  operator uint32_t() { return v; }

  version(int major, int minor, int patch)
      : v(VK_MAKE_VERSION(major, minor, patch)) {}
  int major_() const { return VK_VERSION_MAJOR(v); }
  int minor_() const { return VK_VERSION_MINOR(v); }
  int patch() const { return VK_VERSION_PATCH(v); }

 private:
  uint32_t v;
};

inline bool operator==(version a, version b) {
  return uint32_t(a) == uint32_t(b);
}
inline bool operator!=(version a, version b) {
  return uint32_t(a) != uint32_t(b);
}
inline bool operator<(version a, version b) {
  return uint32_t(a) < uint32_t(b);
}
inline bool operator>(version a, version b) {
  return uint32_t(a) > uint32_t(b);
}
inline bool operator<=(version a, version b) {
  return uint32_t(a) <= uint32_t(b);
}
inline bool operator>=(version a, version b) {
  return uint32_t(a) >= uint32_t(b);
}

inline std::ostream& operator<<(std::ostream& o, version v) {
  return o << v.major_() << "." << v.minor_() << "." << v.patch();
}

constexpr struct non_owning_t {
} non_owning;

class string_ptr {
 public:
  string_ptr(const std::string* s) : cstr_(s->c_str()) {}
  template <size_t N>
  string_ptr(const char (&cstr)[N]) : cstr_(cstr) {}
  string_ptr(non_owning_t, const char* cstr) : cstr_(cstr) {}

  const char* get() const { return cstr_; }

 private:
  const char* cstr_;
};

class string_array : public std::vector<std::string> {
 public:
  using std::vector<std::string>::vector;

  const char* const* get() const {
    string_ptrs_.resize(strings_.size());
    for (size_t i = 0; i < strings_.size(); ++i)
      string_ptrs_[i] = strings_[i].c_str();
    return string_ptrs_.data();
  }

 private:
  mutable std::vector<const char*> string_ptrs_;
  std::vector<std::string> strings_;
};

class string_array_ptr {
 public:
  string_array_ptr(const string_array* sa)
      : cstrarr_(sa->get()), size_(sa->size()) {}
  string_array_ptr(const char* const* cstrarr, size_t size)
      : cstrarr_(cstrarr), size_(size) {}

  const char* const* get() { return cstrarr_; }
  size_t size() const { return size_; }

 private:
  const char* const* cstrarr_;
  size_t size_;
};

template <typename T>
class array_ptr {
 public:
  array_ptr(T* ptr, size_t size) : ptr_(ptr), size_(size) {}

  T* data() const { return ptr_; }
  size_t size() const { return size_; }

 private:
  T* ptr_;
  size_t size_;
};

template <typename T>
class array_view {
 public:
  array_view() : ptr_(nullptr), size_(0) {}
  array_view(T* ptr, size_t size) : ptr_(ptr), size_(size) {}
  template <size_t N>
  array_view(T (&array)[N]) : ptr_(array), size_(N) {}
  template <size_t N>
  array_view(const std::array<T, N>& array) : ptr_(array.data()), size_(N) {}
  array_view(const std::vector<T>& vector)
      : ptr_(vector.data()), size_(vector.size()) {}

  T* data() const { return ptr_; }
  size_t size() const { return size_; }

 private:
  T* ptr_;
  size_t size_;
};

template <>
class array_view<const void> : public std::string_view {
 public:
  array_view(const void* ptr, size_t sz)
      : std::string_view((const char*)ptr, sz) {}
  using std::string_view::string_view;
};

template <>
class array_view<void> {
 public:
 public:
  array_view() : ptr_(nullptr), size_(0) {}
  array_view(void* ptr, size_t size) : ptr_(ptr), size_(size) {}

  void* data() const { return ptr_; }
  size_t size() const { return size_; }

 private:
  void* ptr_;
  size_t size_;
};

#define SPK_DEFINE_BITMASK_BITWISE_OPS(bitmask)                           \
  constexpr bitmask operator~(bitmask a) { return bitmask(~VkFlags(a)); } \
  constexpr bitmask operator|(bitmask a, bitmask b) {                     \
    return bitmask(VkFlags(a) | VkFlags(b));                              \
  }                                                                       \
  constexpr VkFlags operator&(bitmask a, bitmask b) {                     \
    return VkFlags(a) & VkFlags(b);                                       \
  }                                                                       \
  constexpr bitmask operator^(bitmask a, bitmask b) {                     \
    return bitmask(VkFlags(a) ^ VkFlags(b));                              \
  }

#define SPK_BEGIN_BITMASK_OUTPUT(enumeration) \
  if (e == enumeration(0)) return o << "0";   \
  bool first = true;

#define SPK_BITMASK_OUTPUT_ENUMERATOR(enumeration, enumerator) \
  if (e & spk::enumeration::enumerator) {                      \
    if (first)                                                 \
      first = false;                                           \
    else                                                       \
      o << "|";                                                \
    o << #enumerator;                                          \
  }

#define SPK_END_BITMASK_OUTPUT(enumeration) \
  if (first) o << int(e);                   \
  return o;

#define SPK_BEGIN_ENUMERATION_OUTPUT(enumeration)
#define SPK_ENUMERATION_OUTPUT_ENUMERATOR(enumeration, enumerator) \
  if (e == spk::enumeration::enumerator)                           \
    return o << #enumerator;                                       \
  else

#define SPK_END_ENUMERATION_OUTPUT(enumeration) return o << int(e);

#define SPK_BEGIN_RESULT_ERRORS           \
  struct error : std::exception {         \
    virtual spk::result code() const = 0; \
  };

#define SPK_DEFINE_RESULT_ERROR(name)                               \
  struct name : error {                                             \
    spk::result code() const override { return spk::result::name; } \
    const char* what() const noexcept override { return #name; }    \
  };

#define SPK_END_RESULT_ERRORS                                            \
  struct unexpected_command_result : error {                             \
    spk::result res_;                                                    \
    std::string what_;                                                   \
    unexpected_command_result(spk::result res, const char* function)     \
        : res_(res) {                                                    \
      std::ostringstream oss;                                            \
      oss << "unexpected " << res << " returned from " << function;      \
      what_ = oss.str();                                                 \
    }                                                                    \
                                                                         \
    spk::result code() const override { return res_; }                   \
    const char* what() const noexcept override { return what_.c_str(); } \
  };

template <typename T>
struct strip_member_function;
template <class C, typename F>
struct strip_member_function<F C::*> {
  using type = F;
};

template <typename T>
using strip_member_function_t = typename strip_member_function<T>::type;

struct nomove {
  nomove() = default;
  nomove(const nomove&) = delete;
  nomove(nomove&&) = delete;
  nomove& operator=(const nomove&) = delete;
  nomove& operator=(nomove&&) = delete;
};

}  // namespace spk
