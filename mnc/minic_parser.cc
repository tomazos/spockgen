#include "minic_parser.h"

#include <unordered_map>

#include "dvc/parser.h"
#include "dvc/scanner.h"

namespace mnc {

struct Token {
  enum Kind {
    END,
    IDENTIFIER,
    NUMBER,

    // brackets
    LBRACK,
    RBRACK,
    LPAREN,
    RPAREN,

    // keywords
    CONST,
    STRUCT,

    // punct
    ASTERISK,
    COMMA,
    SEMICOLON,
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
    case Token::LBRACK:
      return "LBRACK";
    case Token::RBRACK:
      return "RBRACK";
    case Token::LPAREN:
      return "LPAREN";
    case Token::RPAREN:
      return "RPAREN";
    case Token::CONST:
      return "CONST";
    case Token::STRUCT:
      return "STRUCT";
    case Token::ASTERISK:
      return "ASTERISK";
    case Token::COMMA:
      return "COMMA";
    case Token::IDENTIFIER:
      return "IDENTIFIER";
    case Token::NUMBER:
      return "NUMBER";
    case Token::SEMICOLON:
      return "SEMICOLON";
  }

  DVC_FATAL("unknown token::kind");
}

std::ostream& operator<<(std::ostream& o, const Token& tok) {
  return o << token_kind_to_string(tok.kind) << ':' << tok.spelling << ':'
           << tok.line;
}

struct CScanner : dvc::scanner {
  using dvc::scanner::scanner;

  std::string parse_identifier() {
    std::ostringstream oss;
  again:
    char c = peek();
    if (std::isalnum(c) || c == '_') {
      incr();
      oss.write(&c, 1);
      goto again;
    }
    return oss.str();
  }

  std::string parse_number() {
    std::ostringstream oss;
  again:
    char c = peek();
    if (std::isalnum(c) || c == '_') {
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
      return;
    }
  }

  Token parse_next_token() {
    static const std::unordered_map<char, Token::Kind> punctuation = {
        {'[', Token::LBRACK},   {']', Token::RBRACK}, {'*', Token::ASTERISK},
        {'(', Token::LPAREN},   {')', Token::RPAREN}, {',', Token::COMMA},
        {';', Token::SEMICOLON}};

    static const std::unordered_map<std::string, Token::Kind> keywords = {
        {"const", Token::CONST}, {"struct", Token::STRUCT}};

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

    if (std::isdigit(c)) {
      return {Token::NUMBER, parse_number(), l};
    }

    if (std::isalpha(c) || c == '_') {
      std::string identifier = parse_identifier();
      auto it = keywords.find(identifier);
      if (it != keywords.end()) {
        return {it->second, identifier, l};
      } else {
        return {Token::IDENTIFIER, identifier, l};
      }
    }

    DVC_ERROR("While parsing ", get_data());
    DVC_FATAL("Unexpected character: ", c, " at line ", l);
  }
};

class CParser : public dvc::parser<Token> {
 public:
  using dvc::parser<Token>::parser;

  Declaration parse_declaration_end() {
    Declaration decl = parse_declaration();
    DVC_ASSERT(peek() == Token::END);
    return decl;
  }

  Declaration parse_declaration() {
    std::optional<std::string> root;
    bool const_ = false;
    while (true) {
      switch (peek().kind) {
        case Token::STRUCT:
          incr();
          DVC_ASSERT(peek() == Token::IDENTIFIER);
          [[fallthrough]];
        case Token::IDENTIFIER:
          if (root) goto done_specifiers;
          root = pop().spelling;
          break;
        case Token::CONST:
          DVC_ASSERT(!const_);
          const_ = true;
          incr();
          break;
        default:
          DVC_ASSERT(root, "unexpected token: ", peek());
          goto done_specifiers;
      }
    }
  done_specifiers:;

    Type* t = new Name(root.value());

    if (const_) t = new Const(t);

    while (peek() == Token::ASTERISK) {
      t = new Pointer(t);
      incr();
      if (peek() == Token::CONST) {
        t = new Const(t);
        incr();
      }
    }

    DVC_ASSERT(peek() == Token::IDENTIFIER);
    std::string name = pop().spelling;

    if (peek() == Token::LBRACK) {
      incr();
      Expr* e;
      DVC_ASSERT(peek() == Token::IDENTIFIER || peek() == Token::NUMBER);
      if (peek() == Token::IDENTIFIER) {
        e = new Reference(pop().spelling);
      } else {
        e = new Number(pop().spelling);
      }
      DVC_ASSERT(pop() == Token::RBRACK);
      t = new Array(t, e);
    }

    return Declaration{name, t};
  };

  FunctionPrototype parse_function_prototype() {
    FunctionPrototype function_prototype;
    DVC_ASSERT(pop().spelling == "typedef");
    DVC_ASSERT(peek() == Token::IDENTIFIER);
    Type* t = new Name(pop().spelling);
    if (peek() == Token::ASTERISK) {
      t = new Pointer(t);
      incr();
    }

    function_prototype.return_type = t;

    DVC_ASSERT(pop() == Token::LPAREN);
    DVC_ASSERT(peek() == Token::IDENTIFIER);
    DVC_ASSERT(pop().spelling == "VKAPI_PTR");
    DVC_ASSERT(pop() == Token::ASTERISK);
    DVC_ASSERT(peek() == Token::IDENTIFIER);
    function_prototype.name = pop().spelling;
    DVC_ASSERT(pop() == Token::RPAREN);
    DVC_ASSERT(pop() == Token::LPAREN);
    if (peek() == Token::IDENTIFIER && peek(1) == Token::RPAREN) {
      incr(2);
      return function_prototype;
    }
  parse_another_param:
    function_prototype.params.push_back(parse_declaration());
    switch (peek().kind) {
      case Token::COMMA:
        incr();
        goto parse_another_param;
      default:
        DVC_FATAL(peek());
      case Token::RPAREN:
        incr();
        [[fallthrough]];
    }
    return function_prototype;
  };
};

template <typename F>
auto parse(const std::string& code, F f) {
  CScanner scanner("vk.xml", code);
  std::vector<Token> tokens;
  while (true) {
    Token token = scanner.parse_next_token();
    tokens.push_back(token);
    if (token == Token::END) break;
  }

  CParser parser("vk.xml", tokens);

  return (parser.*f)();
}

Declaration parse_declaration(const std::string& code) {
  return parse(code, &CParser::parse_declaration_end);
}

FunctionPrototype parse_function_prototype(const std::string& code) {
  return parse(code, &CParser::parse_function_prototype);
}

}  // namespace mnc
