#ifndef PARSER_HPP
#define PARSER_HPP
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "lexer.hpp"
#include "symbolTable.hpp"
#include "errorC.hpp"
#include "builtinFunc.hpp"
#include "fileC.hpp"

namespace tool::parse
{
    // 解析期类型环境：变量名 → 类型描述
    using LocalEnv = std::unordered_map<std::string, tool::lex::Value>;

    using namespace tool::lex;
    using namespace tool::sym;
    using namespace tool::builtin;

    // 把解析期记录的值类型映射回源码里的类型名（ref 实参用）
    inline std::string valueTypeName(const Value &v)
    {
        switch (v.type)
        {
        case Value::INT:
            return "int";
        case Value::BOOL:
            return "bool";
        case Value::STRING:
            return "string";
        case Value::CHAR:
            return "char";
        case Value::FLOAT:
            return "float";
        case Value::OBJECT:
            return v.objectClass;
        case Value::ARRAY:
            return "array";
        default:
            return "void";
        }
    }

    // 基础类型名（数组元素类型用）
    inline std::string basicTypeName(Value::Type t)
    {
        switch (t)
        {
        case Value::INT:
            return "int";
        case Value::BOOL:
            return "bool";
        case Value::STRING:
            return "string";
        case Value::CHAR:
            return "char";
        case Value::FLOAT:
            return "float";
        default:
            return "void";
        }
    }

    // int / float / char 之间允许隐式转换
    inline bool isNumericTypeName(const std::string &t)
    {
        return t == "int" || t == "float" || t == "char";
    }

    inline bool isBuiltinTypeName(const std::string &t)
    {
        return t == "int" || t == "void" || t == "bool" || t == "string" ||
               t == "char" || t == "float" || t == "auto";
    }

    inline bool isClassType(const std::string &t)
    {
        return lookupClass(t) != nullptr;
    }

    // 运算符重载的 mangled 方法名，例如 operator+$P、operator[]$int、operator()$int
    inline std::string operatorMangledName(const std::string &symbol, const std::vector<ParamInfo> &params)
    {
        std::string key = "operator" + symbol + "$";
        for (size_t i = 0; i < params.size(); i++)
        {
            if (i)
                key += ",";
            key += params[i].type;
        }
        return key;
    }

    // 构造函数 mangled 名，例如 P$int、P$int,string
    inline std::string constructorMangledName(const std::string &className, const std::vector<ParamInfo> &params)
    {
        std::string key = className + "$";
        for (size_t i = 0; i < params.size(); i++)
        {
            if (i)
                key += ",";
            key += params[i].type;
        }
        return key;
    }

    // 压入一个常量值（模板 var 参数用）
    inline void pushConstant(RpnList &out, const Value &v)
    {
        switch (v.type)
        {
        case Value::INT:
            out.push_back(RpnOp::PushInt(v.intVal));
            break;
        case Value::BOOL:
            out.push_back(RpnOp::PushBool(v.boolVal));
            break;
        case Value::STRING:
            out.push_back(RpnOp::PushStr(v.strVal));
            break;
        case Value::CHAR:
            out.push_back(RpnOp::PushChar(v.charVal));
            break;
        case Value::FLOAT:
            out.push_back(RpnOp::PushFloat(v.floatVal));
            break;
        default:
            out.push_back(RpnOp::PushInt(0));
            break;
        }
    }

    struct ForLoopMarker
    {
        std::vector<int> patchList;
    };

    class Parser
    {
    public:
        std::vector<Token> &tokens;
        std::vector<std::string> &sourceLines;
        int pos;
        error &err;
        std::string fileName;
        std::string fileDir;          // 当前文件所在目录
        std::string currentClass;     // 当前正在解析方法体的类名（访问控制用）
        bool currentIsCtor = false;   // 当前正在解析构造函数体
        std::string templateBaseName; // 模板实例化时的原始模板名（构造函数名用）

        // loop
        std::vector<int> loopBreakPatchStack;
        std::vector<ForLoopMarker> loopStack;
        // 模板实例化时的替换表
        std::unordered_map<std::string, std::string> typeSubst; // 类型参数 → 实参类型
        std::unordered_map<std::string, Value> valueSubst;      // var 参数 → 常量

        static inline std::unordered_set<std::string> instantiatingClasses;

        // 判断当前上下文能否访问 owner 类的某个成员
        bool canAccess(const std::string &owner, Access a) const
        {
            if (a == Access::Public)
                return true;
            if (a == Access::Protected)
                return !currentClass.empty() && currentClass == owner; // 无继承，等价 private
            return !currentClass.empty() && currentClass == owner;
        }

        static inline std::unordered_set<std::string> includedFiles;
        // 系统库头文件目录
        static inline const std::string sysIncludeDir = R"(C:\Users\WEIBO\nLconfig\hfile)";

        void handleInclude(RpnList &mainOut);

        std::string parseFactor(LocalEnv &env, RpnList &out);
        std::string parseTerm(LocalEnv &env, RpnList &out);
        std::string parseAddSub(LocalEnv &env, RpnList &out);     // 原parseExpression改名
        std::string parseRelational(LocalEnv &env, RpnList &out); // 新增关系比较
        std::string parseExpression(LocalEnv &env, RpnList &out); // 新增相等比较，作为最外层

        void parseBlock(LocalEnv &env, RpnList &out, std::vector<std::string> &retTypes);
        void parseBranchBody(LocalEnv &env, RpnList &out, std::vector<std::string> &retTypes, const char *which);
        void parseArrayInitializer(LocalEnv &env, RpnList &out, int ndims, std::vector<int> &path, int level, const std::string &baseType, int *topCount = nullptr);
        void parseArrayDeclRest(LocalEnv &env, RpnList &out, const std::string &arrName, const Token &nameTok, const std::string &elemTypeStr);
        void parseStatement(LocalEnv &env, RpnList &out, std::vector<std::string> &retTypes);

        //   构造函数补充 fileDir 初始化
        Parser(std::vector<Token> &toks, std::vector<std::string> &srcLines, error &e, const std::string &fname)
            : tokens(toks), sourceLines(srcLines), pos(0), err(e), fileName(fname)
        {
            size_t slash = fname.find_last_of("/\\");
            fileDir = (slash == std::string::npos) ? "." : fname.substr(0, slash);
        }

        Token &peek() { return tokens[pos]; }
        Token consume() { return tokens[pos++]; }
        std::string getCurrentSourceLine();
        bool expect(TokenKind k, const std::string &msg);
        std::string parseType(bool &outIsConst, bool &outIsRef);
        void parseCallArgs(FuncInfo *m, LocalEnv &env, RpnList &out, int &argCount,
                           std::vector<std::string> &argTypes,
                           const std::string &callName, const Token &nameTok);
        void parseArgsGeneric(LocalEnv &env, RpnList &out, std::vector<std::string> &argTypes);
        std::string resolveOperatorOverload(ClassInfo *cls, const std::string &symbol,
                                            const std::vector<std::string> &argTypes,
                                            const Token &tok, const std::string &opText);
        std::string resolveConstructor(ClassInfo *cls, const std::vector<std::string> &argTypes,
                                       const Token &tok, const std::string &classNameText);
        void parseFunctionDecl();
        void parseProgram(RpnList &mainOut);
        void parseClassDecl();
        void parseTemplateDecl(const std::string &name, const Token &nameTok);
        std::string instantiateTemplate(const Token &nameTok);
    };

    // ============== 类外部实现 ==============
    inline std::string Parser::getCurrentSourceLine()
    {
        int ln = peek().line;
        if ((size_t)ln < sourceLines.size())
            return sourceLines[ln];
        return "";
    }

    // col 参数
    inline bool Parser::expect(TokenKind k, const std::string &msg)
    {
        if (peek().kind == k)
        {
            consume();
            return true;
        }
        Token t = peek();
        err.Error(fileName, t.line, t.col, t.text, msg, getCurrentSourceLine());
        if (peek().kind != TOK_EOF)
            consume();
        return false;
    }

    inline std::string Parser::parseType(bool &outIsConst, bool &outIsRef)
    {
        outIsConst = false;
        outIsRef = false;

        // 解析 const 修饰符
        if (peek().kind == TOK_CONST)
        {
            consume();
            outIsConst = true;
        }
        // 解析 ref 修饰符
        if (peek().kind == TOK_REF)
        {
            consume();
            outIsRef = true;
        }

        Token t = consume();
        // 内置基本类型
        if (t.kind == TOK_INT || t.kind == TOK_VOID || t.kind == TOK_STRING || t.kind == TOK_CHAR || t.kind == TOK_BOOL || t.kind == TOK_AUTO || t.kind == TOK_FLOAT)
        {
            return t.text;
        }

        // 自定义类类型：必须是已经声明过的类
        if (t.kind == TOK_IDENT)
        {
            // 模板类型参数替换
            auto sub = typeSubst.find(t.text);
            if (sub != typeSubst.end())
                return sub->second;

            // 把 var 参数当类型用
            if (valueSubst.count(t.text))
            {
                err.Error(fileName, t.line, t.col, t.text,
                          "'" + t.text + "' is a value parameter, not a type",
                          getCurrentSourceLine());
                return "int";
            }

            // 模板实例化：Box<int>
            if (peek().kind == TOK_LESS && classTemplates.count(t.text))
                return instantiateTemplate(t);

            if (classTemplates.count(t.text))
            {
                err.Error(fileName, t.line, t.col, t.text,
                          "template '" + t.text + "' requires type arguments",
                          getCurrentSourceLine());
                return "int";
            }

            if (peek().kind == TOK_LESS && lookupClass(t.text))
            {
                err.Error(fileName, t.line, t.col, t.text,
                          "'" + t.text + "' is not a template", getCurrentSourceLine());
                // 跳过 <...>
                int depth = 0;
                while (peek().kind != TOK_EOF)
                {
                    if (peek().kind == TOK_LESS)
                        depth++;
                    else if (peek().kind == TOK_GREATER)
                    {
                        depth--;
                        if (depth == 0)
                        {
                            consume();
                            break;
                        }
                    }
                    consume();
                }
                return t.text;
            }

            if (lookupClass(t.text))
                return t.text;

            err.Error(fileName, t.line, t.col, t.text,
                      "unknown type '" + t.text + "'", getCurrentSourceLine());
            return "int"; // 容错，避免连锁崩溃
        }

        // 都不符合才报错
        err.Error(fileName, t.line, t.col, t.text, "expected type", getCurrentSourceLine());
        return "int";
    }

    // class Name<type T, var N> { ... }：只保存类体 token，实例化时再解析
    inline void Parser::parseTemplateDecl(const std::string &name, const Token &nameTok)
    {
        TemplateInfo ti;
        ti.name = name;
        ti.fileName = fileName;
        ti.line = nameTok.line;
        ti.col = nameTok.col;

        consume(); // 吃掉 '<'
        while (true)
        {
            bool isType = false;
            if (peek().kind == TOK_IDENT && peek().text == "type")
            {
                consume();
                isType = true;
            }
            else if (peek().kind == TOK_VAR)
            {
                consume();
                isType = false;
            }
            else
            {
                Token bad = peek();
                err.Error(fileName, bad.line, bad.col, bad.text,
                          "expected 'type' or 'var' in template parameter list",
                          getCurrentSourceLine());
                return;
            }

            Token pn = consume();
            if (pn.kind != TOK_IDENT)
            {
                err.Error(fileName, pn.line, pn.col, pn.text,
                          "expected template parameter name", getCurrentSourceLine());
                return;
            }

            for (auto &p : ti.params)
            {
                if (p.name == pn.text)
                {
                    err.Error(fileName, pn.line, pn.col, pn.text,
                              "duplicate template parameter '" + pn.text + "'",
                              getCurrentSourceLine());
                }
            }
            ti.params.push_back({isType, pn.text});

            if (peek().kind == TOK_COMMA)
            {
                consume();
                continue;
            }
            break;
        }

        if (!expect(TOK_GREATER, "expected '>' after template parameters"))
            return;

        if (peek().kind != TOK_LBRACE)
        {
            err.Error(fileName, peek().line, peek().col, peek().text,
                      "expected '{' after template parameters", getCurrentSourceLine());
            return;
        }

        int bodyStart = pos;
        int depth = 0;
        int i = pos;
        for (; i < (int)tokens.size(); i++)
        {
            if (tokens[i].kind == TOK_LBRACE)
                depth++;
            else if (tokens[i].kind == TOK_RBRACE)
            {
                depth--;
                if (depth == 0)
                {
                    i++;
                    break;
                }
            }
        }
        if (depth != 0)
        {
            err.Error(fileName, nameTok.line, nameTok.col, name,
                      "unterminated class body", getCurrentSourceLine());
            return;
        }

        ti.bodyTokens.assign(tokens.begin() + bodyStart, tokens.begin() + i);
        ti.sourceLines = sourceLines;
        pos = i;

        if (classTemplates.count(name) || globalClassTable.count(name))
        {
            err.Error(fileName, nameTok.line, nameTok.col, name,
                      "class or template '" + name + "' is already defined",
                      getCurrentSourceLine());
            return;
        }
        classTemplates[name] = ti;
    }

