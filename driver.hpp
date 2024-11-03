#pragma once

#include "parser.hpp"

#define YY_DECL yy::parser::symbol_type yylex([[maybe_unused]]cmd::driver &drv)
YY_DECL;

namespace cmd {
  class driver {
  public:
    driver() = default;
    int parse();
  };
} // namespace cmd
