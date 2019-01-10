#pragma once

#include <glog/logging.h>
#include <string>
#include <vector>

namespace dvc {

template <class Token>
class parser {
 public:
  parser(const std::string& filename, std::vector<Token> data)
      : filename(filename), data(std::move(data)) {}

  const Token& peek(size_t offset = 0) const { return data.at(pos() + offset); }

  const Token& pop() {
    const Token& token = peek();
    incr();
    return token;
  }

  size_t pos() const { return pos_; }
  void pos(size_t pos) { this->pos_ = pos; }

  void incr(size_t offset = 1) {
    pos_ += offset;
    CHECK_LT(pos(), data.size()) << "unexpected end of file " << filename;
  }

 private:
  std::string filename;
  const std::vector<Token> data;
  size_t pos_ = 0;
};

}  // namespace dvc