    // 把 Box<Args...> 单态化成一个具体类，返回实例化后的类名（如 Box<int>）
    inline std::string Parser::instantiateTemplate(const Token &nameTok)
    {
        auto tit = classTemplates.find(nameTok.text);
        if (tit == classTemplates.end())
        {
            err.Error(fileName, nameTok.line, nameTok.col, nameTok.text,
                      "unknown template '" + nameTok.text + "'", getCurrentSourceLine());
            return "int";
        }
        TemplateInfo &ti = tit->second;

        expect(TOK_LESS, "expected '<'");

        std::vector<std::string> argNames;
        std::unordered_map<std::string, std::string> tmap;
        std::unordered_map<std::string, Value> vmap;

        bool argError = false;
        for (size_t i = 0; i < ti.params.size(); i++)
        {
            if (i > 0)
            {
                if (peek().kind != TOK_COMMA)
                {
                    err.Error(fileName, nameTok.line, nameTok.col, nameTok.text,
                              "too few template arguments for '" + ti.name + "'",
                              getCurrentSourceLine());
                    argError = true;
                    break;
                }
                consume();
            }

            const TemplateParam &p = ti.params[i];
            if (p.isType)
            {
                bool dc = false, dr = false;
                std::string at = parseType(dc, dr);
                tmap[p.name] = at;
                argNames.push_back(at);
            }
            else
            {
                Token vt = peek();
                Value val{};
                std::string canon;

                if (vt.kind == TOK_NUMBER)
                {
                    consume();
                    val.type = Value::INT;
                    val.intVal = std::stoi(vt.text);
                    canon = std::to_string(val.intVal);
                }
                else if (vt.kind == TOK_FLOAT)
                {
                    consume();
                    val.type = Value::FLOAT;
                    val.floatVal = std::stof(vt.text);
                    canon = std::to_string(val.floatVal);
                }
                else if (vt.kind == TOK_STRLIT)
                {
                    consume();
                    val.type = Value::STRING;
                    val.strVal = vt.text;
                    canon = "\"" + vt.text + "\"";
                }
                else if (vt.kind == TOK_CHAR_LIT)
                {
                    consume();
                    val.type = Value::CHAR;
                    val.charVal = vt.text.empty() ? 0 : vt.text[0];
                    val.intVal = (unsigned char)val.charVal;
                    canon = "'" + std::string(1, val.charVal) + "'";
                }
                else if (vt.kind == TOK_TRUE || vt.kind == TOK_FALSE)
                {
                    consume();
                    val.type = Value::BOOL;
                    val.boolVal = (vt.kind == TOK_TRUE);
                    canon = val.boolVal ? "true" : "false";
                }
                else
                {
                    err.Error(fileName, vt.line, vt.col, vt.text,
                              "template value argument must be a literal",
                              getCurrentSourceLine());
                    argError = true;
                    break;
                }

                vmap[p.name] = val;
                argNames.push_back(canon);
            }
        }

        if (!argError && peek().kind == TOK_COMMA)
        {
            err.Error(fileName, nameTok.line, nameTok.col, nameTok.text,
                      "too many template arguments for '" + ti.name + "'",
                      getCurrentSourceLine());
            argError = true;
        }

        if (argError)
        {
            // 跳过剩余的 <...>，避免级联错误
            int depth = 1;
            while (peek().kind != TOK_EOF && depth > 0)
            {
                if (peek().kind == TOK_LESS)
                    depth++;
                else if (peek().kind == TOK_GREATER)
                {
                    depth--;
                    if (depth == 0)
                    {
                        consume();
                        break;
                    }
                }
                consume();
            }
            return "int";
        }

        if (!expect(TOK_GREATER, "expected '>' after template arguments"))
            return "int";

        std::string canonical = ti.name + "<";
        for (size_t i = 0; i < argNames.size(); i++)
        {
            if (i)
                canonical += ",";
            canonical += argNames[i];
        }
        canonical += ">";

        if (globalClassTable.count(canonical) || instantiatingClasses.count(canonical))
            return canonical;

        // 合成 class <canonical> { ...原类体... } 的 token 流，交给子 Parser 解析
        std::vector<Token> subTokens;
        subTokens.push_back({TOK_CLASS, "class", ti.line, ti.col, ti.fileName});
        subTokens.push_back({TOK_IDENT, canonical, ti.line, ti.col, ti.fileName});
        for (auto &bt : ti.bodyTokens)
            subTokens.push_back(bt);
        subTokens.push_back({TOK_EOF, "", ti.line, ti.col, ti.fileName});

        instantiatingClasses.insert(canonical);

        Parser sub(subTokens, ti.sourceLines, err, ti.fileName);
        sub.typeSubst = tmap;
        sub.valueSubst = vmap;
        sub.templateBaseName = ti.name;
        sub.parseClassDecl();

        instantiatingClasses.erase(canonical);
        return canonical;
    }

    inline void Parser::parseFunctionDecl()
    {
        // 新语法：function name(params) -> retType { ... }
        expect(TOK_FUNCTION, "expected 'function'");
        Token nameTok = consume();
        if (nameTok.kind != TOK_IDENT)
        {
            err.Error(fileName, nameTok.line, nameTok.col, nameTok.text, "expected function name", getCurrentSourceLine());
            return;
        }
        std::string funcName = nameTok.text;
        expect(TOK_LPAREN, "expected '('");

        // 解析参数列表
        std::vector<ParamInfo> params;
        while (peek().kind != TOK_RPAREN && peek().kind != TOK_EOF)
        {
            bool isRef = false;
            bool dummyConst = false;
            std::string pType = parseType(dummyConst, isRef);
            Token pName = consume();
            if (pName.kind != TOK_IDENT)
            {
                err.Error(fileName, pName.line, pName.col, pName.text,
                          "expected parameter name", getCurrentSourceLine());
                return;
            }
            params.push_back({pType, isRef, pName.text});
            if (peek().kind == TOK_COMMA)
                consume();
        }
        expect(TOK_RPAREN, "expected ')'");

        // 可选： -> 返回类型
        std::string retType = "void";
        if (peek().kind == TOK_MINUS)
        {
            Token t1 = consume();
            if (peek().kind == TOK_GREATER)
            {
                consume(); // 吃掉 >
                bool dummyConst = false, dummyRef = false;
                retType = parseType(dummyConst, dummyRef);
            }
            else
            {
                // 不是 ->，报错回滚
                err.Error(fileName, t1.line, t1.col, "-", "expected '->' for function return type", getCurrentSourceLine());
            }
        }

        expect(TOK_LBRACE, "expected '{'");

        // 占位注册函数到全局表
        FuncInfo f{};
        f.isBuiltin = false;
        f.retType = retType;
        f.params = params;
        globalFuncTable[funcName] = f;

        // 构建参数局部环境
        LocalEnv funcEnv;
        for (auto &p : params)
        {
            Value v{};
            if (p.type == "int")
                v.type = Value::INT;
            else if (p.type == "bool")
                v.type = Value::BOOL;
            else if (p.type == "string")
                v.type = Value::STRING;
            else if (p.type == "char")
                v.type = Value::CHAR;
            else if (p.type == "float")
                v.type = Value::FLOAT;
            else if (lookupClass(p.type))
            {
                v.type = Value::OBJECT;
                v.objectClass = p.type;
            }
            else // auto / any / 未知类型
                v.type = Value::V_VOID;
            funcEnv[p.name] = v;
        }

        std::vector<std::string> returnExprTypes;
        parseBlock(funcEnv, globalFuncTable[funcName].body, returnExprTypes);

        // auto 返回值推导
        if (globalFuncTable[funcName].retType == "auto")
        {
            if (returnExprTypes.empty())
            {
                globalFuncTable[funcName].retType = "void";
            }
            else
            {
                std::string deduced = returnExprTypes[0];
                for (auto &t : returnExprTypes)
                {
                    if (t != deduced)
                    {
                        err.Error(fileName, nameTok.line, nameTok.col, funcName,
                                  "auto return type conflict: return type '" + t + "' vs previous '" + deduced + "'",
                                  getCurrentSourceLine());
                        break;
                    }
                }
                globalFuncTable[funcName].retType = deduced;
            }
        }
    }

