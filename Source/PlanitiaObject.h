#ifndef PLANITIA_OBJECT_H
#define PLANITIA_OBJECT_H

#include <string>

class PlanitiaObject
{
public:
    PlanitiaObject() = default;
    virtual ~PlanitiaObject() = default;
    virtual void Init(const std::string& configfile) = 0;
    virtual void Shutdown() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
};

#endif