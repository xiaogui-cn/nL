#ifndef LEXER_HPP
#define LEXER_HPP
#include <iostream>
#include <vector>
#include <string>
#include <cctype>
#include "errorC.hpp"

namespace tool::lex
{
    struct Value
    {
        enum Type
        {
            INT,
            BOOL,
            STRING,
            V_VOID,
            RETURNED,
            CHAR,
            ARRAY,
            OBJECT,
            FLOAT,
            VD
        } type;
        int intVal;
        bool boolVal;
        float floatVal = 0.0f;
        std::string strVal;
        char charVal;
        bool isConst = false;  // 是否只读
        bool isRef = false;    // 是否是引用变量
        std::string refTarget; // 引用指向的变量名
        int refFrame = -1;     // 引用目标所在的调用帧（-1 表示按上一层推断）

        Value::Type elemType;        // 数组的叶子元素类型
        int arrayDepth = 0;          // 数组维数（1 = 一维）
        std::vector<Value> arrayVal; // 数组元素存储容器

        std::string objectClass;
        std::vector<std::string> memberNames;
        std::vector<Value> memberValues;

        static Value Char(char c)
        {
            Value o{};
            o.type = CHAR;
            o.charVal = c;
            o.intVal = (unsigned char)c; // 同步 ASCII 整数值
            return o;
        }
    };

    enum TokenKind
    {
        TOK_EOF = 0,
        TOK_FUNCTION,
        TOK_RETURN,
        TOK_AUTO,
        TOK_VAR,
        TOK_TRUE,
        TOK_BOOL,
        TOK_FALSE,
        TOK_INT,
        TOK_VD,
        TOK_VOID,
        TOK_STRING,
        TOK_CHAR,
        TOK_IF,
        TOK_ELIF,
        TOK_ELSE,
        TOK_FOR,
        TOK_ARRAY,
        TOK_ALLOC,      // alloc 内存分配
        TOK_ASSIGN_ARR, // assign<a, b> 数组赋值
        TOK_POINTER,    // pointer int *p
        TOK_CLASS,      // class 类声明
        TOK_NEW,        // new 实例化对象
        TOK_THIS,       // this 指针
        TOK_FLOAT,
        TOK_PUBLIC,
        TOK_PRIVATE,
        TOK_PROTECTED,
        TOK_OPERATOR, // operator()

        TOK_JUMP,

        // 特别功能
        TOK_TANT, // tant (tand to)
        TOK_FACT, // fact
        TOK_SQRT, // sqrt

        TOK_IDENT,
        TOK_NUMBER,
        TOK_STRLIT,
        TOK_LPAREN,
        TOK_RPAREN,
        TOK_LBRACE,
        TOK_RBRACE,
        TOK_LBRACKET,
        TOK_RBRACKET,
        TOK_COMMA,
        TOK_SEMICOLON,
        TOK_ASSIGN,
        TOK_PLUS,
        TOK_MINUS,
        TOK_STAR,
        TOK_SLASH,
        TOK_LESS,    // <
        TOK_SHL,     // <<
        TOK_GREATER, // >
        TOK_GE,      // >=
        TOK_LE,      // <=
        TOK_EQ,      // ==
        TOK_NE,      // !=
        TOK_AND,     // &&
        TOK_OR,      // ||
        TOK_AMP,     // &（取地址）
        TOK_INC,     // ++
        TOK_BANG,    // !

        TOK_DOT,
        TOK_COLON,
        TOK_CHAR_LIT,
        TOK_CONST,   // const 只读
        TOK_REF,     // ref 引用
        TOK_PERCENT, // %
        TOK_APPROX_EQ,
        TOK_DOLLAR_TYPE
    };

    enum RpnOpKind
    {
        OP_PUSH_INT,
        OP_PUSH_BOOL,
        OP_PUSH_STR,
        OP_PUSH_CHAR,
        OP_PUSH_REF, // 压入变量引用（用于ref参数传参）
        OP_PUSH_VD,