    inline void Parser::parseStatement(LocalEnv &env, RpnList &out, std::vector<std::string> &retTypes)
    {
        int oldPos = pos; // 进入语句前记录当前指针位置
        switch (peek().kind)
        {
        case TOK_RETURN:
        {
            consume();
            if (peek().kind == TOK_SEMICOLON)
            {
                consume();
                out.push_back(RpnOp::Ret());
                retTypes.push_back("void"); // return; 无表达式
            }
            else
            {
                if (currentIsCtor)
                {
                    err.Error(fileName, peek().line, peek().col, "return",
                              "constructors cannot return a value", getCurrentSourceLine());
                }
                std::string ty = parseExpression(env, out);
                expect(TOK_SEMICOLON, "expected ';'");
                out.push_back(RpnOp::Ret());
                retTypes.push_back(ty);
            }
            return;
        }

        case TOK_JUMP:
        {
            consume();
            expect(TOK_SEMICOLON, "expected ';' after jump");
            if (loopStack.empty())
            {
                err.Error(fileName, peek().line, peek().col, "jump",
                          "jump statement must be inside for‑loop", getCurrentSourceLine());
                return;
            }
            // 获取当前最内层循环的patch列表，记录当前op下标
            out.push_back(RpnOp::BinOp(OP_JUMP_BREAK));
            int opIndex = (int)out.size() - 1;
            loopStack.back().patchList.push_back(opIndex);
            return;
        }

        case TOK_VAR:
        {
            consume(); // 吃掉 var
            bool isConst = false;
            bool dummyRef = false;
            std::string varType = parseType(isConst, dummyRef);

            Token nameTok = peek();
            if (nameTok.kind != TOK_IDENT)
            {
                err.Error(fileName, nameTok.line, nameTok.col, nameTok.text,
                          "expected variable name", getCurrentSourceLine());
                while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                    consume();
                if (peek().kind == TOK_SEMICOLON)
                    consume();
                return;
            }
            std::string varName = consume().text;

            // C++ 风格构造：var P p(args);
            if (peek().kind == TOK_LPAREN)
            {
                ClassInfo *ctorCls = lookupClass(varType);
                if (ctorCls)
                {
                    RpnList argRpn;
                    std::vector<std::string> argTypes;
                    parseArgsGeneric(env, argRpn, argTypes);
                    std::string ctorName = resolveConstructor(ctorCls, argTypes, nameTok, varType);

                    out.push_back(RpnOp::NewObject(varType));
                    for (auto &op : argRpn)
                        out.push_back(op);
                    if (!ctorName.empty())
                        out.push_back(RpnOp::CallCtor(ctorName, (int)argTypes.size()));
                    out.push_back(RpnOp::StoreVar(varName));

                    Value v{};
                    v.type = Value::OBJECT;
                    v.objectClass = varType;
                    v.isConst = isConst;
                    env[varName] = v;

                    expect(TOK_SEMICOLON, "expected ';' after variable declaration");
                    return;
                }
            }

            // const 变量必须初始化
            if (isConst && peek().kind != TOK_ASSIGN)
            {
                err.Error(fileName, nameTok.line, nameTok.col, varName,
                          "const variable must be initialized", getCurrentSourceLine());
            }

            // 有初始化赋值
            if (peek().kind == TOK_ASSIGN)
            {
                consume();
                std::string initType = parseExpression(env, out);

                if (varType == "auto")
                    varType = initType;
                else if (varType != "auto" && initType != varType)
                {
                    // char 与 int 允许隐式互转（按 ASCII）
                    bool compatible =
                        (varType == "char" && initType == "int") ||
                        (varType == "int" && initType == "char") ||
                        (initType == "vd");

                    if (!compatible)
                    {
                        err.Error(fileName, nameTok.line, nameTok.col, varName,
                                  "type mismatch: declared '" + varType + "', got '" + initType + "'",
                                  getCurrentSourceLine());
                    }
                }
            }
            else
            {
                // 无初始化默认值
                if (varType == "int")
                    out.push_back(RpnOp::PushInt(0));
                else if (varType == "bool")
                    out.push_back(RpnOp::PushBool(false));
                else if (varType == "string")
                    out.push_back(RpnOp::PushStr(""));
                else if (varType == "char")
                    out.push_back(RpnOp::PushChar(0));
                else if (varType == "float")
                    out.push_back(RpnOp::PushFloat(0.0f));
                else if (ClassInfo *cc = lookupClass(varType))
                {
                    // 类类型：默认构造（声明了构造函数时调用无参版本）
                    std::vector<std::string> noArgs;
                    std::string ctorName = resolveConstructor(cc, noArgs, nameTok, varType);
                    out.push_back(RpnOp::NewObject(varType));
                    if (!ctorName.empty())
                        out.push_back(RpnOp::CallCtor(ctorName, 0));
                }
                else
                {
                    err.Error(fileName, nameTok.line, nameTok.col, varType,
                              "unknown type '" + varType + "'", getCurrentSourceLine());
                    out.push_back(RpnOp::PushInt(0)); // 占位，保证栈平衡
                }
            }

            expect(TOK_SEMICOLON, "expected ';' after variable declaration");

            // 注册变量到作用域
            Value v{};
            v.isConst = isConst;
            if (varType == "int")
                v.type = Value::INT;
            else if (varType == "bool")
                v.type = Value::BOOL;
            else if (varType == "string")
                v.type = Value::STRING;
            else if (varType == "char")
                v.type = Value::CHAR;
            else if (varType == "float")
                v.type = Value::FLOAT;
            else // 自定义类类型
            {
                v.type = Value::OBJECT;
                v.objectClass = varType;
            }
            env[varName] = v;

            out.push_back(RpnOp::StoreVar(varName));
            return;
        }

        case TOK_IF:
        {
            Token t = peek();
            consume(); // 吃掉 if
            expect(TOK_LPAREN, "expected '(' after 'if'");

            // 解析条件表达式 + 类型校验
            std::string condType = parseExpression(env, out);
            if (condType != "bool" && condType != "int")
            {
                err.Error(fileName, t.line, t.col, "if condition",
                          "condition must be bool or int, got '" + condType + "'",
                          getCurrentSourceLine());
            }

            expect(TOK_RPAREN, "expected ')' after condition");

            // 占位：条件假跳转指令，后面回填地址
            int jmpFalseIdx = (int)out.size();
            out.push_back(RpnOp::JmpIfFalse(0));

            // 解析 if 分支体：{...} 或单条语句（C++ 风格）
            parseBranchBody(env, out, retTypes, "if");

            // 占位：块结束跳转到整个if结尾
            std::vector<int> endJumps;
            endJumps.push_back((int)out.size());
            out.push_back(RpnOp::Jmp(0));

            // 回填假跳转目标：当前就是下一分支的起始位置
            out[jmpFalseIdx].intArg = (int)out.size();

            // ========== 处理 elif 链 ==========
            while (peek().kind == TOK_ELIF)
            {
                Token elifT = peek();
                consume(); // elif
                expect(TOK_LPAREN, "expected '(' after 'elif'");

                std::string elifCondType = parseExpression(env, out);
                if (elifCondType != "bool" && elifCondType != "int")
                {
                    err.Error(fileName, elifT.line, elifT.col, "elif condition",
                              "condition must be bool or int, got '" + elifCondType + "'",
                              getCurrentSourceLine());
                }

                expect(TOK_RPAREN, "expected ')' after elif condition");

                int elifJmpFalseIdx = (int)out.size();
                out.push_back(RpnOp::JmpIfFalse(0));

                parseBranchBody(env, out, retTypes, "elif");

                endJumps.push_back((int)out.size());
                out.push_back(RpnOp::Jmp(0));

                // 回填当前 elif 的假跳转地址
                out[elifJmpFalseIdx].intArg = (int)out.size();
            }

            // ========== 处理 else 分支 ==========
            if (peek().kind == TOK_ELSE)
            {
                consume(); // else
                parseBranchBody(env, out, retTypes, "else");
            }

            // ========== 统一回填所有结尾跳转 ==========
            int endPos = (int)out.size();
            for (int idx : endJumps)
            {
                out[idx].intArg = endPos;
            }

            return;
        }
        case TOK_FOR:
        {
            Token t = peek();
            consume(); // for
            expect(TOK_LPAREN, "expected '(' after 'for'");
            LocalEnv forEnv = env;
            std::vector<std::string> dummyRet;

            // 1 初始化子句
            if (peek().kind != TOK_SEMICOLON)
            {
                parseStatement(forEnv, out, dummyRet);
            }
            else
            {
                consume();
            }

            int condStartIdx = (int)out.size();

            // 2 条件子句
            std::string condType = "int";
            if (peek().kind != TOK_SEMICOLON)
            {
                condType = parseExpression(forEnv, out);
            }
            expect(TOK_SEMICOLON, "expected ';' after for condition");

            int jmpFalseIdx = (int)out.size();
            out.push_back(RpnOp::JmpIfFalse(0));

            // 3 增量子句
            RpnList incRpn;
            if (peek().kind != TOK_RPAREN)
            {
                if (peek().kind == TOK_IDENT && pos + 1 < (int)tokens.size() && tokens[pos + 1].kind == TOK_ASSIGN)
                {
                    std::string varName = consume().text;
                    consume();
                    parseExpression(forEnv, incRpn);
                    incRpn.push_back(RpnOp::StoreVar(varName));
                    incRpn.push_back(RpnOp::Pop());
                }
                else
                {
                    parseExpression(forEnv, incRpn);
                    incRpn.push_back(RpnOp::Pop());
                }
            }
            expect(TOK_RPAREN, "expected ')' after for clauses");
            expect(TOK_LBRACE, "expected '{' after for clauses");

            // ✅进入循环：压入一层循环标记
            loopStack.push_back(ForLoopMarker{});

            // 解析循环块，块内部parseStatement遇到jump会往back().patchList记录op下标
            parseBlock(forEnv, out, dummyRet);

            // 循环体结束，追加增量代码
            for (auto &op : incRpn)
                out.push_back(op);

            // 跳回条件判断开头
            out.push_back(RpnOp::Jmp(condStartIdx));

            // 循环真正出口地址
            int endPos = (int)out.size();
            out[jmpFalseIdx].intArg = endPos;

            // 回填本循环所有 jump(OP_JUMP_BREAK) 的目标地址
            ForLoopMarker curMarker = loopStack.back();
            loopStack.pop_back(); // 出栈

            for (int opIdx : curMarker.patchList)
            {
                out[opIdx].intArg = endPos; // jump直接跳到循环结束，不执行增量
            }
            return;
        }

        case TOK_ARRAY:
        {
            consume(); // 吃掉 array 关键字
            bool dummyConst = false, dummyRef = false;
            std::string elemTypeStr = parseType(dummyConst, dummyRef);

            Token nameTok = consume();
            if (nameTok.kind != TOK_IDENT)
            {
                err.Error(fileName, nameTok.line, nameTok.col, nameTok.text,
                          "expected array name", getCurrentSourceLine());
                while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                    consume();
                if (peek().kind == TOK_SEMICOLON)
                    consume();
                return;
            }
            std::string arrName = nameTok.text;

            // 尺寸、初始化与变量注册由 array/alloc 共用
            parseArrayDeclRest(env, out, arrName, nameTok, elemTypeStr);
            return;
        }

        case TOK_ALLOC:
        {
            consume(); // alloc

            Token nameTok = peek();
            if (nameTok.kind != TOK_IDENT)
            {
                err.Error(fileName, nameTok.line, nameTok.col, nameTok.text,
                          "expected variable name after 'alloc'", getCurrentSourceLine());
                while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                    consume();
                if (peek().kind == TOK_SEMICOLON)
                    consume();
                return;
            }
            std::string arrName = consume().text;

            if (peek().kind != TOK_SHL)
            {
                err.Error(fileName, peek().line, peek().col, peek().text,
                          "expected '<<' after alloc variable name", getCurrentSourceLine());
                while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                    consume();
                if (peek().kind == TOK_SEMICOLON)
                    consume();
                return;
            }
            consume(); // <<

            bool dummyConst = false, dummyRef = false;
            std::string elemTypeStr = parseType(dummyConst, dummyRef);

            parseArrayDeclRest(env, out, arrName, nameTok, elemTypeStr);
            return;
        }

        case TOK_ASSIGN_ARR:
        {
            consume(); // assign
            expect(TOK_LESS, "expected '<' after 'assign'");

            Token aTok = peek();
            if (aTok.kind != TOK_IDENT)
            {
                err.Error(fileName, aTok.line, aTok.col, aTok.text,
                          "expected array name", getCurrentSourceLine());
                while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                    consume();
                if (peek().kind == TOK_SEMICOLON)
                    consume();
                return;
            }
            consume();

            expect(TOK_COMMA, "expected ',' between array names");

            Token bTok = peek();
            if (bTok.kind != TOK_IDENT)
            {
                err.Error(fileName, bTok.line, bTok.col, bTok.text,
                          "expected array name", getCurrentSourceLine());
                while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                    consume();
                if (peek().kind == TOK_SEMICOLON)
                    consume();
                return;
            }
            consume();

            expect(TOK_GREATER, "expected '>' after array names");
            expect(TOK_SEMICOLON, "expected ';' after 'assign'");

            auto ai = env.find(aTok.text);
            auto bi = env.find(bTok.text);
            bool aIsArray = false, bIsArray = false;

            auto lineOf = [&](int ln) -> std::string
            {
                if (ln >= 0 && (size_t)ln < sourceLines.size())
                    return sourceLines[ln];
                return "";
            };
            std::string aLine = lineOf(aTok.line);
            std::string bLine = lineOf(bTok.line);

            if (ai == env.end())
            {
                err.Error(fileName, aTok.line, aTok.col, aTok.text,
                          "undefined identifier", aLine);
            }
            else if (ai->second.type != Value::ARRAY)
            {
                err.Error(fileName, aTok.line, aTok.col, aTok.text,
                          "'" + aTok.text + "' is not an array", aLine);
            }
            else
            {
                aIsArray = true;
            }

            if (bi == env.end())
            {
                err.Error(fileName, bTok.line, bTok.col, bTok.text,
                          "undefined identifier", bLine);
            }
            else if (bi->second.type != Value::ARRAY)
            {
                err.Error(fileName, bTok.line, bTok.col, bTok.text,
                          "'" + bTok.text + "' is not an array", bLine);
            }
            else
            {
                bIsArray = true;
            }

            if (aIsArray && bIsArray)
            {
                if (ai->second.isConst)
                {
                    err.Error(fileName, aTok.line, aTok.col, aTok.text,
                              "cannot assign to const variable '" + aTok.text + "'", aLine);
                }

                if (ai->second.arrayDepth != bi->second.arrayDepth)
                {
                    err.Error(fileName, aTok.line, aTok.col, aTok.text,
                              "cannot assign a " + std::to_string(bi->second.arrayDepth) +
                                  "-dimensional array to " + std::to_string(ai->second.arrayDepth) +
                                  "-dimensional '" + aTok.text + "'",
                              aLine);
                }
                else if (ai->second.elemType != bi->second.elemType)
                {
                    err.Error(fileName, aTok.line, aTok.col, aTok.text,
                              "cannot assign array of '" + basicTypeName(bi->second.elemType) +
                                  "' to '" + basicTypeName(ai->second.elemType) + "'",
                              aLine);
                }
            }

            // b 的数组值整体深拷贝给 a，长度随 b 改变
            out.push_back(RpnOp::LoadVar(bTok.text));
            out.push_back(RpnOp::StoreVar(aTok.text));
            return;
        }

        case TOK_TANT:
        {
            consume(); // 吃掉 tant
            expect(TOK_LESS, "expected '<' after 'tant'");

            // ========== 1. 解析左值（支持变量/数组/对象成员） ==========
            Token lvalTok = peek();
            std::string lvalKind; // "var" / "array" / "member"
            std::string lvalName;
            std::string lvalMember;
            RpnList lvalIndexRpn; // 数组下标表达式

            if (peek().kind == TOK_IDENT && pos + 1 < (int)tokens.size() && tokens[pos + 1].kind == TOK_LBRACKET)
            {
                // 数组左值 arr[idx]
                lvalKind = "array";
                lvalName = consume().text;
                expect(TOK_LBRACKET, "expected '['");
                parseAddSub(env, lvalIndexRpn);
                expect(TOK_RBRACKET, "expected ']'");
            }
            else if ((peek().kind == TOK_IDENT || peek().kind == TOK_THIS) && pos + 2 < (int)tokens.size() && tokens[pos + 1].kind == TOK_DOT && tokens[pos + 2].kind == TOK_IDENT)
            {
                // 对象成员左值 obj.member / this.member
                lvalKind = "member";
                bool isThis = (peek().kind == TOK_THIS);
                lvalName = consume().text;
                consume(); // 吃掉 .
                lvalMember = consume().text;
            }
            else if (peek().kind == TOK_IDENT)
            {
                // 普通变量左值
                lvalKind = "var";
                lvalName = consume().text;
            }
            else
            {
                err.Error(fileName, lvalTok.line, lvalTok.col, lvalTok.text,
                          "expected assignable left value", getCurrentSourceLine());
                // 容错跳过
                while (peek().kind != TOK_GREATER && peek().kind != TOK_EOF)
                    consume();
                if (peek().kind == TOK_GREATER)
                    consume();
                expect(TOK_SEMICOLON, "expected ';'");
                return;
            }

            expect(TOK_COMMA, "expected ','");

            // ========== 2. 解析目标值表达式 ==========
            RpnList targetRpn;
            std::string targetType = parseAddSub(env, targetRpn);

            expect(TOK_COMMA, "expected ','");

            // ========== 3. 解析次数表达式 ==========
            RpnList countRpn;
            std::string countType = parseAddSub(env, countRpn);
            if (countType != "int")
            {
                err.Error(fileName, peek().line, peek().col, "count",
                          "count must be integer", getCurrentSourceLine());
            }

            expect(TOK_GREATER, "expected '>'");
            expect(TOK_SEMICOLON, "expected ';' after tant statement");

            // ========== 4. 生成等价循环RPN指令 ==========
            std::string counterVar = "__tant_counter_" + std::to_string((int)out.size());

            // 初始化计数器 0
            out.push_back(RpnOp::PushInt(0));
            out.push_back(RpnOp::StoreVar(counterVar));
            Value counterVal{};
            counterVal.type = Value::INT;
            env[counterVar] = counterVal;

            // 循环条件起始位置
            int condStart = (int)out.size();

            // 条件：counter < count
            out.push_back(RpnOp::LoadVar(counterVar));
            for (auto &op : countRpn)
                out.push_back(op);
            out.push_back(RpnOp::BinOp(OP_LT));

            // 条件假跳转
            int jmpFalseIdx = (int)out.size();
            out.push_back(RpnOp::JmpIfFalse(0));

            // ========== 循环体：v = (v + target) / 2 ==========
            // 加载左值当前值
            if (lvalKind == "var")
                out.push_back(RpnOp::LoadVar(lvalName));
            else if (lvalKind == "array")
            {
                out.push_back(RpnOp::LoadVar(lvalName));
                for (auto &op : lvalIndexRpn)
                    out.push_back(op);
                out.push_back(RpnOp::ArrayLoad());
            }
            else if (lvalKind == "member")
            {
                if (lvalName == "this")
                    out.push_back(RpnOp::PushThis());
                else
                    out.push_back(RpnOp::LoadVar(lvalName));
                out.push_back(RpnOp::MemberLoad(lvalMember));
            }

            // 加上目标值
            for (auto &op : targetRpn)
                out.push_back(op);
            out.push_back(RpnOp::BinOp(OP_ADD));

            // 除以 2
            out.push_back(RpnOp::PushInt(2));
            out.push_back(RpnOp::BinOp(OP_DIV));

            // 写回左值
            if (lvalKind == "var")
            {
                out.push_back(RpnOp::StoreVar(lvalName));
            }
            else if (lvalKind == "array")
            {
                // 新值当前在栈顶：先存到临时变量，再取数组和下标写入
                std::string tmpVal = "__tant_val_" + std::to_string((int)out.size());
                out.push_back(RpnOp::StoreVar(tmpVal));
                Value tmpV{};
                tmpV.type = Value::FLOAT;
                env[tmpVal] = tmpV;

                out.push_back(RpnOp::LoadVar(lvalName));
                for (auto &op : lvalIndexRpn)
                    out.push_back(op);
                out.push_back(RpnOp::LoadVar(tmpVal));
                out.push_back(RpnOp::ArrayStore());
                out.push_back(RpnOp::StoreVar(lvalName));
            }
            else if (lvalKind == "member")
            {
                // 成员赋值同理
                std::string tmpVal = "__tant_val_" + std::to_string((int)out.size());
                out.push_back(RpnOp::StoreVar(tmpVal));
                Value tmpV{};
                tmpV.type = Value::FLOAT;
                env[tmpVal] = tmpV;

                if (lvalName == "this")
                    out.push_back(RpnOp::PushThis());
                else
                    out.push_back(RpnOp::LoadVar(lvalName));
                out.push_back(RpnOp::LoadVar(tmpVal));
                out.push_back(RpnOp::MemberStore(lvalMember));
            }

            out.push_back(RpnOp::Pop());

            // 计数器 +1
            out.push_back(RpnOp::LoadVar(counterVar));
            out.push_back(RpnOp::PushInt(1));
            out.push_back(RpnOp::BinOp(OP_ADD));
            out.push_back(RpnOp::StoreVar(counterVar));
            out.push_back(RpnOp::Pop());

            // 跳回条件
            out.push_back(RpnOp::Jmp(condStart));

            // 回填假跳转地址
            out[jmpFalseIdx].intArg = (int)out.size();

            return;
        }

        default:
        {
            // 数组下标赋值：arr[i] = value;  arr[i][j] = value;
            if (peek().kind == TOK_IDENT && pos + 2 < (int)tokens.size() && tokens[pos + 1].kind == TOK_LBRACKET)
            {
                Token nameTok = peek();
                std::string arrName = consume().text;

                auto vit = env.find(arrName);
                std::string baseType;
                int depth = 0;
                if (vit == env.end())
                {
                    err.Error(fileName, nameTok.line, nameTok.col, arrName,
                              "undefined identifier", getCurrentSourceLine());
                }
                else if (vit->second.type != Value::ARRAY)
                {
                    err.Error(fileName, nameTok.line, nameTok.col, arrName,
                              "'" + arrName + "' is not an array", getCurrentSourceLine());
                }
                else
                {
                    baseType = basicTypeName(vit->second.elemType);
                    depth = vit->second.arrayDepth;
                }

                out.push_back(RpnOp::LoadVar(arrName)); // 加载数组副本

                int idxCount = 0;
                while (peek().kind == TOK_LBRACKET)
                {
                    consume();
                    std::string idxType = parseExpression(env, out);
                    if (idxType != "int")
                    {
                        Token badIdx = peek();
                        err.Error(fileName, badIdx.line, badIdx.col, idxType,
                                  "array index must be integer", getCurrentSourceLine());
                    }
                    expect(TOK_RBRACKET, "expected ']'");
                    idxCount++;
                }

                expect(TOK_ASSIGN, "expected '='");

                Token valTok = peek();
                std::string valType = parseExpression(env, out); // 右值

                if (depth > 0)
                {
                    bool typeOk = (valType == baseType) ||
                                  (isNumericTypeName(valType) && isNumericTypeName(baseType)) ||
                                  (valType == "vd");
                    if (idxCount > depth)
                        err.Error(fileName, nameTok.line, nameTok.col, arrName,
                                  "too many subscripts for array '" + arrName + "'",
                                  getCurrentSourceLine());
                    else if (idxCount < depth)
                        err.Error(fileName, nameTok.line, nameTok.col, arrName,
                                  "cannot assign to a sub-array; use all " + std::to_string(depth) + " subscripts",
                                  getCurrentSourceLine());
                    else if (!typeOk)
                        err.Error(fileName, valTok.line, valTok.col, valType,
                                  "array element type mismatch: expected '" + baseType + "', got '" + valType + "'",
                                  getCurrentSourceLine());
                }

                out.push_back(RpnOp::ArrayStoreN(idxCount)); // 修改栈上的副本
                out.push_back(RpnOp::StoreVar(arrName));     // 写回原变量
                expect(TOK_SEMICOLON, "expected ';' after assignment");
                return;
            }

            // ===== 成员数组下标赋值：this.arr[i] = value / obj.arr[i] = value =====
            if ((peek().kind == TOK_IDENT || peek().kind == TOK_THIS) &&
                pos + 4 < (int)tokens.size() &&
                tokens[pos + 1].kind == TOK_DOT &&
                tokens[pos + 2].kind == TOK_IDENT &&
                tokens[pos + 3].kind == TOK_LBRACKET)
            {
                bool isThis = (peek().kind == TOK_THIS);
                Token objTok = peek();
                std::string objName = isThis ? std::string("this") : objTok.text;
                consume(); // obj / this

                std::string objType;
                if (isThis)
                {
                    objType = currentClass;
                    if (objType.empty())
                    {
                        err.Error(fileName, objTok.line, objTok.col, "this",
                                  "'this' used outside class method", getCurrentSourceLine());
                    }
                }
                else
                {
                    auto vit = env.find(objName);
                    if (vit == env.end())
                    {
                        err.Error(fileName, objTok.line, objTok.col, objName,
                                  "undefined identifier", getCurrentSourceLine());
                    }
                    else if (vit->second.type != Value::OBJECT)
                    {
                        err.Error(fileName, objTok.line, objTok.col, objName,
                                  "'" + objName + "' is not an object", getCurrentSourceLine());
                    }
                    else
                    {
                        objType = vit->second.objectClass;
                    }
                }

                consume(); // .
                Token memTok = consume();
                std::string memName = memTok.text;

                ClassInfo *cls = lookupClass(objType);
                Value::Type elemType = Value::INT;
                std::string elemTypeName = "int";
                bool memberOk = false;

                if (cls)
                {
                    auto mit = cls->members.find(memName);
                    if (mit == cls->members.end())
                    {
                        err.Error(fileName, memTok.line, memTok.col, memName,
                                  "class '" + cls->name + "' has no member '" + memName + "'",
                                  getCurrentSourceLine());
                    }
                    else if (!mit->second.isArray)
                    {
                        err.Error(fileName, memTok.line, memTok.col, memName,
                                  "member '" + memName + "' is not an array",
                                  getCurrentSourceLine());
                    }
                    else
                    {
                        memberOk = true;
                        elemType = mit->second.elemType;
                        elemTypeName = basicTypeName(elemType);
                        if (!canAccess(cls->name, mit->second.access))
                        {
                            err.Error(fileName, memTok.line, memTok.col, memName,
                                      "cannot access private member '" + memName +
                                          "' of class '" + cls->name + "'",
                                      getCurrentSourceLine());
                        }
                    }
                }
                else if (!objType.empty())
                {
                    err.Error(fileName, memTok.line, memTok.col, memName,
                              "type '" + objType + "' has no members", getCurrentSourceLine());
                }

                // 需要两份对象：一份给 MemberLoad 取出数组，一份留给 MemberStore 写回
                if (isThis)
                {
                    out.push_back(RpnOp::PushThis());
                    out.push_back(RpnOp::PushThis());
                }
                else
                {
                    out.push_back(RpnOp::LoadVar(objName));
                    out.push_back(RpnOp::LoadVar(objName));
                }

                out.push_back(RpnOp::MemberLoad(memName));

                expect(TOK_LBRACKET, "expected '['");
                std::string idxType = parseExpression(env, out);
                if (idxType != "int")
                {
                    err.Error(fileName, peek().line, peek().col, idxType,
                              "array index must be integer", getCurrentSourceLine());
                }
                expect(TOK_RBRACKET, "expected ']'");

                expect(TOK_ASSIGN, "expected '='");

                Token valTok = peek();
                std::string valType = parseExpression(env, out);
                if (memberOk && valType != elemTypeName &&
                    !(isNumericTypeName(valType) && isNumericTypeName(elemTypeName)))
                {
                    err.Error(fileName, valTok.line, valTok.col, valType,
                              "array element type mismatch: expected '" + elemTypeName +
                                  "', got '" + valType + "'",
                              getCurrentSourceLine());
                }

                out.push_back(RpnOp::ArrayStoreN(1));
                out.push_back(RpnOp::MemberStore(memName));
                if (isThis)
                    out.push_back(RpnOp::Pop());
                else
                    out.push_back(RpnOp::StoreVar(objName));

                expect(TOK_SEMICOLON, "expected ';' after assignment");
                return;
            }

            // ===== 对象成员赋值 obj.member = value / this.member = value =====
            if ((peek().kind == TOK_IDENT || peek().kind == TOK_THIS) && pos + 3 < (int)tokens.size() && tokens[pos + 1].kind == TOK_DOT && tokens[pos + 2].kind == TOK_IDENT && tokens[pos + 3].kind == TOK_ASSIGN)
            {
                bool isThis = (peek().kind == TOK_THIS);
                Token objTok = peek();
                std::string objName = isThis ? std::string("this") : objTok.text;
                consume(); // 对象名 / this

                if (isThis && currentClass.empty())
                {
                    err.Error(fileName, objTok.line, objTok.col, "this",
                              "'this' used outside class method", getCurrentSourceLine());
                }

                // 对象的静态类型
                std::string objType = isThis ? currentClass : "";
                if (!isThis)
                {
                    auto vit = env.find(objName);
                    if (vit == env.end())
                    {
                        err.Error(fileName, objTok.line, objTok.col, objName,
                                  "undefined identifier", getCurrentSourceLine());
                    }
                    else if (vit->second.type != Value::OBJECT)
                    {
                        err.Error(fileName, objTok.line, objTok.col, objName,
                                  "'" + objName + "' is not an object", getCurrentSourceLine());
                    }
                    else
                    {
                        objType = vit->second.objectClass;
                    }
                }

                consume(); // 吃掉 .
                Token memTok = consume();
                std::string memName = memTok.text;

                // 成员存在性与访问权限
                ClassInfo *cls = lookupClass(objType);
                std::string memType;
                bool memberOk = false;
                if (!cls)
                {
                    if (!objType.empty())
                    {
                        err.Error(fileName, memTok.line, memTok.col, memName,
                                  "type '" + objType + "' has no members", getCurrentSourceLine());
                    }
                }
                else
                {
                    auto mit = cls->members.find(memName);
                    if (mit == cls->members.end())
                    {
                        err.Error(fileName, memTok.line, memTok.col, memName,
                                  "class '" + cls->name + "' has no member '" + memName + "'",
                                  getCurrentSourceLine());
                    }
                    else
                    {
                        memberOk = true;
                        memType = mit->second.type;
                        if (!canAccess(cls->name, mit->second.access))
                        {
                            err.Error(fileName, memTok.line, memTok.col, memName,
                                      "cannot access private member '" + memName + "' of class '" + cls->name + "'",
                                      getCurrentSourceLine());
                        }
                    }
                }

                // 压入对象（this 压到的是引用描述符，写成员会直接改到原对象）
                if (isThis)
                    out.push_back(RpnOp::PushThis());
                else
                    out.push_back(RpnOp::LoadVar(objName));

                expect(TOK_ASSIGN, "expected '='");

                Token valTok = peek();
                std::string valType = parseExpression(env, out); // 解析右值表达式

                if (memberOk)
                {
                    bool typeOk = (valType == memType) ||
                                  (isNumericTypeName(valType) && isNumericTypeName(memType)) ||
                                  (valType == "vd");
                    if (!typeOk)
                    {
                        err.Error(fileName, valTok.line, valTok.col, valType,
                                  "member type mismatch: expected '" + memType + "', got '" + valType + "'",
                                  getCurrentSourceLine());
                    }
                }

                out.push_back(RpnOp::MemberStore(memName));
                if (isThis)
                    out.push_back(RpnOp::Pop()); // this 已经通过引用改到原对象
                else
                    out.push_back(RpnOp::StoreVar(objName)); // 普通变量需要把改过的副本写回

                expect(TOK_SEMICOLON, "expected ';' after assignment");
                return;
            }
            if (peek().kind == TOK_IDENT && pos + 1 < (int)tokens.size() && tokens[pos + 1].kind == TOK_ASSIGN)
            {
                Token varTok = peek();
                std::string varName = consume().text;

                auto vit = env.find(varName);
                if (vit == env.end())
                {
                    std::string msg = "undefined identifier";
                    if (!currentClass.empty())
                    {
                        ClassInfo *cc = lookupClass(currentClass);
                        if (cc && cc->members.count(varName))
                        {
                            msg = "member '" + varName + "' must be accessed as 'this." +
                                  varName + "'";
                        }
                    }
                    err.Error(fileName, varTok.line, varTok.col, varName, msg, getCurrentSourceLine());
                }
                else if (vit->second.isConst)
                {
                    err.Error(fileName, varTok.line, varTok.col, varName,
                              "cannot assign to const variable '" + varName + "'",
                              getCurrentSourceLine());
                }

                expect(TOK_ASSIGN, "expected '='");
                parseExpression(env, out); // 解析右值表达式
                out.push_back(RpnOp::StoreVar(varName));
                expect(TOK_SEMICOLON, "expected ';' after assignment");
                out.push_back(RpnOp::Pop());
                return;
            }
            // 普通表达式语句
            parseExpression(env, out);
            expect(TOK_SEMICOLON, "expected ';'");
            out.push_back(RpnOp::Pop());
            return;
        }
        }
        if (pos == oldPos && peek().kind != TOK_EOF)
        {
            consume();
        }
    }

