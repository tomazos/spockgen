#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <type_traits>
#include <vector>

namespace spk {

static_assert(std::is_same_v<VkDeviceSize, uint64_t>);

using bool32_t = VkBool32;

struct non_owning_t {
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
class array_view {
 public:
  array_view(T* ptr, size_t size) : ptr_(ptr), size_(size) {}

  T* get() const { return ptr_; }
  size_t size() const { return size_; }

 private:
  T* ptr_;
  size_t size_;
};

#define SPK_DEFINE_BITMASK_BITWISE_OPS(bitmask)                        \
  inline bitmask operator~(bitmask a) { return bitmask(~VkFlags(a)); } \
  inline bitmask operator|(bitmask a, bitmask b) {                     \
    return bitmask(VkFlags(a) | VkFlags(b));                           \
  }                                                                    \
  inline bitmask operator&(bitmask a, bitmask b) {                     \
    return bitmask(VkFlags(a) & VkFlags(b));                           \
  }                                                                    \
  inline bitmask operator^(bitmask a, bitmask b) {                     \
    return bitmask(VkFlags(a) ^ VkFlags(b));                           \
  }

}  // namespace spk