        OP_PUSH_FLOAT,
        OP_STRING_INDEX,
        OP_ADD,
        OP_SUB,
        OP_MUL,
        OP_DIV,
        OP_CALL,
        OP_RET,
        OP_POP,
        OP_LOAD_VAR,
        OP_STORE_VAR,
        OP_JMP_IF_FALSE, // 栈顶条件为假时跳转到目标地址
        OP_JMP,          // 无条件跳转到目标地址
        OP_NEW_ARRAY,    // 创建数组
        OP_ARRAY_LOAD,   // 数组下标读
        OP_ARRAY_STORE,  // 数组下标写
        OP_AND,          // &&
        OP_OR,           // ||
        OP_JUMP_BREAK,

        OP_NEW_OBJECT,   // 新建对象实例
        OP_MEMBER_LOAD,  // 对象成员读取
        OP_MEMBER_STORE, // 对象成员写入
        OP_PUSH_THIS,    // 压入 this 指针
        OP_PUSH_DEFAULT,

        OP_MOD,
        OP_GT,
        OP_LT,
        OP_GE,
        OP_LE,
        OP_EQ,
        OP_NE,
        OP_APPROX_EQ,
        OP_NOT,

        // 特别功能
        OP_FACT,
        OP_SQRT,
        OP_CAST,

        // 多维数组
        OP_NEW_ARRAY_ND,  // 按维数创建（intArg=元素类型，intArg2=维数）
        OP_ARRAY_STORE_N, // 多维下标写入（intArg=下标个数）

        OP_PUSH_OBJ_REF, // 压入对象变量的引用（方法调用用）

        OP_CALL_CTOR // 调用构造函数（intArg=参数个数，strArg=mangled 名）
    };

    struct Token
    {
        TokenKind kind;
        std::string text;
        int line;
        int col;
        std::string fileName;
    };

    struct RpnOp
    {
        RpnOpKind op;
        int intArg;
        bool boolArg;
        std::string strArg;
        char charVal;
        float floatArg;
        int intArg2 = 0; // 第二整数参数（多维数组用）

        static RpnOp PushFloat(float v)
        {
            RpnOp o;
            o.op = OP_PUSH_FLOAT;
            o.intArg = 0;
            o.boolArg = false;
            o.strArg = "";
            o.charVal = 0;
            o.floatArg = v;
            return o;
        }

        static RpnOp PushInt(int v) { return {OP_PUSH_INT, v, false, "", 0}; }
        static RpnOp PushBool(bool v) { return {OP_PUSH_BOOL, 0, v, "", 0}; }
        static RpnOp PushStr(std::string s) { return {OP_PUSH_STR, 0, false, std::move(s), 0}; }
        static RpnOp PushChar(char c) { return {OP_PUSH_CHAR, 0, false, "", c}; }
        static RpnOp StringIndex() { return {OP_STRING_INDEX, 0, false, "", 0}; }
        static RpnOp BinOp(RpnOpKind k) { return {k, 0, false, "", 0}; }
        static RpnOp Call(std::string fname, int argCnt) { return {OP_CALL, argCnt, false, std::move(fname), 0}; }
        static RpnOp Ret() { return {OP_RET, 0, false, "", 0}; }
        static RpnOp Pop() { return {OP_POP, 0, false, "", 0}; }
        static RpnOp LoadVar(std::string name) { return {OP_LOAD_VAR, 0, false, std::move(name), 0}; }
        static RpnOp StoreVar(std::string name) { return {OP_STORE_VAR, 0, false, std::move(name), 0}; }
        static RpnOp JmpIfFalse(int target) { return {OP_JMP_IF_FALSE, target, false, "", 0}; }
        static RpnOp Jmp(int target) { return {OP_JMP, target, false, "", 0}; }

        static RpnOp PushVd()
        {
            RpnOp o{};
            o.op = OP_PUSH_VD;
            o.intArg = 0;
            o.boolArg = false;
            o.strArg = "";
            o.charVal = 0;
            o.floatArg = 0;
            o.intArg2 = 0;
            return o;
        }

        static RpnOp PushRef(std::string name)
        {
            RpnOp o;
            o.op = OP_PUSH_REF;
            o.strArg = std::move(name);
            o.intArg = 0;
            o.boolArg = false;
            o.charVal = 0;
            return o;
        }

        static RpnOp PushDefault(Value::Type ty)
        {
            RpnOp o{};
            o.op = OP_PUSH_DEFAULT;
            o.intArg = static_cast<int>(ty);
            o.boolArg = false;
            o.strArg = "";
            o.charVal = 0;
            o.floatArg = 0;
            o.intArg2 = 0;
            return o;
        }