    // include 指令识别
    inline void Parser::parseProgram(RpnList &mainOut)
    {
        LocalEnv topEnv;
        while (peek().kind != TOK_EOF)
        {
            // 遇到错误立刻停止解析，不继续爆连锁错误
            if (!err.fileName.empty())
                break;

            if (peek().kind == TOK_IDENT && peek().text == "include")
            {
                handleInclude(mainOut);
                continue;
            }
            if (peek().kind == TOK_CLASS)
            {
                parseClassDecl();
                continue;
            }

            TokenKind cur = peek().kind;
            bool isFuncDecl = (cur == TOK_FUNCTION);
            if (isFuncDecl)
            {
                parseFunctionDecl();
            }
            else
            {
                std::vector<std::string> dummyRet;
                parseStatement(topEnv, mainOut, dummyRet);
            }
        }
    }

    // 解析调用实参（函数/方法/operator() 共用）：支持 ref 形参，并做个数/类型检查
    // 解析调用实参（不涉及 ref / 重载检查，operator() 用）
    inline void Parser::parseArgsGeneric(LocalEnv &env, RpnList &out, std::vector<std::string> &argTypes)
    {
        argTypes.clear();
        expect(TOK_LPAREN, "expected '('");
        if (peek().kind != TOK_RPAREN)
        {
            while (true)
            {
                argTypes.push_back(parseExpression(env, out));
                if (peek().kind == TOK_COMMA)
                    consume();
                else
                    break;
            }
        }
        expect(TOK_RPAREN, "expected ')'");
    }

    // 按符号 + 实参类型挑选运算符重载，返回 mangled 方法名；失败时记录错误并返回空串
    inline std::string Parser::resolveOperatorOverload(ClassInfo *cls, const std::string &symbol,
                                                       const std::vector<std::string> &argTypes,
                                                       const Token &tok, const std::string &opText)
    {
        if (!cls)
            return "";

        auto oit = cls->operatorOverloads.find(symbol);
        if (oit == cls->operatorOverloads.end())
        {
            err.Error(fileName, tok.line, tok.col, opText,
                      "class '" + cls->name + "' has no '" + opText + "' operator",
                      getCurrentSourceLine());
            return "";
        }

        for (auto &mangled : oit->second)
        {
            auto fit = cls->methods.find(mangled);
            if (fit == cls->methods.end())
                continue;
            FuncInfo &f = fit->second;
            if (f.params.size() != argTypes.size())
                continue;

            bool ok = true;
            for (size_t i = 0; i < argTypes.size(); i++)
            {
                const std::string &pt = f.params[i].type;
                if (pt == "any" || pt == "auto")
                    continue;
                if (pt != argTypes[i])
                {
                    ok = false;
                    break;
                }
            }
            if (!ok)
                continue;

            auto ait = cls->methodAccess.find(mangled);
            Access acc = (ait != cls->methodAccess.end()) ? ait->second : Access::Private;
            if (!canAccess(cls->name, acc))
            {
                err.Error(fileName, tok.line, tok.col, opText,
                          "cannot access private operator '" + opText + "' of class '" + cls->name + "'",
                          getCurrentSourceLine());
            }
            return mangled;
        }

        std::string got;
        for (size_t i = 0; i < argTypes.size(); i++)
        {
            if (i)
                got += ", ";
            got += argTypes[i];
        }
        err.Error(fileName, tok.line, tok.col, opText,
                  "no matching overload for '" + opText + "' of class '" + cls->name +
                      "' with (" + got + ")",
                  getCurrentSourceLine());
        return "";
    }

    // 按实参类型挑选构造函数重载；返回空串表示使用隐式默认构造（类没有声明构造函数）
    inline std::string Parser::resolveConstructor(ClassInfo *cls, const std::vector<std::string> &argTypes,
                                                  const Token &tok, const std::string &classNameText)
    {
        if (!cls)
            return "";

        auto joinArgs = [&]() -> std::string
        {
            std::string got;
            for (size_t i = 0; i < argTypes.size(); i++)
            {
                if (i)
                    got += ", ";
                got += argTypes[i];
            }
            return got;
        };

        if (cls->constructors.empty())
        {
            // 没有用户构造函数：允许 new P() / var P p;，带参数则报错
            if (!argTypes.empty())
            {
                err.Error(fileName, tok.line, tok.col, classNameText,
                          "class '" + cls->name + "' has no constructor taking (" + joinArgs() + ")",
                          getCurrentSourceLine());
            }
            return "";
        }

        for (auto &mangled : cls->constructors)
        {
            auto fit = cls->methods.find(mangled);
            if (fit == cls->methods.end())
                continue;
            FuncInfo &f = fit->second;
            if (f.params.size() != argTypes.size())
                continue;

            bool ok = true;
            for (size_t i = 0; i < argTypes.size(); i++)
            {
                const std::string &pt = f.params[i].type;
                if (pt == "any" || pt == "auto")
                    continue;
                if (pt != argTypes[i])
                {
                    ok = false;
                    break;
                }
            }
            if (!ok)
                continue;

            auto ait = cls->methodAccess.find(mangled);
            Access acc = (ait != cls->methodAccess.end()) ? ait->second : Access::Private;
            if (!canAccess(cls->name, acc))
            {
                err.Error(fileName, tok.line, tok.col, classNameText,
                          "cannot access private constructor of class '" + cls->name + "'",
                          getCurrentSourceLine());
            }
            return mangled;
        }

        err.Error(fileName, tok.line, tok.col, classNameText,
                  "no matching constructor for '" + cls->name + "' with (" + joinArgs() + ")",
                  getCurrentSourceLine());
        return "";
    }

