#pragma once

#include "vks/vks.h"

namespace sps {

struct Entity : vks::Entity {};

struct Enumerator : Entity {
  const vks::Constant* constant;
};

struct Enumeration : Entity {
  std::string zeroinit() const override { return "= spk::" + name + "(0)"; }

  const vks::Enumeration* enumeration;
  std::vector<Enumerator> enumerators;
  std::vector<std::string> aliases;
};

struct Bitmask : Entity {
  std::string zeroinit() const override { return "= spk::" + name + "(0)"; }

  const vks::Bitmask* bitmask;
  std::vector<Enumerator> enumerators;
  std::vector<std::string> aliases;
};

struct Constant : Entity {
  const vks::Constant* constant;
};

struct Command;  // fwd decl

struct Handle : Entity {
  std::string fullname;
  const vks::Handle* handle;
  std::vector<std::string> aliases;
  std::vector<Command*> commands;
};

struct Name : vks::Type {
  const sps::Entity* entity;
  std::string to_string() const override { return "spk::" + entity->name; };
  void get_entity_dep(const vks::Entity*& entity,
                      bool& complete) const override {
    entity = this->entity;
    complete = true;
  }
  std::string zeroinit() const override { return entity->zeroinit(); }
};

struct Member {
  std::string name;
  std::vector<std::string> len;
  std::vector<bool> optional_;
  bool optional(size_t pos) const {
    return (pos >= optional_.size() ? false : optional_.at(pos));
  }
  const vks::Type* vtype;
  const vks::Type* stype;
  bool zero = true;
  bool null_terminated = false;
  bool accessors_assigned = false;

  bool empty_enum() const {
    if (auto name = dynamic_cast<const sps::Name*>(stype)) {
      if (auto enumeration =
              dynamic_cast<const sps::Enumeration*>(name->entity)) {
        if (enumeration->enumerators.empty()) return true;
      } else if (auto bitmask =
                     dynamic_cast<const sps::Bitmask*>(name->entity)) {
        if (bitmask->enumerators.empty()) return true;
      }
    }
    return false;
  }
};

struct Accessor {
  virtual ~Accessor() = default;
};

struct ValueAccessor : Accessor {
  const Member* member;
  bool large;
};

struct StringAccessor : Accessor {
  const Member* member;
  std::string name;
};

struct BoolAccessor : Accessor {
  const Member* member;
  std::string name;
};

struct SpanAccessor : Accessor {
  std::string name;
  const Member* count;
  const Member* subject;
};

struct Struct : Entity {
  const vks::Struct* struct_;
  std::vector<std::string> aliases;
  std::vector<Member> members;
  std::vector<Accessor*> accessors;
  std::string zeroinit() const override { return ""; }
  virtual bool size_estimate() const { return true; }
};

struct Command : Entity {
  const vks::Command* command;
  std::vector<std::string> aliases;
};

struct Registry {
  const vks::Registry* vreg;

  std::vector<Enumeration*> enumerations;
  std::vector<Bitmask*> bitmasks;
  std::vector<Constant*> constants;
  std::vector<Handle*> handles;
  std::vector<Struct*> structs;
  std::vector<Command*> commands;

  std::unordered_map<const vks::Bitmask*, sps::Bitmask*> bitmask_map;
  std::unordered_map<const vks::Enumeration*, sps::Enumeration*>
      enumeration_map;
  std::unordered_map<const vks::Handle*, sps::Handle*> handle_map;
  std::unordered_map<const vks::Struct*, sps::Struct*> struct_map;
  std::unordered_map<const vks::Enumeration*, const sps::Bitmask*>
      flag_bits_map;
};

}  // namespace sps
