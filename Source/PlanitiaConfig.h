#ifndef PLANITIA_CONFIG_H
#define PLANITIA_CONFIG_H

#include <map>
#include <string>

struct PlanitiaConfigInfo
{
    int datatype = 0;
    float numdata = 0;
    std::string stringdata;
};

enum
{
    PLANITIA_DATA_STRING = 0,
    PLANITIA_DATA_NUMBER
};

#define DATA_STRING PLANITIA_DATA_STRING
#define DATA_NUMBER PLANITIA_DATA_NUMBER

void LoadConfigFile(std::map<std::string, PlanitiaConfigInfo>& configinfomap, const std::string& fileName);

#endif