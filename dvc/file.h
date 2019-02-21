#pragma once

#include <glog/logging.h>
#include <filesystem>
#include <fstream>

namespace dvc {

class file_reader {
 public:
  file_reader(const std::filesystem::path& fspath) {
    ifs.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
    ifs.open(fspath.string().c_str(), std::ios::binary | std::ios::in);
  }

  size_t size() {
    size_t pos = tell();
    ifs.seekg(0, std::ios::end);
    size_t len = tell();
    seek(pos);
    return len;
  }

  void read(void* buf, size_t n) { ifs.read((char*)buf, n); }

  void seek(size_t pos) { ifs.seekg(pos); }

  size_t tell() { return ifs.tellg(); }

 private:
  std::ifstream ifs;
};

constexpr struct append_t {
} append;
constexpr struct truncate_t {
} truncate;

class file_writer {
 public:
  file_writer(const std::filesystem::path& fspath, append_t) {
    open(fspath, std::ios::ate | std::ios::binary);
  }
  file_writer(const std::filesystem::path& fspath, truncate_t) {
    open(fspath, std::ios::trunc | std::ios::binary);
  }
  void write(const void* buf, size_t n) { ofs.write((const char*)buf, n); }
  void write(std::string_view sv) { write(sv.data(), sv.size()); }
  template <typename... Args>
  void print(Args&&... args) {
    (ofs << ... << std::forward<Args>(args));
  }
  void println() { ofs << std::endl; }
  template <typename... Args>
  void println(Args&&... args) {
    print(std::forward<Args>(args)...);
    ofs << std::endl;
  }

  std::ostream& ostream() { return ofs; }

 private:
  void open(const std::filesystem::path& fspath,
            std::ios::openmode mode_extra) {
    ofs.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
    ofs.open(fspath.string().c_str(),
             std::ios::binary | std::ios::out | mode_extra);
  }
  std::ofstream ofs;
};

inline std::string load_file(const std::filesystem::path& filename) {
  file_reader reader(filename);
  std::string s;
  s.resize(reader.size());
  reader.read(&s[0], s.size());
  return s;
}

inline void touch_file(const std::filesystem::path& fspath) {
  file_writer writer(fspath, append);
}

}  // namespace dvc