        static RpnOp NewArray(Value::Type elemType) { return {OP_NEW_ARRAY, (int)elemType, false, "", 0}; }
        static RpnOp ArrayLoad() { return {OP_ARRAY_LOAD, 0, false, "", 0}; }
        static RpnOp ArrayStore() { return {OP_ARRAY_STORE, 0, false, "", 0}; }

        static RpnOp NewArrayND(Value::Type elemType, int ndims)
        {
            RpnOp o = BinOp(OP_NEW_ARRAY_ND);
            o.intArg = (int)elemType;
            o.intArg2 = ndims;
            return o;
        }

        static RpnOp ArrayStoreN(int ndims)
        {
            RpnOp o = BinOp(OP_ARRAY_STORE_N);
            o.intArg = ndims;
            return o;
        }

        static RpnOp PushObjRef(std::string name)
        {
            RpnOp o = BinOp(OP_PUSH_OBJ_REF);
            o.strArg = std::move(name);
            return o;
        }

        static RpnOp CallCtor(std::string name, int argCnt)
        {
            RpnOp o = BinOp(OP_CALL_CTOR);
            o.intArg = argCnt;
            o.strArg = std::move(name);
            return o;
        }

        static RpnOp NewObject(std::string className) { return {OP_NEW_OBJECT, 0, false, std::move(className), 0}; }
        static RpnOp MemberLoad(std::string memberName) { return {OP_MEMBER_LOAD, 0, false, std::move(memberName), 0}; }
        static RpnOp MemberStore(std::string memberName) { return {OP_MEMBER_STORE, 0, false, std::move(memberName), 0}; }
        static RpnOp PushThis() { return {OP_PUSH_THIS, 0, false, "", 0}; }
    };
    using RpnList = std::vector<RpnOp>;
    class Lexer
    {
    public:
        Value parseExpression();
        // 项解析（乘除，中优先级）
        Value parseTerm();
        // 因子解析（字面量、括号、函数调用，最高优先级）
        Value parseFactor();

        std::string source;
        int pos;
        int line;
        int col;
        error &err;
        std::string fileName;

        Lexer(const std::string &src, error &e, const std::string &fname)
            : source(src), pos(0), line(0), col(0), err(e), fileName(fname) {}

        void skipWhitespaceAndComment()
        {
            while (pos < (int)source.size())
            {
                char c = source[pos];
                if (c == ' ' || c == '\t' || c == '\r')
                {
                    pos++;
                    col++; // 同步列号
                }
                else if (c == '\n')
                {
                    pos++;
                    line++;
                    col = 0; // 换行重置列号
                }
                else if (c == '/' && pos + 1 < (int)source.size() && source[pos + 1] == '/')
                {
                    while (pos < (int)source.size() && source[pos] != '\n')
                    {
                        pos++;
                        col++;
                    }
                }
                else if (c == '/' && pos + 1 < (int)source.size() && source[pos + 1] == '*')
                {
                    pos += 2;
                    col += 2;
                    while (pos < (int)source.size())
                    {
                        if (source[pos] == '\n')
                        {
                            line++;
                            col = 0;
                            pos++;
                        }
                        else if (source[pos] == '*' && pos + 1 < (int)source.size() && source[pos + 1] == '/')
                        {
                            pos += 2;
                            col += 2;
                            break;
                        }
                        else
                        {
                            pos++;
                            col++;
                        }
                    }
                }
                else
                {
                    break;
                }
            }
        }

        // 十六进制字符值；不是十六进制返回 -1
        static int hexValue(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        }

