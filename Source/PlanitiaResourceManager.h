#ifndef PLANITIA_RESOURCE_MANAGER_H
#define PLANITIA_RESOURCE_MANAGER_H

#include "PlanitiaObject.h"
#include "PlanitiaPrimitives.h"
#include <map>
#include <string>

class PlanitiaResourceManager : public PlanitiaObject
{
public:
    std::map<std::string, Bitmap*> m_BitmapList;
    std::map<std::string, PlanitiaMesh*> m_MeshList;

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;

    Bitmap* GetBitmap(const std::string& bitmapname, bool test = true);
    PlanitiaMesh* GetMesh(const std::string& meshname);
};

#endif