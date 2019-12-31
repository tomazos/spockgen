#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include <experimental/filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string_view>
#include <unordered_map>

#include "dvc/file.h"
#include "dvc/json.h"
#include "dvc/opts.h"
#include "dvc/parser.h"
#include "dvc/scanner.h"

struct Token {
  enum Kind {
    END,
    IDENTIFIER,
    STRING,
    ELEMENT,
    ATTRIBUTE,
    NAMESPACE,
    MIXED,
    LBRACE,
    RBRACE,
    QMARK,
    ASTERISK,
    VBAR,
    LPAREN,
    RPAREN,
    COMMA,
    EQUALS,
    AMPERSAND
  };

  Token(Kind kind, size_t line) : kind(kind), line(line){};
  Token(Kind kind, const std::string& spelling, size_t line)
      : kind(kind), spelling(spelling), line(line) {}

  Kind kind;
  std::string spelling;
  size_t line;
};

bool operator==(const Token& token, Token::Kind kind) {
  return token.kind == kind;
}
bool operator!=(const Token& a, Token::Kind b) { return !(a == b); }
bool operator==(Token::Kind b, const Token& a) { return a == b; }
bool operator!=(Token::Kind b, const Token& a) { return !(a == b); }

std::string_view token_kind_to_string(Token::Kind kind) {
  switch (kind) {
    case Token::END:
      return "END";
    case Token::IDENTIFIER:
      return "IDENTIFIER";
    case Token::STRING:
      return "STRING";
    case Token::ELEMENT:
      return "ELEMENT";
    case Token::ATTRIBUTE:
      return "ATTRIBUTE";
    case Token::NAMESPACE:
      return "NAMESPACE";
    case Token::MIXED:
      return "MIXED";
    case Token::LBRACE:
      return "LBRACE";
    case Token::RBRACE:
      return "RBRACE";
    case Token::QMARK:
      return "QMARK";
    case Token::ASTERISK:
      return "ASTERISK";
    case Token::VBAR:
      return "VBAR";
    case Token::LPAREN:
      return "LPAREN";
    case Token::RPAREN:
      return "RPAREN";
    case Token::COMMA:
      return "COMMA";
    case Token::EQUALS:
      return "EQUALS";
    case Token::AMPERSAND:
      return "AMPERSAND";
  }
  DVC_FATAL("unknown token::kind");
}

std::ostream& operator<<(std::ostream& o, const Token& tok) {
  return o << token_kind_to_string(tok.kind) << ':' << tok.spelling << ':'
           << tok.line;
}

struct SchemaScanner : dvc::scanner {
  using dvc::scanner::scanner;

  std::string parse_string() {
    incr();
    std::ostringstream oss;
    while (true) {
      char c = pop();
      DVC_ASSERT(peek() != dvc::scanner::eof);
      if (c == '"') return oss.str();
      oss.write(&c, 1);
    }
  }

  std::string parse_identifier() {
    std::ostringstream oss;
  again:
    char c = peek();
    if (std::isalnum(c) || c == '_' || c == ':') {
      incr();
      oss.write(&c, 1);
      goto again;
    }
    return oss.str();
  }

  void skip_whitespace() {
    while (true) {
      if (std::isspace(peek())) {
        incr();
        continue;
      }

      if (peek() == '#') {
        do {
          incr();
        } while (peek() != '\n' && peek() != dvc::scanner::eof);
        continue;
      }
      return;
    }
  }

