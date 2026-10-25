#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include "lexer.hpp"

namespace tool::sym
{
    using namespace tool::lex;

    struct ParamInfo
    {
        std::string type;
        bool isRef;
        std::string name;
    };

    struct FuncInfo
    {
        bool isBuiltin;
        std::string retType;
        std::vector<ParamInfo> params;
        RpnList body;
        std::function<Value(const std::vector<Value> &)> handler;
    };

    inline std::unordered_map<std::string, FuncInfo> globalFuncTable;

    inline FuncInfo *lookupFunc(const std::string &name)
    {
        auto it = globalFuncTable.find(name);
        if (it == globalFuncTable.end())
            return nullptr;
        return &it->second;
    }

    // ===== 类信息结构体 =====
    enum class Access
    {
        Private,
        Protected,
        Public
    };

    struct MemberInfo
    {
        std::string type;
        Access access = Access::Private;

        // 数组成员支持（array T name[] / array T name[N]）
        bool isArray = false;
        Value::Type elemType = Value::INT;
        int arrayDepth = 0;
        int arraySize = 0;
    };

    struct ClassInfo
    {
        std::string name;
        std::unordered_map<std::string, MemberInfo> members;      // 成员名 → 类型 + 访问级别
        std::unordered_map<std::string, FuncInfo> methods;        // 方法名 → 函数体（运算符用 mangled 名）
        std::unordered_map<std::string, Access> methodAccess;     // 方法名 → 访问级别
        // 运算符重载索引：符号（"+"、"()"、"[]" …）→ 各重载的 mangled 方法名
        std::unordered_map<std::string, std::vector<std::string>> operatorOverloads;
        // 构造函数重载索引（mangled 方法名）
        std::vector<std::string> constructors;
    };

    // 全局类表
    inline std::unordered_map<std::string, ClassInfo> globalClassTable;

    // 查找类
    inline ClassInfo *lookupClass(const std::string &name)
    {
        auto it = globalClassTable.find(name);
        return it == globalClassTable.end() ? nullptr : &it->second;
    }

    // ===== 类模板 =====
    struct TemplateParam
    {
        bool isType;     // true: type X；false: var V
        std::string name;
    };

    struct TemplateInfo
    {
        std::string name;
        std::vector<TemplateParam> params;
        std::vector<Token> bodyTokens;      // 类体 token（含 { }）
        std::vector<std::string> sourceLines; // 模板定义所在文件的源码行
        std::string fileName;
        int line = 0;
        int col = 0;
    };

    inline std::unordered_map<std::string, TemplateInfo> classTemplates;

    inline TemplateInfo *lookupTemplate(const std::string &name)
    {
        auto it = classTemplates.find(name);
        return it == classTemplates.end() ? nullptr : &it->second;
    }
}

#endif