    inline void Parser::parseCallArgs(FuncInfo *m, LocalEnv &env, RpnList &out, int &argCount,
                                      std::vector<std::string> &argTypes,
                                      const std::string &callName, const Token &nameTok)
    {
        argCount = 0;
        argTypes.clear();

        expect(TOK_LPAREN, "expected '('");
        if (peek().kind != TOK_RPAREN)
        {
            while (true)
            {
                bool isRefParam = (m && argCount < (int)m->params.size() && m->params[argCount].isRef);
                if (isRefParam)
                {
                    Token argTok = peek();
                    if (argTok.kind != TOK_IDENT)
                    {
                        err.Error(fileName, argTok.line, argTok.col, argTok.text,
                                  "ref argument must be a variable", getCurrentSourceLine());
                        parseExpression(env, out);
                        argTypes.push_back(m->params[argCount].type);
                    }
                    else
                    {
                        std::string varName = consume().text;
                        auto vit = env.find(varName);
                        if (vit == env.end())
                        {
                            err.Error(fileName, argTok.line, argTok.col, varName,
                                      "undefined identifier", getCurrentSourceLine());
                            argTypes.push_back(m->params[argCount].type);
                        }
                        else
                        {
                            if (vit->second.isConst)
                            {
                                err.Error(fileName, argTok.line, argTok.col, varName,
                                          "cannot pass const variable '" + varName + "' as ref argument",
                                          getCurrentSourceLine());
                            }
                            argTypes.push_back(valueTypeName(vit->second));
                        }
                        out.push_back(RpnOp::PushRef(varName));
                    }
                }
                else
                {
                    argTypes.push_back(parseExpression(env, out));
                }

                argCount++;
                if (peek().kind == TOK_COMMA)
                    consume();
                else
                    break;
            }
        }
        expect(TOK_RPAREN, "expected ')'");

        if (m)
        {
            if (argCount != (int)m->params.size())
            {
                err.Error(fileName, nameTok.line, nameTok.col, callName,
                          "wrong number of arguments: expected " + std::to_string(m->params.size()) +
                              ", got " + std::to_string(argCount),
                          getCurrentSourceLine());
            }
            else
            {
                for (int i = 0; i < argCount; i++)
                {
                    std::string paramType = m->params[i].type;
                    if (paramType == "any" || paramType == "auto")
                        continue;
                    if (argTypes[i] != paramType)
                    {
                        err.Error(fileName, nameTok.line, nameTok.col, callName,
                                  "argument type mismatch: expected '" + paramType +
                                      "', got '" + argTypes[i] + "'",
                                  getCurrentSourceLine());
                        break;
                    }
                }
            }
        }
    }

    inline std::string Parser::parseFactor(LocalEnv &env, RpnList &out)
    {
        Token t = peek();
        if (t.kind == TOK_BANG)
        {
            consume();
            // 递归解析后面的因子（支持 !!a 双重取反）
            std::string subType = parseFactor(env, out);
            // 生成OP_NOT指令（需要先在RpnOpKind枚举新增OP_NOT）
            out.push_back(RpnOp::BinOp(OP_NOT));
            // 逻辑非运算结果永远是bool
            return "bool";
        }

        std::string type;
        std::string elemTypeStr;
        std::string identName;
        int arrDepthLeft = 0; // 数组还剩几维

        if (t.kind == TOK_NUMBER)
        {
            consume();
            out.push_back(RpnOp::PushInt(std::stoi(t.text)));
            type = "int";
        }
        else if (t.kind == TOK_TRUE || t.kind == TOK_FALSE)
        {
            bool val = (t.kind == TOK_TRUE);
            consume();
            out.push_back(RpnOp::PushBool(val));
            type = "bool";
        }
        else if (t.kind == TOK_STRLIT)
        {
            consume();
            out.push_back(RpnOp::PushStr(t.text));
            type = "string";
        }
        else if (t.kind == TOK_CHAR_LIT) //   char字面量 'a'
        {
            Token tk = consume();
            char c = tk.text[0];
            out.push_back(RpnOp::PushChar(c));
            type = "char";
        }
        else if (t.kind == TOK_LPAREN)
        {
            consume();
            type = parseExpression(env, out);
            expect(TOK_RPAREN, "expected ')'");
        }
        else if (t.kind == TOK_VD)
        {
            out.push_back(RpnOp::PushVd());
            // 表达式返回特殊类型字符串"vd"
            return "vd";
        }
        else if (t.kind == TOK_THIS)
        {
            consume();
            if (currentClass.empty())
            {
                err.Error(fileName, t.line, t.col, "this",
                          "'this' used outside class method", getCurrentSourceLine());
            }
            out.push_back(RpnOp::PushThis());
            type = currentClass.empty() ? "object" : currentClass;

            // this(...)：类自身的 operator()
            if (peek().kind == TOK_LPAREN)
            {
                std::vector<std::string> argTypes;
                parseArgsGeneric(env, out, argTypes);

                ClassInfo *cls = currentClass.empty() ? nullptr : lookupClass(currentClass);
                std::string mangled = resolveOperatorOverload(cls, "()", argTypes, t, "()");
                if (!mangled.empty())
                {
                    RpnOp callOp = RpnOp::Call(mangled, (int)argTypes.size());
                    callOp.boolArg = true;
                    out.push_back(callOp);
                    type = cls->methods[mangled].retType;
                }
                else
                {
                    type = "void";
                }
            }
        }
        else if (t.kind == TOK_FLOAT)
        {
            consume();
            out.push_back(RpnOp::PushFloat(std::stof(t.text)));
            type = "float";
        }
        else if (t.kind == TOK_NEW)
        {
            consume();
            bool dc = false, dr = false;
            Token typeTok = peek();
            size_t errBefore = err.why.size();
            std::string className = parseType(dc, dr); // 支持 T 与 Box<int>

            if (err.why.size() == errBefore && !lookupClass(className) &&
                isBuiltinTypeName(className))
            {
                err.Error(fileName, typeTok.line, typeTok.col, typeTok.text,
                          "cannot instantiate non-class type '" + className + "'",
                          getCurrentSourceLine());
            }

            // 构造实参先解析到临时 RPN，保证运行时顺序是 [对象, 参数...]
            RpnList argRpn;
            std::vector<std::string> argTypes;
            if (peek().kind == TOK_LPAREN)
                parseArgsGeneric(env, argRpn, argTypes);

            ClassInfo *ctorCls = lookupClass(className);
            std::string ctorName;
            if (ctorCls)
                ctorName = resolveConstructor(ctorCls, argTypes, t, className);

            out.push_back(RpnOp::NewObject(className));
            for (auto &op : argRpn)
                out.push_back(op);
            if (!ctorName.empty())
                out.push_back(RpnOp::CallCtor(ctorName, (int)argTypes.size()));
            type = className;
        }

        else if (t.kind == TOK_IDENT)
        {
            identName = consume().text;
            if (peek().kind == TOK_LPAREN)
            {
                // 对象调用 operator()：obj(args)
                bool handledAsObject = false;
                auto vit = env.find(identName);
                if (vit != env.end() && vit->second.type == Value::OBJECT)
                {
                    handledAsObject = true;
                    out.push_back(RpnOp::PushObjRef(identName));

                    std::vector<std::string> argTypes;
                    parseArgsGeneric(env, out, argTypes);

                    ClassInfo *opCls = lookupClass(vit->second.objectClass);
                    std::string mangled = resolveOperatorOverload(opCls, "()", argTypes, t, "()");
                    if (!mangled.empty())
                    {
                        RpnOp callOp = RpnOp::Call(mangled, (int)argTypes.size());
                        callOp.boolArg = true;
                        out.push_back(callOp);
                        type = opCls->methods[mangled].retType;
                    }
                    else
                    {
                        type = "void";
                    }
                }

                if (!handledAsObject)
                {
                    FuncInfo *finfo = lookupFunc(identName);
                    if (!finfo)
                    {
                        std::string msg = "undefined function";
                        if (!currentClass.empty())
                        {
                            ClassInfo *cc = lookupClass(currentClass);
                            if (cc && cc->methods.count(identName))
                            {
                                msg = "method '" + identName + "' must be called as 'this." +
                                      identName + "(...)'";
                            }
                        }
                        err.Error(fileName, t.line, t.col, identName, msg, getCurrentSourceLine());
                        expect(TOK_LPAREN, "expected '('");
                        while (peek().kind != TOK_RPAREN && peek().kind != TOK_EOF)
                            consume();
                        expect(TOK_RPAREN, "expected ')'");
                        type = "void";
                    }
                    else
                    {
                        expect(TOK_LPAREN, "expected '('");
                        int argCount = 0;
                        std::vector<std::string> argTypes;
                        if (peek().kind != TOK_RPAREN)
                        {
                            while (true)
                            {
                                bool isRefParam = false;
                                if (finfo && argCount < (int)finfo->params.size())
                                    isRefParam = finfo->params[argCount].isRef;

                                if (isRefParam)
                                {
                                    // ref 参数必须传一个已声明的变量，类型取该变量的真实类型
                                    Token argTok = peek();
                                    if (argTok.kind != TOK_IDENT)
                                    {
                                        err.Error(fileName, argTok.line, argTok.col, argTok.text,
                                                  "ref argument must be a variable", getCurrentSourceLine());
                                        parseExpression(env, out);
                                        // 已经报错，用形参类型占位，避免再叠加一条类型不匹配
                                        argTypes.push_back(finfo->params[argCount].type);
                                    }
                                    else
                                    {
                                        std::string varName = consume().text;
                                        auto vit = env.find(varName);
                                        if (vit == env.end())
                                        {
                                            err.Error(fileName, argTok.line, argTok.col, varName,
                                                      "undefined identifier", getCurrentSourceLine());
                                            argTypes.push_back(finfo->params[argCount].type);
                                        }
                                        else
                                        {
                                            if (vit->second.isConst)
                                            {
                                                err.Error(fileName, argTok.line, argTok.col, varName,
                                                          "cannot pass const variable '" + varName + "' as ref argument",
                                                          getCurrentSourceLine());
                                            }
                                            argTypes.push_back(valueTypeName(vit->second));
                                        }
                                        out.push_back(RpnOp::PushRef(varName));
                                    }
                                }
                                else
                                {
                                    std::string argType = parseExpression(env, out);
                                    argTypes.push_back(argType);
                                }

                                argCount++;
                                if (peek().kind == TOK_COMMA)
                                    consume();
                                else
                                    break;
                            }
                        }

                        expect(TOK_RPAREN, "expected ')'");
                        if (argCount != (int)finfo->params.size())
                        {
                            std::string msg = "wrong number of arguments: expected " + std::to_string(finfo->params.size()) + ", got " + std::to_string(argCount);
                            err.Error(fileName, t.line, t.col, identName, msg, getCurrentSourceLine());
                        }
                        else
                        {
                            for (int i = 0; i < argCount; i++)
                            {
                                std::string paramType = finfo->params[i].type;
                                if (paramType == "any" || paramType == "auto") // auto参数直接跳过类型检查
                                    continue;
                                if (argTypes[i] != paramType)
                                {
                                    std::string msg = "argument type mismatch: expected '" + paramType + "', got '" + argTypes[i] + "'";
                                    err.Error(fileName, t.line, t.col, identName, msg, getCurrentSourceLine());
                                    break;
                                }
                            }
                        }
                        out.push_back(RpnOp::Call(identName, argCount));
                        type = finfo->retType;
                    }
                } // if (!handledAsObject)
            }
            else
            {
                auto it = env.find(identName);
                if (it == env.end())
                {
                    // 模板 var 参数：按编译期常量代入
                    auto vs = valueSubst.find(identName);
                    if (vs != valueSubst.end())
                    {
                        pushConstant(out, vs->second);
                        type = valueTypeName(vs->second);
                    }
                    else
                    {
                        std::string msg = "undefined identifier";
                        if (typeSubst.count(identName))
                        {
                            msg = "type parameter '" + identName + "' cannot be used as a value";
                        }
                        else if (!currentClass.empty())
                        {
                            ClassInfo *cc = lookupClass(currentClass);
                            if (cc && cc->members.count(identName))
                            {
                                msg = "member '" + identName + "' must be accessed as 'this." +
                                      identName + "'";
                            }
                        }
                        err.Error(fileName, t.line, t.col, identName, msg, getCurrentSourceLine());
                        out.push_back(RpnOp::LoadVar(identName));
                        type = "int";
                    }
                }
                else
                {
                    // 对象变量后面紧跟 .method( 时压引用而不是值拷贝
                    bool methodCallAhead = false;
                    if (it->second.type == Value::OBJECT &&
                        pos + 2 < (int)tokens.size() &&
                        tokens[pos].kind == TOK_DOT &&
                        tokens[pos + 1].kind == TOK_IDENT &&
                        tokens[pos + 2].kind == TOK_LPAREN)
                    {
                        methodCallAhead = true;
                    }

                    if (methodCallAhead)
                        out.push_back(RpnOp::PushObjRef(identName));
                    else
                        out.push_back(RpnOp::LoadVar(identName));

                    switch (it->second.type)
                    {
                    case Value::INT:
                        type = "int";
                        break;
                    case Value::BOOL:
                        type = "bool";
                        break;
                    case Value::STRING:
                        type = "string";
                        break;
                    case Value::CHAR:
                        type = "char";
                        break; // char变量
                    case Value::FLOAT:
                        type = "float";
                        break;
                    case Value::OBJECT:
                        type = it->second.objectClass; // 类型为类名
                        break;
                    case Value::ARRAY:
                        type = "array";
                        arrDepthLeft = it->second.arrayDepth;
                        elemTypeStr = basicTypeName(it->second.elemType);
                        break;
                    default:
                        type = "void";
                    }
                }
            }
        }
        else if (t.kind == TOK_FACT)
        {
            consume(); // 吃掉 fact
            expect(TOK_LESS, "expected '<' after 'fact'");
            std::string paramType = parseAddSub(env, out); // 解析阶乘参数（用加减层避免吃掉 >）

            // 类型校验：仅支持数值类型
            if (paramType != "int" && paramType != "float" && paramType != "char")
            {
                err.Error(fileName, t.line, t.col, "fact",
                          "fact argument must be numeric", getCurrentSourceLine());
            }

            expect(TOK_GREATER, "expected '>'");
            out.push_back(RpnOp::BinOp(OP_FACT)); // 复用单目运算格式
            type = "int";                         // 阶乘结果为整数
        }

        else if (t.kind == TOK_SQRT)
        {
            consume(); // 吃掉 sqrt
            expect(TOK_LESS, "expected '<' after 'sqrt'");
            std::string paramType = parseAddSub(env, out);

            // 类型校验：仅支持整数类型
            if (paramType != "int" && paramType != "char")
            {
                err.Error(fileName, t.line, t.col, "sqrt",
                          "sqrt argument must be integer", getCurrentSourceLine());
            }

            expect(TOK_GREATER, "expected '>'");
            out.push_back(RpnOp::BinOp(OP_SQRT));
            type = "float"; // 开平方结果为浮点
        }

        else if ((t.kind == TOK_INT || t.kind == TOK_FLOAT || t.kind == TOK_CHAR || t.kind == TOK_BOOL || t.kind == TOK_STRING) && pos + 1 < (int)tokens.size() && tokens[pos + 1].kind == TOK_LESS)
        {
            std::string targetType = consume().text; // 目标类型名
            consume();                               // 吃掉 <

            std::string srcType = parseAddSub(env, out); // 解析源表达式（加减层避免吃掉 >）

            // ========== 编译期类型合法性检查 ==========
            bool allow = false;
            // 数值类型之间可互转
            if (targetType == "int" || targetType == "float" || targetType == "char" || targetType == "bool")
            {
                allow = (srcType == "int" || srcType == "float" || srcType == "char" || srcType == "bool");
            }
            // string 可接受所有基础类型
            else if (targetType == "string")
            {
                allow = (srcType == "int" || srcType == "float" || srcType == "char" || srcType == "bool" || srcType == "string");
            }

            if (!allow)
            {
                err.Error(fileName, t.line, t.col, targetType,
                          "cannot cast '" + srcType + "' to '" + targetType + "'",
                          getCurrentSourceLine());
            }

            expect(TOK_GREATER, "expected '>'");

            // 生成转换指令，intArg 存目标类型枚举
            Value::Type targetTypeEnum;
            if (targetType == "int")
                targetTypeEnum = Value::INT;
            else if (targetType == "float")
                targetTypeEnum = Value::FLOAT;
            else if (targetType == "char")
                targetTypeEnum = Value::CHAR;
            else if (targetType == "bool")
                targetTypeEnum = Value::BOOL;
            else
                targetTypeEnum = Value::STRING;

            RpnOp castOp = RpnOp::BinOp(OP_CAST);
            castOp.intArg = (int)targetTypeEnum;
            out.push_back(castOp);

            type = targetType;
        }

        else if (t.kind == TOK_DOLLAR_TYPE)
        {
            std::string typeName = t.text;
            // 先做模板参数替换
            auto subIt = typeSubst.find(typeName);
            if (subIt != typeSubst.end())
            {
                typeName = subIt->second;
            }

            Value::Type realType;
            if (typeName == "int")
            {
                realType = Value::INT;
            }
            else if (typeName == "float")
            {
                realType = Value::FLOAT;
            }
            else if (typeName == "bool")
            {
                realType = Value::BOOL;
            }
            else if (typeName == "char")
            {
                realType = Value::CHAR;
            }
            else if (typeName == "string")
            {
                realType = Value::STRING;
            }
            else
            {
                err.Error(fileName, t.line, t.col, "$" + typeName,
                          "$: unknown type", getCurrentSourceLine());
                out.push_back(RpnOp::PushInt(0));
                return "int";
            }
            out.push_back(RpnOp::PushDefault(realType));
            return basicTypeName(realType);
        }

        else
        {
            err.Error(fileName, t.line, t.col, t.text, "expected expression", getCurrentSourceLine());
            consume();
            type = "void";
        }

        // 下标解析（数组 / 字符串 / operator[]），成员访问之后也可能出现
        auto parseSubscript = [&]()
        {
            consume();
            std::string idxType = parseExpression(env, out);
            expect(TOK_RBRACKET, "expected ']'");

            if (type == "string")
            {
                out.push_back(RpnOp::StringIndex());
                type = "char";
            }
            else if (type == "array")
            {
                out.push_back(RpnOp::ArrayLoad());
                if (arrDepthLeft > 0)
                    arrDepthLeft--;

                if (arrDepthLeft > 0)
                    type = "array"; // 还有下一维
                else
                    type = elemTypeStr; // 到达叶子元素
            }
            else if (ClassInfo *cls = lookupClass(type))
            {
                // 类类型：查 operator[]
                std::vector<std::string> args{idxType};
                std::string mangled = resolveOperatorOverload(cls, "[]", args, t, "[]");
                if (!mangled.empty())
                {
                    RpnOp callOp = RpnOp::Call(mangled, 1);
                    callOp.boolArg = true;
                    out.push_back(callOp);
                    type = cls->methods[mangled].retType;
                }
                else
                {
                    type = "void";
                }
            }
            else
            {
                err.Error(fileName, t.line, t.col, t.text,
                          "cannot index value of type '" + type + "'", getCurrentSourceLine());
            }
        };

        while (peek().kind == TOK_LBRACKET)
            parseSubscript();

        while (peek().kind == TOK_DOT)
        {
            consume();
            Token memTok = consume();
            if (memTok.kind != TOK_IDENT)
            {
                err.Error(fileName, memTok.line, memTok.col, memTok.text,
                          "expected member name", getCurrentSourceLine());
                break;
            }
            std::string memName = memTok.text;

            ClassInfo *cls = lookupClass(type);
            if (!cls)
            {
                err.Error(fileName, memTok.line, memTok.col, memName,
                          "type '" + type + "' has no members", getCurrentSourceLine());
            }

            if (peek().kind == TOK_LPAREN)
            {
                // ===== 成员方法调用 =====
                FuncInfo *m = nullptr;
                if (cls)
                {
                    auto mit = cls->methods.find(memName);
                    if (mit == cls->methods.end())
                    {
                        err.Error(fileName, memTok.line, memTok.col, memName,
                                  "class '" + cls->name + "' has no method '" + memName + "'",
                                  getCurrentSourceLine());
                    }
                    else
                    {
                        m = &mit->second;
                        auto ait = cls->methodAccess.find(memName);
                        Access acc = (ait != cls->methodAccess.end()) ? ait->second : Access::Private;
                        if (!canAccess(cls->name, acc))
                        {
                            err.Error(fileName, memTok.line, memTok.col, memName,
                                      "cannot access private method '" + memName + "' of class '" + cls->name + "'",
                                      getCurrentSourceLine());
                        }
                    }
                }

                int argCount = 0;
                std::vector<std::string> argTypes;
                parseCallArgs(m, env, out, argCount, argTypes, memName, memTok);
                type = m ? m->retType : "void";

                RpnOp callOp = RpnOp::Call(memName, argCount);
                callOp.boolArg = true;
                out.push_back(callOp);
            }
            else
            {
                // ===== 成员变量读取 =====
                if (cls)
                {
                    auto mit = cls->members.find(memName);
                    if (mit == cls->members.end())
                    {
                        err.Error(fileName, memTok.line, memTok.col, memName,
                                  "class '" + cls->name + "' has no member '" + memName + "'",
                                  getCurrentSourceLine());
                        type = "void";
                    }
                    else
                    {
                        if (!canAccess(cls->name, mit->second.access))
                        {
                            err.Error(fileName, memTok.line, memTok.col, memName,
                                      "cannot access private member '" + memName + "' of class '" + cls->name + "'",
                                      getCurrentSourceLine());
                        }
                        if (mit->second.isArray)
                        {
                            type = "array";
                            arrDepthLeft = mit->second.arrayDepth;
                            elemTypeStr = basicTypeName(mit->second.elemType);
                        }
                        else
                        {
                            type = mit->second.type;
                        }
                    }
                }
                else
                {
                    type = "void";
                }

                out.push_back(RpnOp::MemberLoad(memName));
            }
        }

        // 成员数组下标：this.arr[i] / obj.arr[i]
        while (peek().kind == TOK_LBRACKET)
            parseSubscript();

        return type;
    }