  Token parse_next_token() {
    static const std::unordered_map<char, Token::Kind> punctuation = {
        {'{', Token::LBRACE},   {'}', Token::RBRACE}, {'?', Token::QMARK},
        {'*', Token::ASTERISK}, {'|', Token::VBAR},   {'(', Token::LPAREN},
        {')', Token::RPAREN},   {',', Token::COMMA},  {'=', Token::EQUALS},
        {'&', Token::AMPERSAND}};
    static const std::unordered_map<std::string, Token::Kind> keywords = {
        {"element", Token::ELEMENT},
        {"attribute", Token::ATTRIBUTE},
        {"namespace", Token::NAMESPACE},
        {"mixed", Token::MIXED}};

    skip_whitespace();

    size_t l = line();
    char c = peek();

    if (c == dvc::scanner::eof) return {Token::END, l};

    // punctuation
    auto it = punctuation.find(c);
    if (it != punctuation.end()) {
      incr();
      return {it->second, l};
    }

    if (c == '"') return {Token::STRING, parse_string(), l};

    if (std::isalpha(c) || c == '_') {
      std::string identifier = parse_identifier();
      auto it = keywords.find(identifier);
      if (it != keywords.end()) {
        return {it->second, identifier, l};
      } else {
        return {Token::IDENTIFIER, identifier, l};
      }
    }

    DVC_FATAL("Unexpected character: ", c, " at line ", l);
  }
};

namespace ast {

struct Element;

enum class MemberDisposition { NONE, OPTIONAL, REQUIRED, MULTIPLE };

struct MemberDispositions {
  std::map<std::string, MemberDisposition> attributes;
  std::map<std::string, MemberDisposition> elements;
  std::map<std::string, const Element*> pelements;

  void visit(std::function<void(MemberDisposition&)> f) {
    for (auto& [k, v] : attributes) {
      (void)k;
      f(v);
    }
    for (auto& [k, v] : elements) {
      (void)k;
      f(v);
    }
  }
};

struct BinaryDispositionTable {
  BinaryDispositionTable(
      std::vector<
          std::tuple<MemberDisposition, MemberDisposition, MemberDisposition>>
          data) {
    for (auto [left, right, result] : data) table[left][right] = result;

    for (auto left : {MemberDisposition::NONE, MemberDisposition::OPTIONAL,
                      MemberDisposition::REQUIRED, MemberDisposition::MULTIPLE})
      for (auto right :
           {MemberDisposition::NONE, MemberDisposition::OPTIONAL,
            MemberDisposition::REQUIRED, MemberDisposition::MULTIPLE}) {
        DVC_ASSERT(table.count(left) == 1);
        DVC_ASSERT(table.at(left).count(right) == 1);
      }
  }

  MemberDisposition operator()(MemberDisposition left,
                               MemberDisposition right) const {
    return table.at(left).at(right);
  }

  std::map<MemberDisposition, std::map<MemberDisposition, MemberDisposition>>
      table;
};

struct Schema;

struct Pattern {
  virtual ~Pattern() = default;
  virtual void to_json(dvc::json_writer& w) const = 0;
  virtual void visit(std::function<bool(const Pattern& pattern)> f) const {
    f(*this);
  }

  virtual std::vector<const Pattern*> children() const { return {}; };
  virtual MemberDispositions get_member_dispositions(
      const Schema& schema) const = 0;

  virtual bool is_text(const Schema& schema) const { return false; }
};

using PPattern = std::shared_ptr<Pattern>;

struct Name : Pattern {
  Name(const std::string& name) : name(name) {}
  std::string name;
  void to_json(dvc::json_writer& w) const override { w.write_string(name); }

  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;

  bool is_text(const Schema& schema) const override;
};

struct UnaryPattern : Pattern {
  UnaryPattern(PPattern child) : child(std::move(child)) {}
  PPattern child;
  void to_json(dvc::json_writer& w) const override {
    w.start_array();
    w.write_string(typeid(*this).name());
    child->to_json(w);
    w.end_array();
  }
  void visit(std::function<bool(const Pattern& pattern)> f) const override {
    if (f(*this)) child->visit(f);
  }
  std::vector<const Pattern*> children() const override {
    std::vector<const Pattern*> result;
    result.push_back(child.get());
    return result;
  }
};

struct BinaryPattern : Pattern {
  BinaryPattern(PPattern left, PPattern right)
      : left(std::move(left)), right(std::move(right)) {}
  PPattern left, right;
  void to_json(dvc::json_writer& w) const override {
    w.start_array();
    w.write_string(typeid(*this).name());
    left->to_json(w);
    right->to_json(w);
    w.end_array();
  }
  void visit(std::function<bool(const Pattern& pattern)> f) const override {
    if (f(*this)) {
      left->visit(f);
      right->visit(f);
    }
  }

