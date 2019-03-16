#pragma once

#include <optional>

#include "dvc/log.h"
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
  bool is_empty_enum() const override { return enumeration->is_empty_enum(); }
};

struct Bitmask : Entity {
  std::string zeroinit() const override { return "= spk::" + name + "(0)"; }

  const vks::Bitmask* bitmask;
  std::vector<Enumerator> enumerators;
  std::vector<std::string> aliases;
  bool is_empty_enum() const override { return bitmask->is_empty_enum(); }
};

struct Constant : Entity {
  const vks::Constant* constant;
};

struct Command;  // fwd decl

struct Handle;

struct ManualTranslation {
  std::string handle;
  std::string command;
  std::string interface;
  std::string implementation;
};

struct MemberFunction {
  std::string name;
  const Command* command = nullptr;
  const Handle* main_handle = nullptr;
  std::optional<ManualTranslation> manual_translation;
  bool parent_dispatch = false;
  bool result = false;
  bool resultvec_incomplete = false;
  bool resultvec_void = false;
  const Handle* result_handle = nullptr;
  const vks::Type* sz = nullptr;
  const vks::Type* res = nullptr;
  std::set<size_t> szptrs;

  inline size_t begin() const;
  inline size_t end() const;
};

struct Handle : Entity {
  std::string fullname;
  const vks::Handle* handle;
  std::vector<std::string> aliases;
  std::vector<const MemberFunction*> member_functions;
  const Command* destructor = nullptr;
  bool destructor_parent;
  const Handle* parent = nullptr;
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
  bool is_empty_enum() const override { return entity->is_empty_enum(); }
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

struct Param {
  const vks::Param* param;
  std::string name;
  const vks::Type* vtype;
  const vks::Type* stype;
  const vks::Type* asref() const {
    if (stype->to_string() == "void const *") return nullptr;
    if (param->len) return nullptr;
    auto ptr = dynamic_cast<const vks::Pointer*>(stype);
    if (!ptr) return nullptr;
    if (!param->get_optional(0)) return ptr->T;
    return nullptr;
  }
  bool is_allocation_callbacks() const {
    bool result = (name == "pAllocator");
    if (result)
      DVC_ASSERT_EQ(stype->to_string(), "spk::allocation_callbacks const *");
    return result;
  }
};

struct Command : Entity {
  const vks::Command* command;
  const vks::Type* vreturn_type;
  const vks::Type* sreturn_type;
  std::vector<const Enumerator*> successcodes, errorcodes;
  std::vector<Param> params;
  std::vector<std::string> aliases;

  bool resultvec_void(const vks::Type*& sz, const vks::Type*& res) const {
    if (sreturn_type->to_string() != "void") return false;
    auto szptr =
        dynamic_cast<const vks::Pointer*>(params.at(params.size() - 2).stype);
    DVC_ASSERT(szptr);
    DVC_ASSERT(szptr->T->to_string() == "uint32_t" ||
               szptr->T->to_string() == "size_t");
    auto resptr =
        dynamic_cast<const vks::Pointer*>(params.at(params.size() - 1).stype);
    DVC_ASSERT(resptr);

    if (resptr->T->to_string() == "void") return false;

    sz = szptr->T;
    res = resptr->T;

    return true;
  }
  bool resultvec_incomplete(const vks::Type*& sz, const vks::Type*& res) const {
    bool has_incomplete = false;
    for (const Enumerator* successcode : successcodes)
      if (successcode->name == "incomplete") has_incomplete = true;
    if (!has_incomplete) return false;
    DVC_ASSERT_EQ(successcodes.size(), 2);
    auto szptr =
        dynamic_cast<const vks::Pointer*>(params.at(params.size() - 2).stype);
    DVC_ASSERT(szptr);
    DVC_ASSERT(szptr->T->to_string() == "uint32_t" ||
               szptr->T->to_string() == "size_t");
    auto resptr =
        dynamic_cast<const vks::Pointer*>(params.at(params.size() - 1).stype);
    DVC_ASSERT(resptr);

    if (resptr->T->to_string() == "void") return false;

    sz = szptr->T;
    res = resptr->T;

    return true;
  }
};

inline size_t MemberFunction::begin() const { return parent_dispatch ? 2 : 1; }
inline size_t MemberFunction::end() const {
  if (result) return command->params.size() - 1;
  if (resultvec_incomplete || resultvec_void) return command->params.size() - 2;
  return command->params.size();
}

struct DispatchTable {
  const vks::DispatchTable* dispatch_table;
  std::vector<const Command*> commands;
};

struct Registry {
  const vks::Registry* vreg;

  std::vector<Enumeration*> enumerations;
  std::vector<Bitmask*> bitmasks;
  std::vector<Constant*> constants;
  std::vector<Handle*> handles;
  std::vector<Struct*> structs;
  std::vector<Command*> commands;
  static std::vector<ManualTranslation> manual_translations;

  std::unordered_map<const vks::Bitmask*, sps::Bitmask*> bitmask_map;
  std::unordered_map<const vks::Enumeration*, sps::Enumeration*>
      enumeration_map;
  std::unordered_map<const vks::Handle*, sps::Handle*> handle_map;
  std::unordered_map<const vks::Struct*, sps::Struct*> struct_map;
  std::unordered_map<const vks::Enumeration*, const sps::Bitmask*>
      flag_bits_map;
  std::unordered_map<const vks::Command*, std::vector<const sps::Command*>>
      command_map;
  std::unordered_map<const vks::Constant*, const sps::Enumerator*> codemap;

  DispatchTable*& dispatch_table(vks::DispatchTableKind kind) {
    return dispatch_tables_[size_t(kind)];
  }
  const DispatchTable* dispatch_table(vks::DispatchTableKind kind) const {
    return dispatch_tables_[size_t(kind)];
  }

 private:
  std::array<DispatchTable*, 3> dispatch_tables_;
};

inline const vks::Type* get_pointee(const vks::Type* t) {
  auto p = dynamic_cast<const vks::Pointer*>(t);
  if (!p) return nullptr;
  return p->T;
};

}  // namespace sps
