#ifndef BUILTINFUNC_HPP
#define BUILTINFUNC_HPP
#include "symbolTable.hpp"
#include "lexer.hpp"
namespace tool::builtin
{
    void exec_windows_cmd_print(const tool::lex::Value &val);
    void initBuiltinFunctions();
}
#endif