  std::vector<const Pattern*> children() const override {
    std::vector<const Pattern*> result;
    result.push_back(left.get());
    result.push_back(right.get());
    return result;
  }

 protected:
  MemberDispositions get_binary_member_dispositions(
      const BinaryDispositionTable& table, const Schema& schema) const;
};

struct BracedPattern : Pattern {
  BracedPattern(const std::string& name, PPattern pattern)
      : name(name), pattern(std::move(pattern)) {}
  std::string name;
  PPattern pattern;
  void to_json(dvc::json_writer& w) const override {
    w.start_array();
    w.write_string(typeid(*this).name());
    w.write_string(name);
    pattern->to_json(w);
    w.end_array();
  }
  void visit(std::function<bool(const Pattern& pattern)> f) const override {
    if (f(*this)) pattern->visit(f);
  }
};

struct ZeroOrOne : UnaryPattern {
  using UnaryPattern::UnaryPattern;
  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;
};

struct ZeroOrMore : UnaryPattern {
  using UnaryPattern::UnaryPattern;
  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;
};

struct Mixed : UnaryPattern {
  using UnaryPattern::UnaryPattern;
  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;
};

struct Alternate : BinaryPattern {
  using BinaryPattern::BinaryPattern;
  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;
};

struct Sequence : BinaryPattern {
  using BinaryPattern::BinaryPattern;
  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;
};

struct Interleave : BinaryPattern {
  using BinaryPattern::BinaryPattern;
  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;
};

struct Attribute : BracedPattern {
  using BracedPattern::BracedPattern;
  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;
};

struct Element : BracedPattern {
  using BracedPattern::BracedPattern;

  MemberDispositions get_member_dispositions(
      const Schema& schema) const override;

  MemberDispositions get_element_dispositions(const Schema& schema) const {
    return pattern->get_member_dispositions(schema);
  }

  bool is_simple(const Schema& schema) const {
    return pattern->is_text(schema);
  }
};

struct Schema {
  std::map<std::string, PPattern> productions;

  void to_json(dvc::json_writer& w) const {
    w.start_object();
    for (const auto& [name, pattern] : productions) {
      w.write_key(name);
      pattern->to_json(w);
    }
    w.end_object();
  }

