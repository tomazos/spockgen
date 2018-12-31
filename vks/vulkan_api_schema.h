#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <sstream>

namespace vks {

struct Entity {
  std::string name;

  virtual ~Entity() = default;
};

struct Type {
  virtual std::string to_string() const = 0;
  virtual ~Type() = default;
};

struct Expr {
  virtual std::string to_string() const = 0;
  virtual ~Expr() = default;
};

struct Reference : Expr {
  Entity* entity;
  std::string to_string() const override {
    return entity->name;
  };
};

struct Number : Expr { std::string number;
  std::string to_string() const override {
    return number;
  };
};

struct Name : Type{
  Entity* entity;
  std::string to_string() const override {
    return entity->name;
  };
};

struct Const : Type {
  Type* T;
  std::string to_string() const override {
    return T->to_string() + " " + "const";
  };
};

struct Pointer : Type {
  Type* T;
  std::string to_string() const override {
    return T->to_string() + " " + "*";
  };
};

struct Array : Type {
  Type* T;
  Expr* N;

  std::string to_string() const override {
    return T->to_string() + " [" + N->to_string() + "]";
  };
};

struct External : Entity {
};

struct Platform {
  std::string name;
  std::string protect;
};

struct Constant : Entity {
  std::string value;
  const Platform* platform = nullptr;
};

struct Enumeration : Entity {
  std::vector<const Constant*> enumerators;
};

struct Bitmask : Entity {
  const Enumeration* requires = nullptr;
  const Platform* platform = nullptr;
};

struct Handle : Entity {
  bool dispatchable;
  std::vector<const Handle*> parents;
};

struct Member {
  std::string name;
  Type* type;
};

struct Struct : Entity {
  bool is_union;
  bool returnedonly;
  std::vector<const Struct*> structextends;
  const Platform* platform = nullptr;
  std::vector<Member> members;
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
      if (i != 0)
        oss << ", ";
      oss << params[i].type->to_string();
    }
    oss << ")";
    return oss.str();
  }
};

struct CommandParam {
  std::string name;
  Type* type = nullptr;
};

struct Command : Entity {
  std::string name;
  Type* return_type = nullptr;
  std::vector<CommandParam> params;
  const Platform* platform = nullptr;
  std::string to_type_string() {
    std::ostringstream oss;
    oss << return_type->to_string() << " (*)(";
    for (size_t i = 0; i < params.size(); i++) {
      if (i != 0)
        oss << ", ";
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

  ~Registry() {
    delete_map(platforms);
    delete_map(constants);
    delete_map(enumerations);
    delete_map(bitmasks);
    delete_map(handles);
    delete_map(commands);
  }

 private:
  template<typename Map>
  static void delete_map(Map& m) {
    std::unordered_set<typename Map::mapped_type> s;
    for (auto& kv : m)
      s.insert(kv.second);
    for (auto& v : s)
      delete v;
  }
};

}  // namespace vks
