#pragma once

#include <glog/logging.h>
#include <string>

namespace dvc {

class scanner {
 public:
  scanner(const std::string& filename, std::string data)
      : filename(filename), data(std::move(data)) {}

  static constexpr char eof = 0;

  char peek(size_t offset = 0) const {
    if (pos() + offset > data.size())
      return 0;
    else
      return data[pos() + offset];
  }

  size_t line() const { return line_; }

  char pop() {
    char c = peek();
    incr();
    return c;
  }

  size_t pos() const { return pos_; }
  void pos(size_t pos) { this->pos_ = pos; }

  void incr(size_t offset = 1) {
    if (peek() == '\n') line_++;
    pos_ += offset;
    CHECK_LE(pos(), data.size()) << "unexpected end of file " << filename;
  }

  const std::string& get_data() const { return data; }

 private:
  std::string filename;
  const std::string data;
  size_t pos_ = 0;
  size_t line_ = 0;
};

}  // namespace dvc