        // 解码一个转义序列：pos 指向反斜杠，返回解码后的字符并推进 pos/col
        char readEscapedChar()
        {
            int escLine = line;
            int escCol = col; // 反斜杠所在列

            pos++; // 跳过 '\'
            col++;

            if (pos >= (int)source.size())
            {
                err.Error(fileName, escLine, escCol, "\\", "unterminated escape sequence", "");
                return '\\';
            }

            char e = source[pos];
            // 反斜杠后直接换行：不消耗换行，交给调用方报"未闭合"
            if (e == '\n')
            {
                err.Error(fileName, escLine, escCol, "\\", "unterminated escape sequence", "");
                return '\\';
            }

            switch (e)
            {
            case 'n':
                pos++;
                col++;
                return '\n';
            case 't':
                pos++;
                col++;
                return '\t';
            case 'r':
                pos++;
                col++;
                return '\r';
            case 'a':
                pos++;
                col++;
                return '\a';
            case 'b':
                pos++;
                col++;
                return '\b';
            case 'f':
                pos++;
                col++;
                return '\f';
            case 'v':
                pos++;
                col++;
                return '\v';
            case '0':
                pos++;
                col++;
                return '\0';
            case '\\':
                pos++;
                col++;
                return '\\';
            case '\'':
                pos++;
                col++;
                return '\'';
            case '"':
                pos++;
                col++;
                return '"';
            case '?':
                pos++;
                col++;
                return '?';
            case 'x':
            {
                pos++; // 跳过 'x'
                col++;
                int val = 0;
                int digits = 0;
                while (digits < 2 && pos < (int)source.size() && hexValue(source[pos]) >= 0)
                {
                    val = val * 16 + hexValue(source[pos]);
                    pos++;
                    col++;
                    digits++;
                }
                if (digits == 0)
                {
                    err.Error(fileName, escLine, escCol, "\\x",
                              "\\x escape expects 1-2 hex digits", "");
                    return 'x';
                }
                return (char)val;
            }
            default:
                // 未知转义：报错，但保留字符本身，避免连锁错误
                err.Error(fileName, escLine, escCol, std::string("\\") + e,
                          std::string("unknown escape sequence '\\") + e + "'", "");
                pos++;
                col++;
                return e;
            }
        }

