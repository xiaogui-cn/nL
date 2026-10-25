#ifndef FILE_C_
#define FILE_C_

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>

namespace tool
{
    namespace file
    {
        std::vector<std::string> readFile(const std::string &filePath)
        {
            std::vector<std::string> lines;
            std::ifstream fin(filePath);
            if (!fin.is_open())
            {
                throw std::runtime_error("unknown file");
                return lines;
            }

            std::string line;
            while (getline(fin, line))
                lines.push_back(line);

            fin.close();
            return lines;
        }
    }
}

#endif