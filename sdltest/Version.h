#pragma once

#include <vulkan/vulkan.h>
#include <ostream>

namespace spk {

class Version {
 public:
  Version(uint32_t major_, uint32_t minor_, uint32_t patch)
      : version(VK_MAKE_VERSION(major_, minor_, patch)) {}

  uint32_t major_() const { return VK_VERSION_MAJOR(version); }
  uint32_t minor_() const { return VK_VERSION_MINOR(version); }
  uint32_t patch() const { return VK_VERSION_PATCH(version); }

  operator uint32_t() { return version; }

 private:
  uint32_t version;
};

inline std::ostream& operator<<(std::ostream& o, Version v) {
  return o << v.major_() << '.' << v.minor_() << '.' << v.patch();
}

}  // namespace spk
