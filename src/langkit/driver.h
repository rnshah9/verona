// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#pragma once

#include "parse.h"
#include "rewrite.h"

#include <CLI/CLI.hpp>

namespace langkit
{
  class Driver
  {
  private:
    CLI::App app;
    Parse parser;
    std::vector<std::pair<Token, Pass>> passes;
    std::vector<std::string> limits;

    bool emit_ast = false;
    bool diag = false;
    std::string path;
    std::string limit;

  public:
    Driver(
      const std::string& name,
      Parse parser,
      std::initializer_list<std::pair<Token, Pass>> passes)
    : app(name), parser(parser), passes(passes)
    {
      for (auto& [token, pass] : passes)
        limits.push_back(token.str());

      app.add_flag("-a,--ast", emit_ast, "Emit an abstract syntax tree.");
      app.add_flag("-d,--diagnostics", diag, "Emit diagnostics.");
      app.add_option("-p,--pass", limit, "Run up to this pass.")
        ->transform(CLI::IsMember(limits));
      app.add_option("path", path, "Path to compile.")->required();
    }

    int run(int argc, char** argv)
    {
      try
      {
        app.parse(argc, argv);
      }
      catch (const CLI::ParseError& e)
      {
        return app.exit(e);
      }

      auto lim = Token(limit);
      auto ast = parser.parse(path);

      if (!ast)
        return -1;

      for (auto& [name, pass] : passes)
      {
        size_t count;
        size_t changes;
        std::tie(ast, count, changes) = pass.repeat(ast);

        if (diag)
        {
          std::cout << "Pass " << name.str() << ": " << count << " iterations, "
                    << changes << " nodes rewritten." << std::endl;
        }

        if (name == lim)
          break;
      }

      if (emit_ast)
        std::cout << ast->str() << std::endl;

      return 0;
    }
  };
}