  void visit(std::function<bool(const Pattern&)> f) {
    for (const auto& [name, pattern] : productions) {
      (void)name;
      pattern->visit(f);
    }
  }
};

bool Name::is_text(const Schema& schema) const {
  if (name == "text" || name == "xsd:float")
    return true;
  else
    return schema.productions.at(name)->is_text(schema);
}

MemberDispositions Name::get_member_dispositions(const Schema& schema) const {
  MemberDispositions dispositions;

  if (name == "text" || name == "xsd:float") return dispositions;
  const Pattern* pattern = schema.productions.at(name).get();
  if (auto element = dynamic_cast<const Element*>(pattern)) {
    dispositions.elements[element->name] = MemberDisposition::REQUIRED;
    dispositions.pelements[element->name] = element;
    return dispositions;
  } else {
    return pattern->get_member_dispositions(schema);
  }
}

MemberDispositions ZeroOrOne::get_member_dispositions(
    const Schema& schema) const {
  MemberDispositions dispositions = child->get_member_dispositions(schema);
  dispositions.visit([](MemberDisposition& dis) {
    if (dis == MemberDisposition::REQUIRED) dis = MemberDisposition::OPTIONAL;
  });
  return dispositions;
}

MemberDispositions ZeroOrMore::get_member_dispositions(
    const Schema& schema) const {
  MemberDispositions dispositions = child->get_member_dispositions(schema);
  dispositions.visit(
      [](MemberDisposition& dis) { dis = MemberDisposition::MULTIPLE; });
  return dispositions;
}

MemberDispositions Mixed::get_member_dispositions(const Schema& schema) const {
  MemberDispositions dispositions = child->get_member_dispositions(schema);
  //  dispositions.mixed = true;
  return dispositions;
}

MemberDispositions BinaryPattern::get_binary_member_dispositions(
    const BinaryDispositionTable& table, const Schema& schema) const {
  MemberDispositions l = left->get_member_dispositions(schema);
  MemberDispositions r = right->get_member_dispositions(schema);

  auto v = [](const std::map<std::string, MemberDisposition>& a,
              const std::string& k) {
    auto it = a.find(k);
    if (it == a.end())
      return MemberDisposition::NONE;
    else
      return it->second;
  };

  auto m = [&](const std::map<std::string, MemberDisposition>& a,
               const std::map<std::string, MemberDisposition>& b) {
    std::map<std::string, MemberDisposition> result;
    std::set<std::string> keys;
    for (const auto& [k, v] : a) {
      (void)v;
      keys.insert(k);
    }
    for (const auto& [k, v] : b) {
      (void)v;
      keys.insert(k);
    }
    for (const auto& k : keys) {
      result[k] = table(v(a, k), v(b, k));
    }
    return result;
  };

  auto mp = [&](const std::map<std::string, const ast::Element*>& a,
                const std::map<std::string, const ast::Element*>& b) {
    std::map<std::string, const ast::Element*> result;
    std::set<std::string> keys;
    for (const auto& [k, v] : a) {
      (void)v;
      keys.insert(k);
    }
    for (const auto& [k, v] : b) {
      (void)v;
      keys.insert(k);
    }
    for (const auto& k : keys) {
      if (a.count(k) && b.count(k))
        if (a.at(k) != b.at(k))
          DVC_ASSERT(a.at(k)->is_simple(schema) && b.at(k)->is_simple(schema),
                     "nonsimple nonequal subelements ", k);

      result[k] = (a.count(k) ? a.at(k) : b.at(k));
    }
    return result;
  };

  MemberDispositions dispositions;
  dispositions.attributes = m(l.attributes, r.attributes);
  dispositions.elements = m(l.elements, r.elements);
  dispositions.pelements = mp(l.pelements, r.pelements);

  return dispositions;
}

BinaryDispositionTable additive_binary_member_disposition_table({
    {MemberDisposition::NONE, MemberDisposition::NONE, MemberDisposition::NONE},
    {MemberDisposition::NONE, MemberDisposition::OPTIONAL,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::NONE, MemberDisposition::REQUIRED,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::NONE, MemberDisposition::MULTIPLE,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::OPTIONAL, MemberDisposition::NONE,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::OPTIONAL, MemberDisposition::OPTIONAL,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::OPTIONAL, MemberDisposition::REQUIRED,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::OPTIONAL, MemberDisposition::MULTIPLE,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::REQUIRED, MemberDisposition::NONE,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::REQUIRED, MemberDisposition::OPTIONAL,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::REQUIRED, MemberDisposition::REQUIRED,
     MemberDisposition::REQUIRED},
    {MemberDisposition::REQUIRED, MemberDisposition::MULTIPLE,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::MULTIPLE, MemberDisposition::NONE,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::MULTIPLE, MemberDisposition::OPTIONAL,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::MULTIPLE, MemberDisposition::REQUIRED,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::MULTIPLE, MemberDisposition::MULTIPLE,
     MemberDisposition::MULTIPLE},
});

BinaryDispositionTable multiplicative_binary_member_disposition_table({
    {MemberDisposition::NONE, MemberDisposition::NONE, MemberDisposition::NONE},
    {MemberDisposition::NONE, MemberDisposition::OPTIONAL,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::NONE, MemberDisposition::REQUIRED,
     MemberDisposition::REQUIRED},
    {MemberDisposition::NONE, MemberDisposition::MULTIPLE,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::OPTIONAL, MemberDisposition::NONE,
     MemberDisposition::OPTIONAL},
    {MemberDisposition::OPTIONAL, MemberDisposition::OPTIONAL,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::OPTIONAL, MemberDisposition::REQUIRED,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::OPTIONAL, MemberDisposition::MULTIPLE,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::REQUIRED, MemberDisposition::NONE,
     MemberDisposition::REQUIRED},
    {MemberDisposition::REQUIRED, MemberDisposition::OPTIONAL,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::REQUIRED, MemberDisposition::REQUIRED,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::REQUIRED, MemberDisposition::MULTIPLE,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::MULTIPLE, MemberDisposition::NONE,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::MULTIPLE, MemberDisposition::OPTIONAL,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::MULTIPLE, MemberDisposition::REQUIRED,
     MemberDisposition::MULTIPLE},
    {MemberDisposition::MULTIPLE, MemberDisposition::MULTIPLE,
     MemberDisposition::MULTIPLE},
});

MemberDispositions Alternate::get_member_dispositions(
    const Schema& schema) const {
  return get_binary_member_dispositions(
      additive_binary_member_disposition_table, schema);
}

MemberDispositions Sequence::get_member_dispositions(
    const Schema& schema) const {
  return get_binary_member_dispositions(
      multiplicative_binary_member_disposition_table, schema);
}

MemberDispositions Interleave::get_member_dispositions(
    const Schema& schema) const {
  return get_binary_member_dispositions(
      multiplicative_binary_member_disposition_table, schema);
}

MemberDispositions Attribute::get_member_dispositions(
    const Schema& schema) const {
  MemberDispositions dispositions;
  dispositions.attributes[name] = MemberDisposition::REQUIRED;
  return dispositions;
}

MemberDispositions Element::get_member_dispositions(
    const Schema& schema) const {
  MemberDispositions dispositions;
  dispositions.elements[name] = MemberDisposition::REQUIRED;
  dispositions.pelements[name] = this;
  return dispositions;
}

}  // namespace ast

