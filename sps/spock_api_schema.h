#pragma once

#include "vulkanhpp/vulkan_api_schema.h"

namespace sps {

struct Entity {
  std::string name;
};

struct Enumerator : Entity {
  const vks::Constant* constant;
};

struct Enumeration : Entity {
  const vks::Enumeration* enumeration;
  std::vector<Enumerator> enumerators;
  std::vector<std::string> aliases;
};

struct Bitmask : Entity {
  const vks::Bitmask* bitmask;
  std::vector<Enumerator> enumerators;
  std::vector<std::string> aliases;
};

struct Constant : Entity {
  const vks::Constant* constant;
};

struct Command; // fwd decl

struct Handle : Entity {
  const vks::Handle* handle;
  std::vector<std::string> aliases;
  std::vector<Command*> commands;
};

struct Struct : Entity {
  const vks::Struct* struct_;
  std::vector<std::string> aliases;
};

struct Command : Entity {
  const vks::Command* command;
  std::vector<std::string> aliases;
};

struct Registry {
  std::vector<Enumeration*> enumerations;
  std::vector<Bitmask*> bitmasks;
  std::vector<Constant*> constants;
  std::vector<Handle*> handles;
  std::vector<Struct*> structs;
  std::vector<Command*> commands;
};

}  // namespace sps