        // 读取下一个 Token
        Token nextToken()
        {
            skipWhitespaceAndComment();
            if (pos >= (int)source.size())
                return {TOK_EOF, "", line, col, fileName};

            char c = source[pos];
            int startCol = col; // 记录Token起始列

            // 标识符 / 关键字
            if (std::isalpha((unsigned char)c) || c == '_')
            {
                int start = pos;
                while (pos < (int)source.size() && (std::isalnum((unsigned char)source[pos]) || source[pos] == '_'))
                {
                    pos++;
                    col++;
                }
                std::string word = source.substr(start, pos - start);
                TokenKind k = TOK_IDENT;
                if (word == "function")
                    k = TOK_FUNCTION;
                else if (word == "return")
                    k = TOK_RETURN;
                else if (word == "bool")
                    k = TOK_BOOL;
                else if (word == "int")
                    k = TOK_INT;
                else if (word == "void")
                    k = TOK_VOID;
                else if (word == "string")
                    k = TOK_STRING;
                else if (word == "true")
                    k = TOK_TRUE;
                else if (word == "false")
                    k = TOK_FALSE;
                else if (word == "var")
                    k = TOK_VAR;
                else if (word == "char")
                    k = TOK_CHAR;
                else if (word == "auto")
                    k = TOK_AUTO;
                else if (word == "if")
                    k = TOK_IF;
                else if (word == "elif")
                    k = TOK_ELIF;
                else if (word == "else")
                    k = TOK_ELSE;
                else if (word == "const")
                    k = TOK_CONST;
                else if (word == "ref")
                    k = TOK_REF;
                else if (word == "for")
                    k = TOK_FOR;
                else if (word == "array")
                    k = TOK_ARRAY;
                else if (word == "alloc")
                    k = TOK_ALLOC;
                else if (word == "assign")
                    k = TOK_ASSIGN_ARR;
                else if (word == "operator")
                    k = TOK_OPERATOR;
                else if (word == "class")
                    k = TOK_CLASS;
                else if (word == "new")
                    k = TOK_NEW;
                else if (word == "this")
                    k = TOK_THIS;
                else if (word == "float")
                    k = TOK_FLOAT;
                else if (word == "public")
                    k = TOK_PUBLIC;
                else if (word == "private")
                    k = TOK_PRIVATE;
                else if (word == "protected")
                    k = TOK_PROTECTED;
                else if (word == "tant")
                    k = TOK_TANT;
                else if (word == "fact")
                    k = TOK_FACT;
                else if (word == "sqrt")
                    k = TOK_SQRT;
                else if (word == "vd")
                    k = TOK_VD;
                else if (word == "jump") // 新增
                    k = TOK_JUMP;
                return {k, word, line, startCol};
            }

            // 数字字面量
            if (isdigit((unsigned char)c) || (c == '.' && pos + 1 < (int)source.size() && isdigit((unsigned char)source[pos + 1])))
            {
                std::string num;
                bool isFloat = false;
                while (pos < (int)source.size() && (isdigit((unsigned char)source[pos]) || source[pos] == '.'))
                {
                    if (source[pos] == '.')
                    {
                        if (isFloat)
                            break; // 第二个小数点直接停止
                        isFloat = true;
                    }
                    num += source[pos];
                    pos++;
                    col++;
                }
                if (isFloat)
                    return {TOK_FLOAT, num, line, startCol, fileName};
                else
                    return {TOK_NUMBER, num, line, startCol, fileName};
            }

            // 字符串字面量（支持转义）
            if (c == '"')
            {
                int strLine = line;
                pos++;
                col++;
                std::string value;
                bool closed = false;
                while (pos < (int)source.size())
                {
                    char sc = source[pos];
                    if (sc == '"')
                    {
                        pos++;
                        col++;
                        closed = true;
                        break;
                    }
                    if (sc == '\n')
                        break; // 字符串不允许跨行
                    if (sc == '\\')
                    {
                        value += readEscapedChar();
                        continue;
                    }
                    value += sc;
                    pos++;
                    col++;
                }
                if (!closed)
                {
                    err.Error(fileName, strLine, startCol, "\"",
                              "unterminated string literal", "");
                }
                return {TOK_STRLIT, value, strLine, startCol, fileName};
            }

            // 字符字面量（支持转义，如 '\n'、'\x41'）
            if (c == '\'')
            {
                int ln = line;
                int sc = startCol;
                pos++;
                col++;
                if (pos >= (int)source.size() || source[pos] == '\n')
                {
                    err.Error(fileName, ln, sc, "'", "unclosed char literal", "");
                    return {TOK_CHAR_LIT, "", ln, sc, fileName};
                }
                if (source[pos] == '\'')
                {
                    // ''
                    err.Error(fileName, ln, sc, "''", "empty char literal", "");
                    pos++;
                    col++;
                    return {TOK_CHAR_LIT, "", ln, sc, fileName};
                }

                char ch;
                if (source[pos] == '\\')
                    ch = readEscapedChar();
                else
                {
                    ch = source[pos];
                    pos++;
                    col++;
                }

                // 找闭合单引号
                if (pos >= (int)source.size() || source[pos] == '\n')
                {
                    err.Error(fileName, ln, sc, std::string(1, ch), "unclosed char literal", "");
                    return {TOK_CHAR_LIT, "", ln, sc, fileName};
                }
                if (source[pos] != '\'')
                {
                    err.Error(fileName, ln, sc, std::string(1, ch),
                              "char literal must be single character 'x'", "");
                    while (pos < (int)source.size() && source[pos] != '\'' && source[pos] != '\n')
                    {
                        pos++;
                        col++;
                    }
                    if (pos < (int)source.size() && source[pos] == '\'')
                    {
                        pos++;
                        col++;
                    }
                    return {TOK_CHAR_LIT, "", ln, sc, fileName};
                }
                pos++;
                col++; // 吃掉结尾 '
                return {TOK_CHAR_LIT, std::string(1, ch), ln, sc, fileName};
            }

            // 双字符比较运算符（优先匹配长符号）
            if (pos + 1 < (int)source.size())
            {
                char next = source[pos + 1];
                if (c == '>' && next == '=')
                {
                    pos += 2;
                    col += 2;
                    return {TOK_GE, ">=", line, startCol, fileName};
                }
                if (c == '<' && next == '<')
                {
                    pos += 2;
                    col += 2;
                    return {TOK_SHL, "<<", line, startCol, fileName};
                }
                if (c == '<' && next == '=')
                {
                    pos += 2;
                    col += 2;
                    return {TOK_LE, "<=", line, startCol, fileName};
                }
                if (c == '=' && next == '=')
                {
                    pos += 2;
                    col += 2;
                    return {TOK_EQ, "==", line, startCol, fileName};
                }
                if (c == '!' && next == '=')
                {
                    pos += 2;
                    col += 2;
                    return {TOK_NE, "!=", line, startCol, fileName};
                }
                if (c == '~' && next == '=')
                {
                    pos += 2;
                    col += 2;
                    return {TOK_APPROX_EQ, "~=", line, startCol, fileName};
                }
                if (c == '&' && next == '&')
                {
                    pos += 2;
                    col += 2;
                    return {TOK_AND, "&&", line, startCol, fileName};
                }
                if (c == '|' && next == '|')
                {
                    pos += 2;
                    col += 2;
                    return {TOK_OR, "||", line, startCol, fileName};
                }
            }

            if (c == '!')
            {
                pos++;
                col++;
                return {TOK_BANG, "!", line, startCol, fileName};
            }

            // 单字符符号
            pos++;
            col++;
            switch (c)
            {
            case '(':
                return {TOK_LPAREN, "(", line, startCol, fileName};
            case ')':
                return {TOK_RPAREN, ")", line, startCol, fileName};
            case '{':
                return {TOK_LBRACE, "{", line, startCol, fileName};
            case '}':
                return {TOK_RBRACE, "}", line, startCol, fileName};
            case ',':
                return {TOK_COMMA, ",", line, startCol, fileName};
            case ';':
                return {TOK_SEMICOLON, ";", line, startCol, fileName};
            case '=':
                return {TOK_ASSIGN, "=", line, startCol, fileName};
            case '+':
                return {TOK_PLUS, "+", line, startCol, fileName};
            case '-':
                return {TOK_MINUS, "-", line, startCol, fileName};
            case '*':
                return {TOK_STAR, "*", line, startCol, fileName};
            case '/':
                return {TOK_SLASH, "/", line, startCol, fileName};
            case '<':
                return {TOK_LESS, "<", line, startCol, fileName};
            case '>':
                return {TOK_GREATER, ">", line, startCol, fileName};
            case '.':
                return {TOK_DOT, ".", line, startCol, fileName};
            case '[':
                return {TOK_LBRACKET, "[", line, startCol, fileName};
            case ']':
                return {TOK_RBRACKET, "]", line, startCol, fileName};
            case '%':
                return {TOK_PERCENT, "%", line, startCol, fileName};
            case ':':
                return {TOK_COLON, ":", line, startCol, fileName};
            case '$':
            {
                int dollarLine = line;
                int dollarCol = startCol;
                pos++;
                col++;
                // $后面必须是字母或下划线
                if (pos >= (int)source.size() ||
                    (!std::isalpha(static_cast<unsigned char>(source[pos])) && source[pos] != '_'))
                {
                    err.Error(fileName, dollarLine, dollarCol, "$",
                              "$ must be followed by type identifier", "");
                    return {TOK_EOF, "", dollarLine, dollarCol, fileName};
                }
                int nameStart = pos;
                while (pos < (int)source.size() &&
                       (std::isalnum(static_cast<unsigned char>(source[pos])) || source[pos] == '_'))
                {
                    pos++;
                    col++;
                }
                std::string typeName = source.substr(nameStart, pos - nameStart);
                return {TOK_DOLLAR_TYPE, typeName, dollarLine, dollarCol, fileName};
            }

            default:
                err.Error(fileName, line, startCol, std::string(1, c), "unexpected character", "");
                return {TOK_EOF, "", line, startCol, fileName};
            }
        }

        // 一次性扫描全部 Token
        std::vector<Token> tokenize()
        {
            std::vector<Token> toks;
            Token t;
            do
            {
                t = nextToken();
                toks.push_back(t);
            } while (t.kind != TOK_EOF);
            return toks;
        }
    };

    // ===== 调试：打印所有分词 =====
    inline void dumpTokens(const std::vector<Token> &toks)
    {
        std::cout << "\n===== Lexer Tokens =====\n";
        for (size_t i = 0; i < toks.size(); i++)
        {
            std::cout << "[" << i << "] line:" << toks[i].line
                      << " col:" << toks[i].col
                      << " kind:" << (int)toks[i].kind
                      << " text:\"" << toks[i].text << "\"\n";
        }
        std::cout << "========================\n";
    }
}
#endif