// comment:
//   # ... \n
//
// namespace_decl:
//    namespace ID = STRING
//
// pattern:
//    ID = expr
//
// expr:
//    element ID { expr }
//    attribute ID { expr }
//    ID
//    expr ?
//    expr *
//    ( expr )
//    expr | expr
//    expr , expr
//    expr & expr

class SchemaParser : public dvc::parser<Token> {
 public:
  using dvc::parser<Token>::parser;
  ast::Schema parse_schema() {
    ast::Schema schema;

    while (peek() == Token::NAMESPACE) parse_namespace_decl();

    while (peek() != Token::END)
      DVC_ASSERT(schema.productions.insert(parse_production()).second);

    return schema;
  }

  void parse_namespace_decl() {
    DVC_ASSERT_EQ(pop(), Token::NAMESPACE);
    DVC_ASSERT_EQ(pop(), Token::IDENTIFIER);
    DVC_ASSERT_EQ(pop(), Token::EQUALS);
    DVC_ASSERT_EQ(pop(), Token::STRING);
  }

  std::pair<std::string, ast::PPattern> parse_production() {
    Token key = pop();
    DVC_ASSERT(key == Token::IDENTIFIER, "unexpected token: ", key);
    DVC_ASSERT_EQ(pop(), Token::EQUALS);
    return {std::move(key.spelling), parse_pattern()};
  }

  ast::PPattern parse_pattern() {
    ast::PPattern pattern;

    if (peek() == Token::ELEMENT) {
      pattern = parse_element();
    } else if (peek() == Token::ATTRIBUTE) {
      pattern = parse_attribute();
    } else if (peek() == Token::MIXED) {
      pattern = parse_mixed();
    } else if (peek() == Token::LPAREN) {
      pop();
      pattern = parse_pattern();
      DVC_ASSERT_EQ(pop(), Token::RPAREN);
    } else if (peek() == Token::IDENTIFIER) {
      pattern = parse_name();
    } else {
      DVC_FATAL("expected element, attribute, identifier, paren: ", peek());
    }

    if (peek() == Token::QMARK) {
      incr();
      pattern = std::make_shared<ast::ZeroOrOne>(std::move(pattern));
    } else if (peek() == Token::ASTERISK) {
      incr();
      pattern = std::make_shared<ast::ZeroOrMore>(std::move(pattern));
    }

    if (peek() == Token::VBAR) {
      incr();
      pattern =
          std::make_shared<ast::Alternate>(std::move(pattern), parse_pattern());
    } else if (peek() == Token::COMMA) {
      incr();
      pattern =
          std::make_shared<ast::Sequence>(std::move(pattern), parse_pattern());
    } else if (peek() == Token::AMPERSAND) {
      incr();
      pattern = std::make_shared<ast::Interleave>(std::move(pattern),
                                                  parse_pattern());
    }

    return pattern;
  }

