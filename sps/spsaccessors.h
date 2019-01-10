#pragma once

#include "sps/sps.h"

namespace sps {

void add_accessors(sps::Registry& sreg, const vks::Registry& vreg);

struct AccessorFactory {
  virtual void process_struct(const sps::Registry& sreg,
                              const vks::Registry& vreg,
                              sps::Struct& sstruct) const = 0;
};

struct SingularAccessorFactory : AccessorFactory {
  void process_struct(const sps::Registry& sreg, const vks::Registry& vreg,
                      sps::Struct& sstruct) const override;
  virtual bool process_member(const sps::Registry& sreg,
                              const vks::Registry& vreg, sps::Struct& sstruct,
                              const sps::Member& member) const = 0;
};

}  // namespace sps
