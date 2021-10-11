//
//  FibLang
//  A Programming Language just for writing Fibonacci number program. :)
//
//  Copyright (c) 2021 Yuji Hirose. All rights reserved. MIT License
//  MIT License
//

#include <fstream>
#include <variant>

#include "peglib.h"

using namespace std;
using namespace peg;
using namespace peg::udl;

//-----------------------------------------------------------------------------
// Parser
//-----------------------------------------------------------------------------

shared_ptr<Ast> parse(const string& source, ostream& out) {
  parser pg(R"(
    # Syntax
    START             ← STATEMENTS
    STATEMENTS        ← (DEFINITION / EXPRESSION)*
    DEFINITION        ← 'def' Identifier '(' Identifier ')' EXPRESSION
    EXPRESSION        ← TERNARY
    TERNARY           ← CONDITION ('?' EXPRESSION ':' EXPRESSION)?
    CONDITION         ← INFIX (ConditionOperator INFIX)?
    INFIX             ← CALL (InfixOperator CALL)*
    CALL              ← PRIMARY ('(' EXPRESSION ')')?
    PRIMARY           ← FOR / Identifier / '(' EXPRESSION ')' / Number
    FOR               ← 'for' Identifier 'from' Number 'to' Number EXPRESSION

    # Token
    ConditionOperator ← '<'
    InfixOperator     ← '+' / '-'
    Identifier        ← !Keyword < [a-zA-Z][a-zA-Z0-9_]* >
    Number            ← < [0-9]+ >
    Keyword           ← 'def' / 'for' / 'from' / 'to'

    %whitespace       ← [ \t\r\n]*
    %word             ← [a-zA-Z]
  )");

  pg.log = [&](size_t ln, size_t col, const string& msg) {
    out << ln << ":" << col << ": " << msg << endl;
  };

  pg.enable_ast();

  shared_ptr<Ast> ast;
  if (pg.parse(source, ast)) {
    return AstOptimizer(true).optimize(ast);
  }
  return nullptr;
}

//-----------------------------------------------------------------------------
// Value
//-----------------------------------------------------------------------------

struct Value;
struct Environment;

struct Function {
  string_view param;
  function<Value(shared_ptr<Environment> env)> eval;

  Function(string_view param,
           function<Value(shared_ptr<Environment> env)>&& eval)
      : param(param), eval(eval) {}
};

struct Value {
  enum class Type { Nil, Bool, Long, Function };
  Type type;
  any v;
  //variant<nullptr_t, bool, long, string_view, Function> v;

  // Constructor
  Value() : type(Type::Nil) {}
  explicit Value(bool b) : type(Type::Bool), v(b) {}
  explicit Value(long l) : type(Type::Long), v(l) {}
  explicit Value(Function&& f) : type(Type::Function), v(f) {}

  // Cast value
  bool to_bool() const {
    switch (type) {
      case Type::Bool:
        return any_cast<bool>(v);
      case Type::Long:
        return any_cast<long>(v) != 0;
      default:
        throw runtime_error("type error.");
    }
  }

  long to_long() const {
    switch (type) {
      case Type::Long:
        return any_cast<long>(v);
      default:
        throw runtime_error("type error.");
    }
  }

  Function to_function() const {
    switch (type) {
      case Type::Function:
        return any_cast<Function>(v);
      default:
        throw runtime_error("type error.");
    }
  }

  // Comparison
  bool operator<(const Value& rhs) const {
    switch (type) {
      case Type::Nil:
        return false;
      case Type::Bool:
        return to_bool() < rhs.to_bool();
      case Type::Long:
        return to_long() < rhs.to_long();
      default:
        throw logic_error("invalid internal condition.");
    }
    // NOTREACHED
  }

  // String representation
  string str() const {
    switch (type) {
      case Type::Nil:
        return "nil";
      case Type::Bool:
        return to_bool() ? "true" : "false";
      case Type::Long:
        return std::to_string(to_long());
      case Type::Function:
        return "[function]";
      default:
        throw logic_error("invalid internal condition.");
    }
  }
};

//-----------------------------------------------------------------------------
// Environment
//-----------------------------------------------------------------------------

struct Environment {
  shared_ptr<Environment> outer;
  map<string_view, Value> values;

  Environment(shared_ptr<Environment> outer = nullptr) : outer(outer) {}

  const Value& get_value(string_view s) const {
    if (values.find(s) != values.end()) {
      return values.at(s);
    } else if (outer) {
      return outer->get_value(s);
    }
    throw runtime_error("undefined variable '" + string(s) + "'...");
  }