  template <typename BracedPattern>
  ast::PPattern parse_braced_pattern(Token::Kind kind) {
    DVC_ASSERT_EQ(pop(), kind);
    Token id = pop();
    DVC_ASSERT_EQ(id, Token::IDENTIFIER);
    DVC_ASSERT_EQ(pop(), Token::LBRACE);
    ast::PPattern pattern = parse_pattern();
    DVC_ASSERT_EQ(pop(), Token::RBRACE);
    return std::make_shared<BracedPattern>(id.spelling, std::move(pattern));
  }

  ast::PPattern parse_mixed() {
    DVC_ASSERT_EQ(pop(), Token::MIXED);
    DVC_ASSERT_EQ(pop(), Token::LBRACE);
    ast::PPattern pattern = parse_pattern();
    DVC_ASSERT_EQ(pop(), Token::RBRACE);
    return std::make_shared<ast::Mixed>(std::move(pattern));
  }

  ast::PPattern parse_element() {
    return parse_braced_pattern<ast::Element>(Token::ELEMENT);
  }

  ast::PPattern parse_attribute() {
    return parse_braced_pattern<ast::Attribute>(Token::ATTRIBUTE);
  }

  ast::PPattern parse_name() {
    Token id = pop();
    DVC_ASSERT_EQ(id, Token::IDENTIFIER);
    return std::make_shared<ast::Name>(id.spelling);
  }
};

ast::Schema parse_schema(const std::filesystem::path& schema_path) {
  SchemaScanner scanner(schema_path.filename().string(),
                        dvc::load_file(schema_path));
  std::vector<Token> tokens;
  Token token = scanner.parse_next_token();
  while (true) {
    tokens.push_back(token);
    if (token == Token::END) break;
    token = scanner.parse_next_token();
  }

  //  for (size_t i = 0; i < tokens.size(); i++)
  //    LOG(INFO) << "token " << i << " = " << tokens.at(i);
  SchemaParser parser(schema_path.filename().string(), std::move(tokens));
  return parser.parse_schema();
}

std::string DVC_OPTION(namespace_, -, "relaxnggen",
                       "namespace to put generated code in");
std::string DVC_OPTION(protocol, -, "", "protocol name");