    inline std::string Parser::parseTerm(LocalEnv &env, RpnList &out)
    {
        std::string type = parseFactor(env, out);
        while (peek().kind == TOK_STAR || peek().kind == TOK_SLASH || peek().kind == TOK_PERCENT)
        {
            Token op = consume();
            std::string rightType = parseFactor(env, out);

            std::string symbol = (op.kind == TOK_STAR) ? "*" : (op.kind == TOK_SLASH) ? "/"
                                                                                      : "%";

            // 类类型左操作数：查 operator* / operator/
            if (ClassInfo *cls = lookupClass(type); cls && op.kind != TOK_PERCENT)
            {
                std::vector<std::string> args{rightType};
                std::string mangled = resolveOperatorOverload(cls, symbol, args, op, symbol);
                if (!mangled.empty())
                {
                    RpnOp callOp = RpnOp::Call(mangled, 1);
                    callOp.boolArg = true;
                    out.push_back(callOp);
                    type = cls->methods[mangled].retType;
                }
                else
                {
                    type = "int";
                }
                continue;
            }

            if (isClassType(type) || isClassType(rightType))
            {
                err.Error(fileName, op.line, op.col, op.text,
                          "operator '" + symbol + "' is not defined for class operands",
                          getCurrentSourceLine());
                type = "int";
                continue;
            }

            // 类型推导：有 float 则结果为 float
            if (type == "float" || rightType == "float")
                type = "float";
            else
                type = "int";

            if (op.kind == TOK_STAR)
                out.push_back(RpnOp::BinOp(OP_MUL));
            else if (op.kind == TOK_SLASH)
                out.push_back(RpnOp::BinOp(OP_DIV));
            else
                out.push_back(RpnOp::BinOp(OP_MOD));
        }
        return type;
    }

    inline std::string Parser::parseAddSub(LocalEnv &env, RpnList &out)
    {
        std::string type = parseTerm(env, out);
        while (peek().kind == TOK_PLUS || peek().kind == TOK_MINUS)
        {
            Token op = consume();
            std::string rightType = parseTerm(env, out);

            std::string symbol = (op.kind == TOK_PLUS) ? "+" : "-";

            // 类类型左操作数：查 operator+ / operator-
            if (ClassInfo *cls = lookupClass(type))
            {
                std::vector<std::string> args{rightType};
                std::string mangled = resolveOperatorOverload(cls, symbol, args, op, symbol);
                if (!mangled.empty())
                {
                    RpnOp callOp = RpnOp::Call(mangled, 1);
                    callOp.boolArg = true;
                    out.push_back(callOp);
                    type = cls->methods[mangled].retType;
                }
                else
                {
                    type = "int";
                }
                continue;
            }

            if (isClassType(rightType))
            {
                err.Error(fileName, op.line, op.col, op.text,
                          "operator '" + symbol + "' is not defined for class operands",
                          getCurrentSourceLine());
                type = "int";
                continue;
            }

            if (op.kind == TOK_PLUS)
            {
                bool leftStr = (type == "string");
                bool rightStr = (rightType == "string");
                bool leftChar = (type == "char");
                bool rightChar = (rightType == "char");
                bool leftFloat = (type == "float");
                bool rightFloat = (rightType == "float");

                if (leftStr || rightStr)
                {
                    // 字符串拼接逻辑不变
                    if (!rightStr && !rightChar)
                        err.Error(fileName, op.line, op.col, "+", "cannot concatenate string with " + rightType, getCurrentSourceLine());
                    if (!leftStr && !leftChar)
                        err.Error(fileName, op.line, op.col, "+", "cannot concatenate " + type + " with string", getCurrentSourceLine());
                    type = "string";
                }
                else if (leftFloat || rightFloat)
                {
                    type = "float"; // 浮点加法
                }
                else
                {
                    type = "int"; // 整数加法
                }
                out.push_back(RpnOp::BinOp(OP_ADD));
            }
            else
            {
                // 减法：有 float 则结果为 float
                if (type == "float" || rightType == "float")
                    type = "float";
                else
                    type = "int";
                out.push_back(RpnOp::BinOp(OP_SUB));
            }
        }
        return type;
    }

    inline std::string Parser::parseRelational(LocalEnv &env, RpnList &out)
    {
        std::string type = parseAddSub(env, out);
        while (peek().kind == TOK_GREATER || peek().kind == TOK_LESS || peek().kind == TOK_GE || peek().kind == TOK_LE)
        {
            Token op = consume();
            std::string rightType = parseAddSub(env, out);

            // 类型校验：仅 int/float 可比较
            bool valid = (type == "int" || type == "float") && (rightType == "int" || rightType == "float");
            if (!valid)
            {
                err.Error(fileName, op.line, op.col, op.text,
                          "comparison only supports int/float type",
                          getCurrentSourceLine());
            }

            if (op.kind == TOK_GREATER)
                out.push_back(RpnOp::BinOp(OP_GT));
            else if (op.kind == TOK_LESS)
                out.push_back(RpnOp::BinOp(OP_LT));
            else if (op.kind == TOK_GE)
                out.push_back(RpnOp::BinOp(OP_GE));
            else if (op.kind == TOK_LE)
                out.push_back(RpnOp::BinOp(OP_LE));
            type = "bool";
        }
        return type;
    }

    inline std::string Parser::parseExpression(LocalEnv &env, RpnList &out)
    {
        // 逻辑运算符的操作数必须是 bool / int
        auto checkLogicalOperand = [&](const std::string &t, const Token &op, const char *which)
        {
            if (t == "bool" || t == "int")
                return;
            err.Error(fileName, op.line, op.col, op.text,
                      std::string("logical operator '") + which +
                          "' requires bool or int operand, got '" + t + "'",
                      getCurrentSourceLine());
        };

        // 非 bool 的 int 操作数统一转成 bool，保证两路分支结果类型一致
        auto pushBoolCast = [&](RpnList &o)
        {
            RpnOp castOp = RpnOp::BinOp(OP_CAST);
            castOp.intArg = (int)Value::BOOL;
            o.push_back(castOp);
        };

        // 相等 / 约等于（== != ~=）
        auto parseEqualityLevel = [&](LocalEnv &e, RpnList &o) -> std::string
        {
            std::string type = parseRelational(e, o);
            while (peek().kind == TOK_EQ || peek().kind == TOK_NE || peek().kind == TOK_APPROX_EQ)
            {
                Token op = consume();
                std::string rightType = parseRelational(e, o);
                // 类型校验：仅数值类型可比较（int/float/char）
                bool leftNum = (type == "int" || type == "float" || type == "char");
                bool rightNum = (rightType == "int" || rightType == "float" || rightType == "char");
                if (!leftNum || !rightNum)
                {
                    err.Error(fileName, op.line, op.col, op.text,
                              "comparison only supports int/float/char",
                              getCurrentSourceLine());
                }

                if (op.kind == TOK_EQ)
                    o.push_back(RpnOp::BinOp(OP_EQ));
                else if (op.kind == TOK_NE)
                    o.push_back(RpnOp::BinOp(OP_NE));
                else // 约等于
                    o.push_back(RpnOp::BinOp(OP_APPROX_EQ));
                type = "bool";
            }
            return type;
        };

        // &&：优先级低于相等比较，高于 ||
        // 短路求值：a 为假时不计算 b，直接压 false
        //   <a> JMP_IF_FALSE Lfalse <b> [CAST bool] JMP Lend Lfalse: PUSH_BOOL false Lend:
        auto parseAndLevel = [&](LocalEnv &e, RpnList &o) -> std::string
        {
            std::string type = parseEqualityLevel(e, o);
            while (peek().kind == TOK_AND)
            {
                Token op = consume();

                // 重载 &&：按 C++ 语义不短路，直接函数调用
                if (ClassInfo *cls = lookupClass(type))
                {
                    std::string rightType = parseEqualityLevel(e, o);
                    std::vector<std::string> args{rightType};
                    std::string mangled = resolveOperatorOverload(cls, "&&", args, op, "&&");
                    if (!mangled.empty())
                    {
                        RpnOp callOp = RpnOp::Call(mangled, 1);
                        callOp.boolArg = true;
                        o.push_back(callOp);
                        type = cls->methods[mangled].retType;
                    }
                    else
                    {
                        type = "bool";
                    }
                    continue;
                }

                checkLogicalOperand(type, op, "&&");

                int jmpFalse = (int)o.size();
                o.push_back(RpnOp::JmpIfFalse(0));

                std::string rightType = parseEqualityLevel(e, o);
                checkLogicalOperand(rightType, op, "&&");
                if (rightType != "bool")
                    pushBoolCast(o);

                int jmpEnd = (int)o.size();
                o.push_back(RpnOp::Jmp(0));

                o[jmpFalse].intArg = (int)o.size();
                o.push_back(RpnOp::PushBool(false));
                o[jmpEnd].intArg = (int)o.size();

                type = "bool";
            }
            return type;
        };

        // ||：优先级最低
        // 短路求值：a 为真时不计算 b，直接压 true
        //   <a> JMP_IF_FALSE Lright PUSH_BOOL true JMP Lend Lright: <b> [CAST bool] Lend:
        std::string type = parseAndLevel(env, out);
        while (peek().kind == TOK_OR)
        {
            Token op = consume();

            if (ClassInfo *cls = lookupClass(type))
            {
                std::string rightType = parseAndLevel(env, out);
                std::vector<std::string> args{rightType};
                std::string mangled = resolveOperatorOverload(cls, "||", args, op, "||");
                if (!mangled.empty())
                {
                    RpnOp callOp = RpnOp::Call(mangled, 1);
                    callOp.boolArg = true;
                    out.push_back(callOp);
                    type = cls->methods[mangled].retType;
                }
                else
                {
                    type = "bool";
                }
                continue;
            }

            checkLogicalOperand(type, op, "||");

            int jmpFalse = (int)out.size();
            out.push_back(RpnOp::JmpIfFalse(0));

            out.push_back(RpnOp::PushBool(true));

            int jmpEnd = (int)out.size();
            out.push_back(RpnOp::Jmp(0));

            out[jmpFalse].intArg = (int)out.size();

            std::string rightType = parseAndLevel(env, out);
            checkLogicalOperand(rightType, op, "||");
            if (rightType != "bool")
                pushBoolCast(out);

            out[jmpEnd].intArg = (int)out.size();
            type = "bool";
        }
        return type;
    }

