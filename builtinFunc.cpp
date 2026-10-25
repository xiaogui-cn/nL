#include "builtinFunc.hpp"
#include <iostream>
#include <limits>

namespace tool::builtin
{
    using namespace tool::lex;
    using namespace tool::sym;

    void initBuiltinFunctions()
    {
        {
            // 输出函数
            FuncInfo printFi;
            printFi.isBuiltin = true;
            printFi.retType = "void";
            printFi.params.push_back({"any", false, "s"}); // 第二个参数是isRef
            printFi.handler = [](const std::vector<Value> &args) -> Value
            {
                if (!args.empty())
                {
                    const Value &v = args[0];
                    if (v.type == Value::INT)
                        std::cout << v.intVal;
                    else if (v.type == Value::BOOL)
                        std::cout << (v.boolVal ? "true" : "false");
                    else if (v.type == Value::STRING)
                        std::cout << v.strVal;
                    else if (v.type == Value::CHAR)
                        std::cout << v.charVal;
                    else if (v.type == Value::FLOAT) // 新增
                        std::cout << v.floatVal;
                    std::cout << std::flush;
                }
                return Value{};
            };
            globalFuncTable["_windows_cmd_print"] = printFi;
        }

        {
            // 输入函数
            FuncInfo cinFi;
            cinFi.isBuiltin = true;
            cinFi.retType = "auto";
            cinFi.params.push_back({"any", false, "prompt"});
            cinFi.handler = [](const std::vector<Value> &args) -> Value
            {
                Value res{};

                // 不要再 ignore 一整行：getline 自己会吃掉行尾换行，
                // 之前的 ignore 会把用户输入的第一行直接吞掉。
                std::string line;
                std::getline(std::cin, line);

                if (!args.empty())
                {
                    const Value &v = args[0];
                    switch (v.type)
                    {
                    case Value::INT:
                        std::cout << v.intVal;
                        res.type = Value::INT;
                        break;
                    case Value::BOOL:
                        std::cout << (v.boolVal ? "true" : "false");
                        res.type = Value::BOOL;
                        break;
                    case Value::STRING:
                        std::cout << v.strVal;
                        res.type = Value::STRING;
                        break;
                    case Value::CHAR:
                        std::cout << v.charVal;
                        res.type = Value::CHAR;
                        break;
                    case Value::FLOAT:
                        std::cout << v.floatVal;
                        res.type = Value::FLOAT;
                        break;
                    default:
                        break;
                    }
                    std::cout << std::flush;
                }

                if (res.type == Value::INT)
                {
                    try
                    {
                        res.intVal = std::stoi(line);
                    }
                    catch (std::invalid_argument &e)
                    {
                        std::cout << "\nINPUT IS NOT A NUMBER.\n";
                        std::exit(1);
                    }
                    catch (std::out_of_range &e)
                    {
                        std::cout << "\nINPUT NUMBER TOO BIG.\n";
                        std::exit(1);
                    }
                }
                else if (res.type == Value::BOOL)
                    res.boolVal = line == "true" ? true : false;
                else if (res.type == Value::CHAR)
                    res.charVal = line[0];
                else if (res.type == Value::FLOAT)
                {
                    try
                    {
                        res.floatVal = std::stof(line);
                    }
                    catch (std::invalid_argument &e)
                    {
                        std::cout << "\nINPUT IS NOT A NUMBER.\n";
                        std::exit(1);
                    }
                    catch (std::out_of_range &e)
                    {
                        std::cout << "\nINPUT NUMBER TOO BIG.\n";
                        std::exit(1);
                    }
                }
                else
                    res.strVal = line;

                return res;
            };
            globalFuncTable["_windows_cmd_cin"] = cinFi;
        }

        {
            // 终止函数
            FuncInfo exitFi;
            exitFi.isBuiltin = true;
            exitFi.retType = "void";
            exitFi.params.push_back({"int", false, "status"});
            exitFi.handler = [](const std::vector<Value> &args) -> Value
            {
                exit(args[0].intVal);
                return Value{};
            };
            globalFuncTable["_exit"] = exitFi;
        }
    }
}