void generate_relaxng_parser(const std::filesystem::path& schema_file,
                             const std::filesystem::path& hout) {
  ast::Schema schema = parse_schema(schema_file);
  dvc::file_writer w(hout, dvc::truncate);

  std::map<const ast::Pattern*, std::string> global_pattern_names;
  std::map<std::string, const ast::Pattern*> global_name_patterns;

  for (const auto& [name, pattern] : schema.productions) {
    DVC_ASSERT(global_name_patterns.count(name) == 0);
    global_pattern_names[pattern.get()] = name;
    global_name_patterns[name] = pattern.get();
  }

  std::set<const ast::Element*> simple_elements;

  std::map<const ast::Element*, std::string> element_type_names;
  std::map<std::string, const ast::Element*> element_name_types;

  std::string last_element;
  schema.visit([&](const ast::Pattern& pattern) -> bool {
    if (global_pattern_names.count(&pattern)) {
      last_element = global_pattern_names.at(&pattern);
    }

    //    if (dynamic_cast<const ast::Mixed*>(&pattern)) return false;
    if (auto element = dynamic_cast<const ast::Element*>(&pattern)) {
      bool simple_element = false;
      std::string type_name;
      if (global_pattern_names.count(&pattern)) {
        type_name = global_pattern_names.at(&pattern);
        last_element = type_name;
      } else
        type_name = last_element + "_" + element->name;
      if (auto name = dynamic_cast<const ast::Name*>(element->pattern.get()))
        if (name->name == "text" || name->name == "xsd:float")
          simple_element = true;
      if (simple_element)
        simple_elements.insert(element);
      else {
        if (element_name_types.count(type_name)) type_name = type_name + "2";
        DVC_ASSERT(element_type_names.count(element) == 0,
                   "duplicate element type name: ", type_name);
        DVC_ASSERT(element_name_types.count(type_name) == 0,
                   "duplicate element type name: ", type_name);
        element_type_names[element] = type_name;
        element_name_types[type_name] = element;
      }
    }
    return true;
  });

  auto apply_disposition =
      [](const std::string& type,
         ast::MemberDisposition disposition) -> std::string {
    switch (disposition) {
      case ast::MemberDisposition::OPTIONAL:
        return "std::optional<" + type + ">";
      case ast::MemberDisposition::REQUIRED:
        return type;
      case ast::MemberDisposition::MULTIPLE:
        return "std::vector<" + type + ">";
      case ast::MemberDisposition::NONE:
        DVC_FATAL("none disposition: ", (int)disposition);
    }
    DVC_FATAL("none disposition: ", (int)disposition);
  };

  struct StructDesign {
    std::string name;

    struct Member {
      enum Kind { ATTRIBUTE, SUBELEMENT };
      std::string type;
      std::string output_name;
      Kind kind;
      std::string input_name;
    };

    std::vector<Member> members;

    std::set<std::string> dependencies;
  };

  std::map<std::string, StructDesign> struct_designs_map;

  for (const auto& [element_type_name, element] : element_name_types) {
    StructDesign design;
    design.name = element_type_name;

    ast::MemberDispositions md = element->get_element_dispositions(schema);
    for (const auto& [attribute, disposition] : md.attributes) {
      std::string name = attribute;
      if (md.elements.count(name)) name += "_attribute";
      if (name == "requires") name = "requires_";
      std::string type = "std::string";
      StructDesign::Member member;
      member.type = apply_disposition(type, disposition);
      member.output_name = name;
      member.input_name = attribute;
      member.kind = StructDesign::Member::Kind::ATTRIBUTE;
      design.members.push_back(member);
    }
    for (const auto& [subelement_name, disposition] : md.elements) {
      const ast::Element* subelement = md.pelements.at(subelement_name);
      std::string name = subelement_name;
      if (md.attributes.count(name)) name += "_subelement";
      if (name == "enum") name = "enum_";

      std::string type;
      if (subelement->is_simple(schema))
        type = "std::string";
      else {
        type = element_type_names.at(subelement);
        design.dependencies.insert(type);
      }
      StructDesign::Member member;
      member.type = apply_disposition(type, disposition);
      member.output_name = name;
      member.input_name = subelement_name;
      member.kind = StructDesign::Member::Kind::SUBELEMENT;
      design.members.push_back(member);
    }
    struct_designs_map[design.name] = design;
  }

  std::set<std::string> todo;

  for (const auto& [name, struct_design] : struct_designs_map) {
    (void)struct_design;
    todo.insert(name);
  }

  std::vector<StructDesign> struct_designs_depord;

outer_loop:
  while (!todo.empty()) {
    for (const std::string& name : todo) {
      const StructDesign& struct_design = struct_designs_map.at(name);
      bool depsdone = true;
      for (const std::string& dependency : struct_design.dependencies)
        if (todo.count(dependency)) {
          depsdone = false;
          break;
        }
      if (depsdone) {
        struct_designs_depord.push_back(struct_design);
        todo.erase(name);  // ATTN: Modifying current loop
        goto outer_loop;
      }
    }
  }

  w.println("// autogenerated from ", schema_file);
  w.println();
  w.println("#pragma once");
  w.println();
  w.println("#include <optional>");
  w.println("#include <string>");
  w.println("#include <vector>");
  w.println();
  w.println("#include \"relaxng/relaxng.h\"");
  w.println();
  w.println("namespace ", namespace_, " {");
  w.println();
  w.println("struct ", protocol, "{};");
  w.println();

  for (const StructDesign& struct_design : struct_designs_depord) {
    w.println("struct ", struct_design.name, " : ::relaxng::GeneratedClass {");

    for (const StructDesign::Member& member : struct_design.members) {
      w.println("  ", member.type, " ", member.output_name, ";");
    }
    w.println("};");
    w.println();
  }
  w.println();
  w.println("}  // namespace ", namespace_);
  w.println();
  w.println("namespace relaxng {");
  w.println();

  std::string protocol_qname = "::" + namespace_ + "::" + protocol;

  w.println("template<>");
  w.println("struct ProtocolReflection<", protocol_qname, "> {");
  w.println("  static constexpr size_t num_classes = ",
            struct_designs_depord.size(), ";");
  w.println("};");
  w.println();

  for (size_t class_index = 0; class_index < struct_designs_depord.size();
       class_index++) {
    const StructDesign& struct_design = struct_designs_depord.at(class_index);
    std::string class_qname = "::" + namespace_ + "::" + struct_design.name;

    w.println("template<>");
    w.println("struct ProtocolClassReflection<", protocol_qname, ", ",
              class_index, "> {");
    w.println("  using protocol = ", protocol_qname, ";");
    w.println("  static constexpr size_t class_index = ", class_index, ";");
    w.println("  using type = ", class_qname, ";");
    w.println("  static constexpr size_t num_members = ",
              struct_design.members.size(), ";");
    w.println("};");
    w.println();

    w.println("template<>");
    w.println("struct ClassReflection<", class_qname, "> {");
    w.println("  using protocol = ", protocol_qname, ";");
    w.println("  static constexpr size_t class_index = ", class_index, ";");
    w.println("  using type = ", class_qname, ";");
    w.println("  static constexpr size_t num_members = ",
              struct_design.members.size(), ";");
    w.println("  static constexpr bool present = true;");
    w.println("};");
    w.println();
    for (size_t member_index = 0; member_index < struct_design.members.size();
         member_index++) {
      const StructDesign::Member& member =
          struct_design.members.at(member_index);

      w.println("template<>");
      w.println("struct ClassMemberReflection<", class_qname, ", ",
                member_index, "> {");
      w.println("  static constexpr size_t member_index = ", member_index, ";");
      w.println("  static constexpr auto member_ptr = &", class_qname,
                "::", member.output_name, ";");
      w.println("  static constexpr std::string_view output_name = \"",
                member.output_name, "\";");
      w.println("  static constexpr std::string_view input_name = \"",
                member.input_name, "\";");
      w.println("  static constexpr MemberKind member_kind = ",
                (member.kind == StructDesign::Member::ATTRIBUTE
                     ? "MemberKind::ATTRIBUTE"
                     : "MemberKind::SUBELEMENT"),
                ";");
      w.println("};");
    }
    w.println();
  }

  w.println("}  // namespace relaxng");

  //  w.println("// begin fwd decls");
  //  for (const auto& [element_type_name, element] : element_name_types) {
  //    (void)element;
  //    w.println("struct ", element_type_name, ";");
  //  }
  //
  //  w.println("// end fwd decls");
  //  w.println();
}

std::filesystem::path DVC_OPTION(schema, -, dvc::required,
                                 "The input compact relaxng schema file");
std::filesystem::path DVC_OPTION(hout, -, dvc::required,
                                 "The output generated C++ .h file");

int main(int argc, char** argv) {
  dvc::init_options(argc, argv);
  if (!exists(schema)) DVC_FATAL("File not found: ", schema);
  generate_relaxng_parser(schema, hout);
}
