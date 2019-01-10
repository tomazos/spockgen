#include "spsaccessors.h"

#include <glog/logging.h>

#include "dvc/string.h"

namespace sps {

void SingularAccessorFactory::process_struct(const sps::Registry& sreg,
                                             const vks::Registry& vreg,
                                             sps::Struct& sstruct) const {
  for (sps::Member& member : sstruct.members) {
    if (member.accessors_assigned) continue;
    if (process_member(sreg, vreg, sstruct, member)) {
      member.accessors_assigned = true;
    }
  }
}

struct ValueAccessoryFactory : SingularAccessorFactory {
  bool process_member(const sps::Registry& sreg, const vks::Registry& vreg,
                      sps::Struct& sstruct,
                      const sps::Member& member) const override {
    if (member.empty_enum()) return true;

    auto accessor = new sps::ValueAccessor;
    accessor->member = &member;
    accessor->large = member.stype->size_estimate();
    sstruct.accessors.push_back(accessor);
    return true;
  }
};

struct BoolAccessoryFactory : SingularAccessorFactory {
  bool process_member(const sps::Registry& sreg, const vks::Registry& vreg,
                      sps::Struct& sstruct,
                      const sps::Member& member) const override {
    if (member.stype->to_string() != "spk::bool32_t") return false;

    std::string name = member.name;
    CHECK(dvc::endswith(name, "_"));
    name = name.substr(0, name.size() - 1);

    auto accessor = new sps::BoolAccessor;
    accessor->name = name;
    accessor->member = &member;
    sstruct.accessors.push_back(accessor);
    return true;
  }
};

struct SpanAccessoryFactory : AccessorFactory {
  void process_struct(const sps::Registry& sreg, const vks::Registry& vreg,
                      sps::Struct& sstruct) const override {
    size_t nmembers = sstruct.members.size();
    for (size_t i = 0; i < nmembers; ++i) {
      sps::Member& scount = sstruct.members.at(i);
      const vks::Member& vcount = sstruct.struct_->members.at(i);
      std::vector<size_t> js;
      for (size_t j = 0; j < nmembers; ++j) {
        if (i == j) continue;
        sps::Member& subject = sstruct.members.at(j);
        if (subject.len.empty()) continue;
        CHECK(subject.len.size() == 1);
        std::string len = subject.len.at(0);
        if (len != vcount.name) continue;

        std::string name = subject.name;
        CHECK(dvc::endswith(name, "_"));
        name = name.substr(0, name.size() - 1);
        if (dvc::startswith(name, "p_")) name = name.substr(2, name.size() - 2);

        auto accessor = new sps::SpanAccessor;
        accessor->name = name;
        accessor->count = &scount;
        accessor->subject = &subject;
        std::string tname = accessor->count->stype->to_string();
        CHECK(tname == "uint32_t" || tname == "size_t");
        sstruct.accessors.push_back(accessor);
        scount.accessors_assigned = true;
        subject.accessors_assigned = true;
      }
    }
  }
};

struct StringAccessoryFactory : SingularAccessorFactory {
  bool process_member(const sps::Registry& sreg, const vks::Registry& vreg,
                      sps::Struct& sstruct,
                      const sps::Member& member) const override {
    if (!member.null_terminated) return false;
    if (!member.len.empty()) return false;

    CHECK(member.stype->to_string() == "char const *");

    std::string name = member.name;

    CHECK(dvc::endswith(name, "_"));
    name = name.substr(0, name.size() - 1);

    if (dvc::startswith(name, "p_")) name = name.substr(2, name.size() - 2);

    auto accessor = new sps::StringAccessor;
    accessor->name = name;
    accessor->member = &member;
    sstruct.accessors.push_back(accessor);
    return true;
  }
};

void add_accessors(sps::Registry& sreg, const vks::Registry& vreg) {
  static std::vector<const AccessorFactory*> factories = {
      new SpanAccessoryFactory, new BoolAccessoryFactory,
      new StringAccessoryFactory, new ValueAccessoryFactory};

  for (sps::Struct* struct_ : sreg.structs)
    for (auto factory : factories)
      factory->process_struct(sreg, vreg, *struct_);
}

}  // namespace sps
