#ifndef STRING_C_
#define STRING_C_

#include <string>

namespace tool
{
    namespace str
    {
        std::string ltrimF(std::string s)
        {
            size_t i = 0;
            while (i < s.size() && s[i] == ' ')
                i++;
            return s.substr(i);
        }

        std::string ltrimL(std::string line)
        {
            size_t cmt = line.find("//");
            if (cmt != std::string::npos)
                line = line.substr(0, cmt);

            size_t end = line.find_last_not_of(' ');
            if (end != std::string::npos)
                line = line.substr(0, end + 1);
            else
                line = "";
            return line;
        }
    }

    std::string firstWord(std::string s)
    {
        std::string s1 = "";
        for (int i = 0; i < s.size(); ++i)
        {
            if (s[i] == ' ' || s[i] == '[' || s[i] == '(')
                return s1;
            s1 += s[i];
        }
        return s1;
    }
}
#endif