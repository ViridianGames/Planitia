#include "PlanitiaConfig.h"
#include "Geist/Logging.h"

#include <fstream>
#include <sstream>

static void StripCR(std::string& s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == ' '))
        s.pop_back();
}

void LoadConfigFile(std::map<std::string, PlanitiaConfigInfo>& configinfomap, const std::string& fileName)
{
    std::ifstream instream(fileName);
    if (instream.fail())
    {
        throw std::string("Could not open " + fileName);
    }

    configinfomap.clear();
    std::string line;
    while (std::getline(instream, line))
    {
        StripCR(line);
        auto idx = line.find('=');
        if (idx == std::string::npos) continue;

        std::string leftside = line.substr(0, idx);
        while (!leftside.empty() && leftside.back() == ' ') leftside.pop_back();

        std::string rightside = line.substr(idx + 1);
        while (!rightside.empty() && rightside.front() == ' ') rightside.erase(0, 1);
        StripCR(rightside);

        PlanitiaConfigInfo info;
        if (rightside.find_first_not_of("0123456789-.") == std::string::npos)
        {
            info.datatype = DATA_NUMBER;
            info.numdata = std::stof(rightside);
        }
        else
        {
            info.datatype = DATA_STRING;
            info.stringdata = rightside;
        }
        configinfomap[leftside] = info;
    }
}