#pragma once

#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vks {

struct Entity {
  std::string name;

  virtual std::string zeroinit() const { return "= 0"; }
  virtual ~Entity() = default;
  virtual bool size_estimate() const { return false; }
  virtual bool is_empty_enum() const { return false; }
};

struct Type {
  virtual std::string to_string() const = 0;
  virtual bool is_empty_enum() const { return false; }
  virtual std::string make_declaration(const std::string& id,
                                       bool zero = true) const {
    return to_string() + " " + id + (zero ? zeroinit() : "");
  }

  virtual std::string zeroinit() const { return "= 0"; }

  virtual bool size_estimate() const { return false; }

  virtual void get_entity_dep(const Entity*& entity, bool& complete) const = 0;
  virtual ~Type() = default;
};

struct Expr {
  virtual std::string to_string() const = 0;
  virtual ~Expr() = default;
};

struct Number : Expr {
  std::string number;
  std::string to_string() const override { return number; };
};

struct Reference : Expr {
  Entity* entity;
  std::string to_string() const override { return entity->name; };
};

struct Name : Type {
  Entity* entity;
  bool is_empty_enum() const override { return entity->is_empty_enum(); }

  std::string zeroinit() const override { return entity->zeroinit(); }

  std::string to_string() const override { return entity->name; };
  void get_entity_dep(const Entity*& entity, bool& complete) const override {
    entity = this->entity;
    complete = true;
  }
  virtual bool size_estimate() const { return entity->size_estimate(); }
};

struct Const : Type {
  const Type* T;
  std::string to_string() const override {
    return T->to_string() + " " + "const";
  };

  virtual bool size_estimate() const { return T->size_estimate(); }

  std::string zeroinit() const override { return T->zeroinit(); }

  void get_entity_dep(const Entity*& entity, bool& complete) const override {
    T->get_entity_dep(entity, complete);
  }
};

struct Pointer : Type {
  const Type* T;
  std::string to_string() const override { return T->to_string() + " " + "*"; };
  void get_entity_dep(const Entity*& entity, bool& complete) const override {
    T->get_entity_dep(entity, complete);
    complete = false;
  }
};

struct Array : Type {
  const Type* T;
  Expr* N;

  std::string to_string() const override {
    return T->to_string() + " [" + N->to_string() + "]";
  };
  std::string make_declaration(const std::string& id,
                               bool zero = true) const override {
    return "std::array<" + T->to_string() + ", " + N->to_string() + "> " + id +
           (zero ? zeroinit() : "");
  }
  void get_entity_dep(const Entity*& entity, bool& complete) const override {
    T->get_entity_dep(entity, complete);
  }
  std::string zeroinit() const override { return "= {}"; }
  virtual bool size_estimate() const { return true; }
};

struct External : Entity {};

struct Platform {
  std::string name;
  std::string protect;
};

struct Constant : Entity {
  std::string value;
  const Platform* platform = nullptr;
};

struct Enumeration : Entity {
  std::string zeroinit() const override { return "= " + name + "(0)"; }
  std::vector<const Constant*> enumerators;
  bool is_empty_enum() const override { return enumerators.empty(); }
};

struct Bitmask : Entity {
  std::string zeroinit() const override { return "= " + name + "(0)"; }
  const Enumeration* requires = nullptr;
  const Platform* platform = nullptr;
  bool is_empty_enum() const override {
    return requires == nullptr || requires->is_empty_enum();
  }
};

struct Handle : Entity {
  bool dispatchable;
  std::set<const Handle*> parents;
};

struct Member {
  std::string name;
  Type* type;
  std::vector<std::string> len;
  std::vector<bool> optional;
};

struct Struct : Entity {
  bool is_union;
  bool returnedonly;
  const Platform* platform = nullptr;
  std::vector<Member> members;
  const Constant* structured_type = nullptr;
  std::vector<const Struct*> structextends;
  virtual bool size_estimate() const { return true; }
};

struct FunctionPrototypeParam {
  std::string name;
  Type* type;
};

struct FunctionPrototype : Entity {
  Type* return_type = nullptr;
  std::vector<FunctionPrototypeParam> params;
  std::string to_type_string() {
    std::ostringstream oss;
    oss << return_type->to_string() << " (*)(";
    for (size_t i = 0; i < params.size(); i++) {
      if (i != 0) oss << ", ";
      oss << params[i].type->to_string();
    }
    oss << ")";
    return oss.str();
  }
};

struct Param {
  std::string name;
  Type* type = nullptr;
};

struct DispatchTable;

struct Command : Entity {
  std::string name;
  Type* return_type = nullptr;
  std::vector<Param> params;
  const Platform* platform = nullptr;
  const DispatchTable* dispatch_table = nullptr;

  std::vector<const Constant*> successcodes;
  std::vector<const Constant*> errorcodes;

  std::string to_type_string() {
    std::ostringstream oss;
    oss << return_type->to_string() << " (*)(";
    for (size_t i = 0; i < params.size(); i++) {
      if (i != 0) oss << ", ";
      oss << params[i].type->to_string();
    }
    oss << ")";
    return oss.str();
  }
};

struct Feature {
  std::vector<Entity*> entities;
};

struct Extension {
  std::vector<Entity*> entities;
};

enum class DispatchTableKind { GLOBAL, INSTANCE, DEVICE };

inline std::string_view to_string(DispatchTableKind kind) {
  switch (kind) {
    case DispatchTableKind::GLOBAL:
      return "global";
    case DispatchTableKind::INSTANCE:
      return "instance";
    case DispatchTableKind::DEVICE:
      return "device";
  };
  return "UNKNOWN";
}

struct DispatchTable {
  DispatchTableKind kind;
  std::vector<const vks::Command*> commands;
};

struct Registry {
  std::unordered_map<std::string, Entity*> entities;

  std::unordered_map<std::string, Platform*> platforms;
  std::unordered_map<std::string, Constant*> constants;
  std::unordered_map<std::string, Enumeration*> enumerations;
  std::unordered_map<std::string, Bitmask*> bitmasks;
  std::unordered_map<std::string, Handle*> handles;
  std::unordered_map<std::string, Struct*> structs;
  std::unordered_map<std::string, FunctionPrototype*> function_prototypes;
  std::unordered_map<std::string, Command*> commands;
  std::unordered_map<std::string, External*> externals;

  DispatchTable*& dispatch_table(DispatchTableKind kind) {
    return dispatch_tables_[size_t(kind)];
  }
  const DispatchTable* dispatch_table(DispatchTableKind kind) const {
    return dispatch_tables_[size_t(kind)];
  }

 private:
  std::array<DispatchTable*, 3> dispatch_tables_;
};

}  // namespace vks
