#pragma once

#include <glog/logging.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <memory>

namespace dvc {

class json_writer {
 public:
  json_writer(std::ostream& o)
      : ostream_wrapper(std::make_unique<rapidjson::OStreamWrapper>(o)),
        writer(*ostream_wrapper) {}

  void write_null() { CHECK(writer.Null()); }

  void write_bool(bool b) { CHECK(writer.Bool(b)); }

  void write_number(double d) { CHECK(writer.Double(d)); }

  void write_string(std::string_view sv) {
    CHECK(writer.String(sv.data(), sv.size()));
  }

  void write_key(std::string_view sv) {
    CHECK(writer.Key(sv.data(), sv.size()));
  }

  void start_object() { CHECK(writer.StartObject()); }

  void end_object() { CHECK(writer.EndObject()); }

  void start_array() { CHECK(writer.StartArray()); }

  void end_array() { CHECK(writer.EndArray()); }

 private:
  std::unique_ptr<rapidjson::OStreamWrapper> ostream_wrapper;
  rapidjson::Writer<rapidjson::OStreamWrapper> writer;
};

}  // namespace dvc
