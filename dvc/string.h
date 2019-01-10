#pragma once

#include <string>
#include <vector>

namespace dvc {

inline std::vector<std::string> split(const std::string& sep,
                                      const std::string& joined) {
  std::vector<std::string> result;
  size_t pos = 0;
  while (true) {
    size_t next_pos = joined.find(sep, pos);
    if (next_pos == std::string::npos) {
      result.push_back(joined.substr(pos));
      return result;
    }
    result.push_back(joined.substr(pos, next_pos - pos));
    pos = next_pos + sep.size();
  }
}

template <typename Container>
inline std::string join(const std::string& sep, const Container& container) {
  std::ostringstream oss;
  bool first = true;
  for (const auto& element : container) {
    if (!first) oss << sep;
    oss << element;
    first = false;
  }
  return oss.str();
}

constexpr bool startswith(std::string_view subject, std::string_view prefix) {
  if (prefix.size() > subject.size()) return false;

  return subject.substr(0, prefix.size()) == prefix;
}

constexpr bool endswith(std::string_view subject, std::string_view suffix) {
  if (suffix.size() > subject.size()) return false;

  return subject.substr(subject.size() - suffix.size()) == suffix;
}

}  // namespace dvc
