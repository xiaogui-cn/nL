#ifndef PREPROCESSOR_HPP
#define PREPROCESSOR_HPP
#include "lexer.hpp"
#include <unordered_map>
#include <vector>

namespace tool::pre
{
    using namespace tool::lex;

    class Preprocessor
    {
    public:
        // 宏表：宏名 -> 替换 Token 序列
        std::unordered_map<std::string, std::vector<Token>> macroMap;

        std::vector<Token> process(const std::vector<Token> &rawTokens)
        {
            std::vector<Token> result;
            int i = 0;
            int n = rawTokens.size();

            while (i < n)
            {
                const Token &t = rawTokens[i];

                // ===== 识别 define 宏定义 =====
                if (t.kind == TOK_IDENT && t.text == "define")
                {
                    i++; // 跳过 define 关键字

                    // 读取宏名（必须是标识符）
                    if (i >= n || rawTokens[i].kind != TOK_IDENT)
                    {
                        // 语法错误：跳过整行
                        int errLine = t.line;
                        while (i < n && rawTokens[i].line == errLine)
                            i++;
                        continue;
                    }
                    std::string macroName = rawTokens[i].text;
                    int defLine = rawTokens[i].line;
                    i++;

                    // 收集替换内容：同一行内所有后续 Token
                    std::vector<Token> replaceTokens;
                    while (i < n && rawTokens[i].line == defLine)
                    {
                        replaceTokens.push_back(rawTokens[i]);
                        i++;
                    }

                    // 存入宏表
                    macroMap[macroName] = replaceTokens;
                    continue;
                }

                // ===== 普通标识符：查宏替换 =====
                if (t.kind == TOK_IDENT)
                {
                    auto it = macroMap.find(t.text);
                    if (it != macroMap.end())
                    {
                        // 插入替换序列
                        for (const auto &rt : it->second)
                        {
                            result.push_back(rt);
                        }
                        i++;
                        continue;
                    }
                }

                // ===== 字符串/其他 Token：直接保留 =====
                // 注意：TOK_STRLIT 字符串永远不会进入上面的替换分支
                result.push_back(t);
                i++;
            }

            return result;
        }
    };
}

#endif