  void set_value(string_view s, Value&& val) { values.emplace(s, val); }

  static shared_ptr<Environment> make_with_builtins() {
    auto env = make_shared<Environment>();
    env->set_value("puts"sv,
                   Value(Function("arg", [](shared_ptr<Environment> env) {
                     cout << env->get_value("arg").str() << endl;
                     return Value();
                   })));
    return env;
  }
};

//-----------------------------------------------------------------------------
// Interpreter
//-----------------------------------------------------------------------------

Value eval(const Ast& ast, shared_ptr<Environment> env) {
  switch (ast.tag) {
    // Rules
    case "STATEMENTS"_: {
      // (DEFINITION / EXPRESSION)*
      if (!ast.nodes.empty()) {
        auto it = ast.nodes.begin();
        while (it != ast.nodes.end() - 1) {
          eval(**it, env);
          ++it;
        }
        return eval(**it, env);
      }
      return Value();
    }
    case "DEFINITION"_: {
      // 'def' Identifier '(' Identifier ')' EXPRESSION
      auto name = ast.nodes[0]->token;
      auto param = ast.nodes[1]->token;
      auto body = ast.nodes[2];

      env->set_value(
          name, Value(Function(param, [=](shared_ptr<Environment> callEnv) {
            return eval(*body, callEnv);
          })));

      return Value();
    }
    case "TERNARY"_: {
      // CONDITION ('?' EXPRESSION ':' EXPRESSION)?
      auto cond = eval(*ast.nodes[0], env).to_bool();
      auto idx = cond ? 1 : 2;
      return eval(*ast.nodes[idx], env);
    }
    case "CONDITION"_: {
      // INFIX (ConditionOperator INFIX)?
      auto lhs = eval(*ast.nodes[0], env);
      auto rhs = eval(*ast.nodes[2], env);
      auto ret = lhs < rhs;
      return Value(ret);
    }
    case "INFIX"_: {
      // CALL (InfixOperator CALL)*
      auto l = eval(*ast.nodes[0], env).to_long();
      for (size_t i = 1; i < ast.nodes.size(); i += 2) {
        auto o = ast.nodes[i]->token;
        auto r = eval(*ast.nodes[i + 1], env).to_long();
        if (o == "+") {
          l += r;
        } else if (o == "-") {
          l -= r;
        }
      }
      return Value(l);
    }
    case "CALL"_: {
      // PRIMARY ('(' EXPRESSION ')')?
      auto name = ast.nodes[0]->token;
      auto fn = env->get_value(name).to_function();
      auto val = eval(*ast.nodes[1], env);

      auto callEnv = make_shared<Environment>(env);
      callEnv->set_value(fn.param, move(val));

      try {
        return fn.eval(callEnv);
      } catch (const Value& e) {
        return e;
      }
    }
    case "FOR"_: {
      // 'for' Identifier 'from' Number 'to' Number EXPRESSION
      auto ident = ast.nodes[0]->token;
      auto from = eval(*ast.nodes[1], env).to_long();
      auto to = eval(*ast.nodes[2], env).to_long();
      auto& expr = *ast.nodes[3];

      for (auto i = from; i <= to; i++) {
        auto call_env = make_shared<Environment>(env);
        call_env->set_value(ident, Value(i));
        eval(expr, call_env);
      }
      return Value();
    }

    // Tokens
    case "Identifier"_: {
      // !Keyword < [a-zA-Z][a-zA-Z0-9_]* >
      return Value(env->get_value(ast.token));
    }
    case "Number"_: {
      // < [0-9]+ >
      return Value(ast.token_to_number<long>());
    }
  }
  return Value();
}

//-----------------------------------------------------------------------------
// main
//-----------------------------------------------------------------------------

int main(int argc, const char** argv) {
  if (argc < 2) {
    cerr << "usage: fib [source file path]" << endl;
    return -1;
  }

  ifstream f{argv[1]};
  if (!f) {
    cerr << "can't open the source file." << endl;
    return -2;
  }
  auto s = string(istreambuf_iterator<char>(f), istreambuf_iterator<char>());

  try {
    auto ast = parse(s, cerr);
    if (!ast) {
      return -3;
    }
    // cerr << ast_to_s(ast) << endl;

    auto env = Environment::make_with_builtins();
    eval(*ast, env);
  } catch (const exception& e) {
    cerr << e.what() << endl;
    return -4;
  }

  return 0;
}