    // 递归解析多维数组初始化列表：{{1,2},{3,4}}
    // 数组本身已经在栈顶，这里只压 [下标...] [值]，再用 ARRAY_STORE_N 写回栈上的数组
    // array / alloc 共用：解析各维尺寸、创建数组、初始化并存入变量
    // 第一维可以写成 []，此时大小由初始化列表的元素个数推断
    inline void Parser::parseArrayDeclRest(LocalEnv &env, RpnList &out, const std::string &arrName, const Token &nameTok, const std::string &elemTypeStr)
    {
        Value::Type elemType = Value::INT;
        bool isBasicType = true;
        if (elemTypeStr == "int")
            elemType = Value::INT;
        else if (elemTypeStr == "bool")
            elemType = Value::BOOL;
        else if (elemTypeStr == "string")
            elemType = Value::STRING;
        else if (elemTypeStr == "char")
            elemType = Value::CHAR;
        else if (elemTypeStr == "float")
            elemType = Value::FLOAT;
        else
            isBasicType = false;

        if (!isBasicType)
        {
            err.Error(fileName, nameTok.line, nameTok.col, elemTypeStr,
                      "unsupported array element type", getCurrentSourceLine());
            elemType = Value::INT;
        }

        int ndims = 0;
        bool inferFirst = false;
        RpnList sizeRpn; // 用户写出的各维尺寸表达式

        while (peek().kind == TOK_LBRACKET)
        {
            consume();

            if (peek().kind == TOK_RBRACKET)
            {
                // []：只允许第一维，大小由初始化列表推断
                if (ndims != 0)
                {
                    err.Error(fileName, peek().line, peek().col, "[]",
                              "only the first dimension can be inferred", getCurrentSourceLine());
                }
                else
                {
                    inferFirst = true;
                }
                consume(); // ]
            }
            else
            {
                std::string sizeType = parseExpression(env, sizeRpn);
                if (sizeType != "int")
                {
                    err.Error(fileName, nameTok.line, nameTok.col, arrName,
                              "array size must be integer", getCurrentSourceLine());
                }
                expect(TOK_RBRACKET, "expected ']' after array size");
            }
            ndims++;
        }

        if (ndims == 0)
        {
            err.Error(fileName, nameTok.line, nameTok.col, arrName,
                      "expected '[' after array type", getCurrentSourceLine());
            while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                consume();
            if (peek().kind == TOK_SEMICOLON)
                consume();
            return;
        }
        if (ndims > 8)
        {
            err.Error(fileName, nameTok.line, nameTok.col, arrName,
                      "too many array dimensions (max 8)", getCurrentSourceLine());
        }

        // 注册数组变量到作用域（仅登记类型）
        Value v{};
        v.type = Value::ARRAY;
        v.elemType = elemType;
        v.arrayDepth = ndims;
        env[arrName] = v;

        // 可选 '=' + 初始化列表
        bool hasAssign = false;
        if (peek().kind == TOK_ASSIGN)
        {
            consume();
            hasAssign = true;
        }

        bool hasInit = (peek().kind == TOK_LBRACE);

        if (inferFirst)
        {
            if (hasInit)
            {
                std::vector<int> path;
                RpnList initRpn;
                int elementCount = 0;
                parseArrayInitializer(env, initRpn, ndims, path, 0, elemTypeStr, &elementCount);

                out.push_back(RpnOp::PushInt(elementCount));
                for (auto &op : sizeRpn)
                    out.push_back(op);
                out.push_back(RpnOp::NewArrayND(elemType, ndims));
                for (auto &op : initRpn)
                    out.push_back(op);
            }
            else
            {
                err.Error(fileName, nameTok.line, nameTok.col, arrName,
                          "cannot infer array size; write [N] or provide an initializer",
                          getCurrentSourceLine());
                out.push_back(RpnOp::PushInt(0));
                for (auto &op : sizeRpn)
                    out.push_back(op);
                out.push_back(RpnOp::NewArrayND(elemType, ndims));
            }
        }
        else
        {
            if (hasAssign && !hasInit)
            {
                Token bad = peek();
                err.Error(fileName, bad.line, bad.col, bad.text,
                          "expected '{' after '=' in array declaration", getCurrentSourceLine());
            }

            for (auto &op : sizeRpn)
                out.push_back(op);
            out.push_back(RpnOp::NewArrayND(elemType, ndims));

            if (hasInit)
            {
                std::vector<int> path;
                parseArrayInitializer(env, out, ndims, path, 0, elemTypeStr);
            }
        }

        out.push_back(RpnOp::StoreVar(arrName));
        expect(TOK_SEMICOLON, "expected ';' after array declaration");
    }

    inline void Parser::parseArrayInitializer(LocalEnv &env, RpnList &out, int ndims, std::vector<int> &path, int level, const std::string &baseType, int *topCount)
    {
        expect(TOK_LBRACE, "expected '{' in array initializer");

        int idx = 0;
        while (peek().kind != TOK_RBRACE && peek().kind != TOK_EOF)
        {
            path.push_back(idx);

            if (level < ndims - 1)
            {
                if (peek().kind != TOK_LBRACE)
                {
                    Token bad = peek();
                    err.Error(fileName, bad.line, bad.col, bad.text,
                              "multidimensional array initializer needs nested '{ }'",
                              getCurrentSourceLine());

                    // 跳过当前这一层的剩余初始化列表，避免每个元素都报一次
                    int braceDepth = 0;
                    while (peek().kind != TOK_EOF)
                    {
                        if (peek().kind == TOK_LBRACE)
                            braceDepth++;
                        else if (peek().kind == TOK_RBRACE)
                        {
                            if (braceDepth == 0)
                            {
                                consume(); // 吃掉当前层的 '}'
                                break;
                            }
                            braceDepth--;
                        }
                        consume();
                    }

                    path.pop_back();
                    return;
                }
                else
                {
                    parseArrayInitializer(env, out, ndims, path, level + 1, baseType);
                }
            }
            else
            {
                for (int p : path)
                    out.push_back(RpnOp::PushInt(p));

                Token valTok = peek();
                std::string valType = parseExpression(env, out);
                bool typeOk = (valType == baseType) || (isNumericTypeName(valType) && isNumericTypeName(baseType)) || (valType == "vd");
                if (!typeOk)
                {
                    err.Error(fileName, valTok.line, valTok.col, valType,
                              "array element type mismatch: expected '" + baseType + "', got '" + valType + "'",
                              getCurrentSourceLine());
                }
                out.push_back(RpnOp::ArrayStoreN(ndims));
            }

            path.pop_back();
            idx++;

            if (peek().kind == TOK_COMMA)
                consume();
            else
                break;
        }

        if (topCount && level == 0)
            *topCount = idx;

        expect(TOK_RBRACE, "expected '}' after array initializer");
    }

    // if / elif / else 的分支体：有 { } 就是块，否则只收下一条语句（与 C++ 一致）
    inline void Parser::parseBranchBody(LocalEnv &env, RpnList &out, std::vector<std::string> &retTypes, const char *which)
    {
        if (peek().kind == TOK_LBRACE)
        {
            consume();
            parseBlock(env, out, retTypes);
            return;
        }

        // 没有 { } 时至少要有一条语句；连语句都没有才报错
        TokenKind k = peek().kind;
        if (k == TOK_EOF || k == TOK_RBRACE || k == TOK_ELSE || k == TOK_ELIF || k == TOK_SEMICOLON)
        {
            Token t = peek();
            err.Error(fileName, t.line, t.col, t.text,
                      std::string("expected '{' or a statement after '") + which + "'",
                      getCurrentSourceLine());
            if (k == TOK_SEMICOLON)
                consume(); // 吃掉空语句，避免解析器原地打转
            return;
        }

        parseStatement(env, out, retTypes);
    }

    inline void Parser::parseBlock(LocalEnv &env, RpnList &out, std::vector<std::string> &retTypes)
    {
        while (peek().kind != TOK_RBRACE && peek().kind != TOK_EOF)
        {
            parseStatement(env, out, retTypes);
        }
        expect(TOK_RBRACE, "expected '}'");
    }

    inline void Parser::handleInclude(RpnList &mainOut)
    {
        Token tInclude = peek();
        std::string includeSrcLine = getCurrentSourceLine();
        consume(); // 吃掉 include

        std::string filePath;
        bool isSystem = false;

        if (peek().kind == TOK_LESS)
        {
            consume();
            isSystem = true;
            std::string libName;
            while (peek().kind != TOK_GREATER && peek().kind != TOK_EOF)
            {
                Token tk = peek();
                if (tk.kind == TOK_IDENT)
                {
                    libName += tk.text;
                    consume();
                }
                else if (tk.kind == TOK_SLASH)
                {
                    libName += '/';
                    consume();
                }
                else
                {
                    // <>里面只允许标识符和斜杠 /
                    err.Error(fileName, tInclude.line, tInclude.col, "include",
                              "system include <...> only allow identifier and '/'", includeSrcLine);
                    // 容错，直接跳到 >
                    while (peek().kind != TOK_GREATER && peek().kind != TOK_EOF)
                        consume();
                    break;
                }
            }
            if (!expect(TOK_GREATER, "expected '>' after system include path"))
            {
                if (peek().kind == TOK_SEMICOLON)
                    consume();
                return;
            }
            // 拼接系统头文件路径，跨平台兼容，优先 /，windows会自动处理
            filePath = sysIncludeDir + "/" + libName + ".hnl";
        }

        else if (peek().kind == TOK_STRLIT)
        {
            // ""本地头文件：完整文件名，原样使用，不自动补后缀
            std::string name = consume().text;
            filePath = fileDir + "\\" + name;
        }
        else
        {
            err.Error(fileName, tInclude.line, tInclude.col, "include", "expected '<header>' or \"filename\"", includeSrcLine);
            return;
        }

        // 可选分号，写不写;都兼容
        if (peek().kind == TOK_SEMICOLON)
        {
            consume();
        }

        // 重复包含
        if (includedFiles.count(filePath))
            return;
        includedFiles.insert(filePath);

        std::vector<std::string> headerLines;
        try
        {
            headerLines = tool::file::readFile(filePath);
        }
        catch (const std::runtime_error &)
        {
            std::string msg = isSystem ? "system header not found" : "header file not found";
            err.Error(fileName, tInclude.line, tInclude.col, "include", msg, includeSrcLine);
            return;
        }

        std::string headerSrc;
        for (auto &line : headerLines)
            headerSrc += line + '\n';
        Lexer lexer(headerSrc, err, filePath);
        std::vector<Token> headerTokens = lexer.tokenize();

        Parser headerParser(headerTokens, headerLines, err, filePath);
        headerParser.parseProgram(mainOut);
    }

    // ===== 调试：打印RPN字节码 =====
    inline void dumpRpn(const RpnList &rpn)
    {
        std::cout << "\n===== Parser RPN Bytecode =====\n";
        const char *opNames[] = {
            "PUSH_INT", "PUSH_BOOL", "PUSH_STR", "PUSH_CHAR", "PUSH_REF", "PUSH_FLOAT", "PUSH_DEFAULT",
            "STRING_INDEX", "ADD", "SUB", "MUL", "DIV", "CALL", "RET", "POP",
            "LOAD_VAR", "STORE_VAR", "JMP_IF_FALSE", "JMP", "NEW_ARRAY", "ARRAY_LOAD",
            "ARRAY_STORE", "AND", "OR", "NEW_OBJECT", "MEMBER_LOAD", "MEMBER_STORE",
            "PUSH_THIS", "MOD", "GT", "LT", "GE", "LE", "EQ", "NE", "APPROX_EQ",
            "FACT", "SQRT", "CAST", "NEW_ARRAY_ND", "ARRAY_STORE_N", "PUSH_OBJ_REF",
            "PUSH_VD", "OP_NOT", "OP_JUMP_BREAK",
            "CALL_CTOR"};
        static_assert(sizeof(opNames) / sizeof(opNames[0]) == (size_t)OP_CALL_CTOR + 1,
                      "opNames[] is out of sync with RpnOpKind");
        for (size_t i = 0; i < rpn.size(); i++)
        {
            std::cout << "[" << i << "] " << opNames[(int)rpn[i].op];
            if (rpn[i].op == OP_PUSH_INT)
                std::cout << "  val=" << rpn[i].intArg;
            else if (rpn[i].op == OP_PUSH_BOOL)
                std::cout << "  val=" << (rpn[i].boolArg ? "true" : "false");
            else if (rpn[i].op == OP_PUSH_STR || rpn[i].op == OP_PUSH_REF || rpn[i].op == OP_PUSH_OBJ_REF || rpn[i].op == OP_LOAD_VAR || rpn[i].op == OP_STORE_VAR || rpn[i].op == OP_CALL || rpn[i].op == OP_CALL_CTOR)
                std::cout << "  name=\"" << rpn[i].strArg << "\"";
            else if (rpn[i].op == OP_JMP || rpn[i].op == OP_JMP_IF_FALSE)
                std::cout << "  target=" << rpn[i].intArg;
            else if (rpn[i].op == OP_NEW_ARRAY_ND)
                std::cout << "  elemType=" << rpn[i].intArg << " dims=" << rpn[i].intArg2;
            else if (rpn[i].op == OP_ARRAY_STORE_N)
                std::cout << "  dims=" << rpn[i].intArg;
            std::cout << "\n";
        }
        std::cout << "==============================\n";
    }

