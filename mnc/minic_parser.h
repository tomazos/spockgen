#pragma once

#include <string>
#include <vector>

namespace mnc {

struct Expr { virtual ~Expr() = default; };

struct Reference : Expr {
  Reference(const std::string& name) : name(name) {}
  std::string name;
};

struct Number : Expr {
  Number(const std::string& number) : number(number) {}
  std::string number;
};

struct Type { virtual ~Type() = default; };

struct Name : Type {
  Name(const std::string& name) : name(name) {}
  std::string name;
};

struct Const : Type {
  Const(Type* T) : T(T) {}
  Type* T;
};

struct Pointer : Type {
  Pointer(Type* T) : T(T) {}
  Type* T;
};

struct Array : Type {
  Array(Type* T, Expr* N) : T(T), N(N) {}
  Type* T;
  Expr* N;
};

struct Declaration {
  std::string name;
  Type* type;
};

struct FunctionPrototype {
  std::string name;
  Type* return_type;
  std::vector<Declaration> params;
};

Declaration parse_declaration(const std::string&);
FunctionPrototype parse_function_prototype(const std::string&);

}  // namespace mnc
