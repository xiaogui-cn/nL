#ifndef ERROR_C_
#define ERROR_C_
#include <iostream>
#include <string>
#include <vector>
class error
{
public:
    std::vector<std::string> fileName;
    std::vector<int> line;
    std::vector<int> col; //   新增：错误列号
    std::vector<std::string> about;
    std::vector<std::string> why;
    std::vector<std::string> cur;

    /// @param fileName_ 输入文件名
    /// @param line_ 行号
    /// @param col_ 列号
    /// @param about_ 关于字串
    /// @param why_ 原因
    /// @param cur_ 整行内容
    void Error(std::string fileName_, int line_, int col_, std::string about_, std::string why_, std::string cur_)
    {
        fileName.push_back(fileName_);
        line.push_back(line_);
        col.push_back(col_);
        about.push_back(about_);
        why.push_back(why_);
        cur.push_back(cur_);
    }

    /// @brief 输出结果
    void Print()
    {
        for (size_t i = 0; i < why.size(); ++i)
        {
            std::cout << "nL.exe: \033[91merror: \033[0m"
                      << fileName[i] << ":"
                      << line[i] + 1 << ":"
                      << col[i] + 1 << ": "
                      << "\n   "
                      << why[i] << ':' << std::endl;
            std::cout << "     " << cur[i] << std::endl;
            std::cout << "     ";
            //   直接按列号缩进，不再用字符串查找
            for (int k = 0; k < col[i]; k++)
                std::cout << ' ';
            std::cout << "\033[91m";
            std::cout << '^';
            size_t markLen = about[i].size();
            for (size_t k = 1; k < markLen; k++)
                std::cout << '~';
            std::cout << "\n\033[0m"
                      << std::endl;
        }
    }
};
#endif