    inline void Parser::parseClassDecl()
    {
        consume(); // 吃掉 class
        Token nameTok = consume();
        if (nameTok.kind != TOK_IDENT)
        {
            err.Error(fileName, nameTok.line, nameTok.col, nameTok.text,
                      "expected class name", getCurrentSourceLine());
            // 容错跳过类体
            while (peek().kind != TOK_LBRACE && peek().kind != TOK_EOF)
                consume();
            if (peek().kind == TOK_LBRACE)
                consume();
            int depth = 1;
            while (depth > 0 && peek().kind != TOK_EOF)
            {
                if (peek().kind == TOK_LBRACE)
                    depth++;
                else if (peek().kind == TOK_RBRACE)
                    depth--;
                consume();
            }
            return;
        }
        std::string className = nameTok.text;

        // 类模板：class Name<type T, var N> { ... }
        if (peek().kind == TOK_LESS)
        {
            parseTemplateDecl(className, nameTok);
            return;
        }

        expect(TOK_LBRACE, "expected '{' after class name");

        if (globalClassTable.count(className) || classTemplates.count(className))
        {
            err.Error(fileName, nameTok.line, nameTok.col, className,
                      "class or template '" + className + "' is already defined",
                      getCurrentSourceLine());
        }

        // 先注册空类：方法体里可以引用类自身、其它成员和后面的方法
        ClassInfo &cls = globalClassTable[className];
        cls = ClassInfo{};
        cls.name = className;

        struct MethodDecl
        {
            std::string name;
            FuncInfo info;
            Access access;
            int bodyPos;
            bool isCtor;
        };
        std::vector<MethodDecl> methodDecls;

        Access access = Access::Private; // C++ class 默认 private

        // ================= 第一趟：成员变量 + 方法签名 =================
        while (peek().kind != TOK_RBRACE && peek().kind != TOK_EOF)
        {
            // 访问标签 public: / private: / protected:
            if (peek().kind == TOK_PUBLIC || peek().kind == TOK_PRIVATE || peek().kind == TOK_PROTECTED)
            {
                if (peek().kind == TOK_PUBLIC)
                    access = Access::Public;
                else if (peek().kind == TOK_PRIVATE)
                    access = Access::Private;
                else
                    access = Access::Protected;

                consume();
                expect(TOK_COLON, "expected ':' after access specifier");
                continue;
            }

            if (peek().kind == TOK_ARRAY)
            {
                // 数组成员：array T name[]; 或 array T name[N];
                consume();
                bool dc = false, dr = false;
                std::string elemTypeStr = parseType(dc, dr);

                Token memNameTok = consume();
                if (memNameTok.kind != TOK_IDENT)
                {
                    err.Error(fileName, memNameTok.line, memNameTok.col, memNameTok.text,
                              "expected member name", getCurrentSourceLine());
                    while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                        consume();
                    if (peek().kind == TOK_SEMICOLON)
                        consume();
                    continue;
                }

                Value::Type elemType = Value::INT;
                bool basic = true;
                if (elemTypeStr == "int")
                    elemType = Value::INT;
                else if (elemTypeStr == "bool")
                    elemType = Value::BOOL;
                else if (elemTypeStr == "string")
                    elemType = Value::STRING;
                else if (elemTypeStr == "char")
                    elemType = Value::CHAR;
                else if (elemTypeStr == "float")
                    elemType = Value::FLOAT;
                else
                    basic = false;

                if (!basic)
                {
                    err.Error(fileName, memNameTok.line, memNameTok.col, elemTypeStr,
                              "array member element type must be a basic type",
                              getCurrentSourceLine());
                    elemType = Value::INT;
                }

                int size = 0;
                if (peek().kind == TOK_LBRACKET)
                {
                    consume();
                    if (peek().kind == TOK_NUMBER)
                    {
                        size = std::stoi(consume().text);
                    }
                    else if (peek().kind != TOK_RBRACKET)
                    {
                        err.Error(fileName, peek().line, peek().col, peek().text,
                                  "array member size must be a literal integer",
                                  getCurrentSourceLine());
                        while (peek().kind != TOK_RBRACKET && peek().kind != TOK_SEMICOLON &&
                               peek().kind != TOK_EOF)
                            consume();
                    }
                    expect(TOK_RBRACKET, "expected ']' after array member");
                }
                else
                {
                    err.Error(fileName, memNameTok.line, memNameTok.col, memNameTok.text,
                              "expected '[' after array member name", getCurrentSourceLine());
                }

                if (peek().kind == TOK_LBRACKET)
                {
                    err.Error(fileName, peek().line, peek().col, peek().text,
                              "only one-dimensional array members are supported",
                              getCurrentSourceLine());
                    while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                        consume();
                }

                if (cls.members.count(memNameTok.text))
                {
                    err.Error(fileName, memNameTok.line, memNameTok.col, memNameTok.text,
                              "duplicate member '" + memNameTok.text + "'", getCurrentSourceLine());
                }
                else
                {
                    MemberInfo mi;
                    mi.type = "array";
                    mi.access = access;
                    mi.isArray = true;
                    mi.elemType = elemType;
                    mi.arrayDepth = 1;
                    mi.arraySize = size;
                    cls.members[memNameTok.text] = mi;
                }

                expect(TOK_SEMICOLON, "expected ';' after member");
                continue;
            }

            if (peek().kind == TOK_VAR)
            {
                // 成员变量
                consume();
                bool isConst = false, dummyRef = false;
                std::string memType = parseType(isConst, dummyRef);
                Token memNameTok = consume();
                if (memNameTok.kind != TOK_IDENT)
                {
                    err.Error(fileName, memNameTok.line, memNameTok.col, memNameTok.text,
                              "expected member name", getCurrentSourceLine());
                    while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                        consume();
                    if (peek().kind == TOK_SEMICOLON)
                        consume();
                    continue;
                }

                bool isBase = isBuiltinTypeName(memType);
                if (!isBase && !lookupClass(memType))
                {
                    err.Error(fileName, memNameTok.line, memNameTok.col, memType,
                              "member type '" + memType + "' is not a known type or class", getCurrentSourceLine());
                }

                if (cls.members.count(memNameTok.text))
                {
                    err.Error(fileName, memNameTok.line, memNameTok.col, memNameTok.text,
                              "duplicate member '" + memNameTok.text + "'", getCurrentSourceLine());
                }
                else
                {
                    cls.members[memNameTok.text] = MemberInfo{memType, access};
                }

                // 类内成员初始化暂不支持
                if (peek().kind == TOK_ASSIGN)
                {
                    err.Error(fileName, peek().line, peek().col, "=",
                              "in-class member initializer is not supported", getCurrentSourceLine());
                    while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                        consume();
                }

                expect(TOK_SEMICOLON, "expected ';' after member");
                continue;
            }

            bool cppCtorStyle = (peek().kind == TOK_IDENT &&
                                 (peek().text == className ||
                                  (!templateBaseName.empty() && peek().text == templateBaseName)) &&
                                 pos + 1 < (int)tokens.size() &&
                                 tokens[pos + 1].kind == TOK_LPAREN);

            if (peek().kind == TOK_FUNCTION || cppCtorStyle)
            {
                if (peek().kind == TOK_FUNCTION)
                    consume(); // function
                Token funcNameTok = consume();

                bool isOperator = false;
                std::string opSymbol;
                std::string methodName;

                if (funcNameTok.kind == TOK_OPERATOR)
                {
                    isOperator = true;
                    if (peek().kind == TOK_LPAREN)
                    {
                        consume();
                        if (peek().kind != TOK_RPAREN)
                        {
                            err.Error(fileName, peek().line, peek().col, peek().text,
                                      "expected ')' in 'operator()'", getCurrentSourceLine());
                        }
                        else
                        {
                            consume();
                        }
                        opSymbol = "()";
                    }
                    else if (peek().kind == TOK_LBRACKET)
                    {
                        consume();
                        if (peek().kind != TOK_RBRACKET)
                        {
                            err.Error(fileName, peek().line, peek().col, peek().text,
                                      "expected ']' in 'operator[]'", getCurrentSourceLine());
                        }
                        else
                        {
                            consume();
                        }
                        opSymbol = "[]";
                    }
                    else if (peek().kind == TOK_PLUS || peek().kind == TOK_MINUS ||
                             peek().kind == TOK_STAR || peek().kind == TOK_SLASH ||
                             peek().kind == TOK_AND || peek().kind == TOK_OR)
                    {
                        opSymbol = consume().text; // "+" "-" "*" "/" "&&" "||"
                    }
                    else
                    {
                        err.Error(fileName, peek().line, peek().col, peek().text,
                                  "unsupported operator; supported: + - * / && || () []",
                                  getCurrentSourceLine());
                        opSymbol = "+";
                        if (peek().kind != TOK_EOF)
                            consume();
                    }
                }
                else if (funcNameTok.kind == TOK_IDENT)
                {
                    methodName = funcNameTok.text;
                }
                else
                {
                    err.Error(fileName, funcNameTok.line, funcNameTok.col, funcNameTok.text,
                              "expected method name", getCurrentSourceLine());
                    while (peek().kind != TOK_SEMICOLON && peek().kind != TOK_EOF)
                        consume();
                    continue;
                }

                bool isCtor = (!isOperator &&
                               (methodName == className ||
                                (!templateBaseName.empty() && methodName == templateBaseName)));

                expect(TOK_LPAREN, "expected '('");

                std::vector<ParamInfo> params;
                while (peek().kind != TOK_RPAREN && peek().kind != TOK_EOF)
                {
                    bool isRef = false, dummyConst2 = false;
                    std::string pType = parseType(dummyConst2, isRef);
                    Token pName = consume();
                    if (pName.kind != TOK_IDENT)
                    {
                        err.Error(fileName, pName.line, pName.col, pName.text,
                                  "expected parameter name", getCurrentSourceLine());
                        return;
                    }
                    params.push_back({pType, isRef, pName.text});
                    if (peek().kind == TOK_COMMA)
                        consume();
                }
                expect(TOK_RPAREN, "expected ')'");

                if (isOperator)
                {
                    if (opSymbol != "()" && params.size() != 1)
                    {
                        err.Error(fileName, funcNameTok.line, funcNameTok.col, opSymbol,
                                  "operator '" + opSymbol + "' must take exactly 1 parameter",
                                  getCurrentSourceLine());
                    }
                    for (auto &p : params)
                    {
                        if (p.isRef)
                        {
                            err.Error(fileName, funcNameTok.line, funcNameTok.col, p.name,
                                      "operator parameters cannot be ref", getCurrentSourceLine());
                        }
                    }
                    methodName = operatorMangledName(opSymbol, params);
                }

                // 可选 -> 返回类型
                std::string retType = "void";
                if (peek().kind == TOK_MINUS)
                {
                    consume();
                    if (peek().kind == TOK_GREATER)
                    {
                        consume();
                        bool dummyConst = false, dummyRef = false;
                        retType = parseType(dummyConst, dummyRef);
                    }
                    else
                    {
                        err.Error(fileName, funcNameTok.line, funcNameTok.col, "-",
                                  "expected '->' for method return type", getCurrentSourceLine());
                    }
                }

                if (isCtor)
                {
                    if (retType != "void")
                    {
                        err.Error(fileName, funcNameTok.line, funcNameTok.col, methodName,
                                  "constructors cannot have a return type", getCurrentSourceLine());
                    }
                    for (auto &p : params)
                    {
                        if (p.isRef)
                        {
                            err.Error(fileName, funcNameTok.line, funcNameTok.col, p.name,
                                      "constructor parameters cannot be ref", getCurrentSourceLine());
                        }
                    }
                    methodName = constructorMangledName(className, params);
                }

                if (peek().kind != TOK_LBRACE)
                {
                    err.Error(fileName, peek().line, peek().col, peek().text,
                              "expected '{' for method body", getCurrentSourceLine());
                    continue;
                }
                int bodyPos = pos;

                // 跳过方法体，第二趟再真正解析
                int depth = 0;
                while (peek().kind != TOK_EOF)
                {
                    if (peek().kind == TOK_LBRACE)
                        depth++;
                    else if (peek().kind == TOK_RBRACE)
                    {
                        depth--;
                        if (depth == 0)
                        {
                            consume();
                            break;
                        }
                    }
                    consume();
                }

                FuncInfo info{};
                info.isBuiltin = false;
                info.retType = retType;
                info.params = params;

                std::string declLine =
                    (funcNameTok.line >= 0 && (size_t)funcNameTok.line < sourceLines.size())
                        ? sourceLines[funcNameTok.line]
                        : "";

                if (isCtor)
                {
                    if (cls.methods.count(methodName))
                    {
                        std::string sig;
                        for (size_t i = 0; i < params.size(); i++)
                        {
                            if (i)
                                sig += ", ";
                            sig += params[i].type;
                        }
                        err.Error(fileName, funcNameTok.line, funcNameTok.col, className,
                                  "duplicate constructor with (" + sig + ")", declLine);
                    }
                    else
                    {
                        cls.methods[methodName] = info;
                        cls.methodAccess[methodName] = access;
                        cls.constructors.push_back(methodName);
                        methodDecls.push_back({methodName, info, access, bodyPos, true});
                    }
                }
                else if (isOperator)
                {
                    if (cls.methods.count(methodName))
                    {
                        std::string sig;
                        for (size_t i = 0; i < params.size(); i++)
                        {
                            if (i)
                                sig += ", ";
                            sig += params[i].type;
                        }
                        err.Error(fileName, funcNameTok.line, funcNameTok.col, opSymbol,
                                  "duplicate overload for 'operator" + opSymbol + "' with (" + sig + ")",
                                  declLine);
                    }
                    else
                    {
                        cls.methods[methodName] = info;
                        cls.methodAccess[methodName] = access;
                        cls.operatorOverloads[opSymbol].push_back(methodName);
                        methodDecls.push_back({methodName, info, access, bodyPos, false});
                    }
                }
                else
                {
                    if (cls.methods.count(methodName))
                    {
                        err.Error(fileName, funcNameTok.line, funcNameTok.col, methodName,
                                  "duplicate method '" + methodName + "'", declLine);
                    }
                    else
                    {
                        cls.methods[methodName] = info;
                        cls.methodAccess[methodName] = access;
                        methodDecls.push_back({methodName, info, access, bodyPos, false});
                    }
                }
                continue;
            }

            Token bad = peek();
            err.Error(fileName, bad.line, bad.col, bad.text,
                      "expected member declaration or access specifier", getCurrentSourceLine());
            consume();
        }

        if (!expect(TOK_RBRACE, "expected '}' after class body"))
            return;

        int endPos = pos; // 类体结束后的位置

        // ================= 第二趟：解析方法体 =================
        std::string savedClass = currentClass;
        for (auto &decl : methodDecls)
        {
            currentClass = className;
            pos = decl.bodyPos;
            expect(TOK_LBRACE, "expected '{'");

            LocalEnv methodEnv;
            Value thisVal{};
            thisVal.type = Value::OBJECT;
            thisVal.objectClass = className;
            methodEnv["this"] = thisVal;

            // 方法参数必须注册进作用域
            for (auto &p : decl.info.params)
            {
                Value v{};
                if (p.type == "int")
                    v.type = Value::INT;
                else if (p.type == "bool")
                    v.type = Value::BOOL;
                else if (p.type == "string")
                    v.type = Value::STRING;
                else if (p.type == "char")
                    v.type = Value::CHAR;
                else if (p.type == "float")
                    v.type = Value::FLOAT;
                else
                {
                    v.type = Value::OBJECT;
                    v.objectClass = p.type;
                }
                methodEnv[p.name] = v;
            }

            std::vector<std::string> retTypes;
            bool savedCtor = currentIsCtor;
            currentIsCtor = decl.isCtor;
            parseBlock(methodEnv, cls.methods[decl.name].body, retTypes);
            currentIsCtor = savedCtor;

            // auto 返回类型推导
            if (cls.methods[decl.name].retType == "auto")
            {
                if (retTypes.empty())
                {
                    cls.methods[decl.name].retType = "void";
                }
                else
                {
                    std::string deduced = retTypes[0];
                    for (auto &t : retTypes)
                    {
                        if (t != deduced)
                        {
                            err.Error(fileName, nameTok.line, nameTok.col, decl.name,
                                      "auto return type conflict: return type '" + t +
                                          "' vs previous '" + deduced + "'",
                                      getCurrentSourceLine());
                            break;
                        }
                    }
                    cls.methods[decl.name].retType = deduced;
                }
            }
        }
        currentClass = savedClass;
        pos = endPos; // 恢复到类声明之后
    }
}
#endif
