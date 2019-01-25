#include <gflags/gflags.h>
#include <glog/logging.h>

#include "spk/loader.h"
#include "spk/spock.h"

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  spk::loader loader;

  spk::global_dispatch_table global_dispatch_table;

  //  uint32_t property_count;
  //  global_dispatch_table.enumerate_instance_extension_properties(
  //      nullptr, &property_count, nullptr);
  //  std::vector<spk::extension_properties> extension_properties_vector(
  //      property_count);
  //
  //  global_dispatch_table.enumerate_instance_extension_properties(
  //      nullptr, &property_count, extension_properties_vector.data());
  //
  //  for (const spk::extension_properties& extension_properties :
  //       extension_properties_vector) {
  //    LOG(ERROR) << extension_properties.extension_name().data();
  //  }
  //
  //  spk::instance_create_info instance_create_info;
  //  //  instance_create_info.set_pp_enabled_extension_names(value)
  //
  //  spk::instance_ref instance;
  //  CHECK(spk::result::success == global_dispatch_table.create_instance(
  //                                    &instance_create_info, nullptr,
  //                                    &instance));
}
