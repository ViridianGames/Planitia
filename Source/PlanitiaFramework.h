#ifndef PLANITIA_FRAMEWORK_H
#define PLANITIA_FRAMEWORK_H

#include <string>

namespace PlanitiaFramework
{
    void Init(const std::string& configfile);
    void Shutdown();
    void Update();
    void DrawPost();
    bool IsActive();
}

#endif