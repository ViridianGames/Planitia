#include "PlanitiaResourceManager.h"

Bitmap* PlanitiaResourceManager::GetBitmap(const std::string& bitmapname, bool test)
{
    std::string key = NormalizePath(bitmapname);
    auto node = m_BitmapList.find(key);
    if (test && node != m_BitmapList.end())
        return node->second;

    auto* temp = new Bitmap();
    temp->Load(key);
    m_BitmapList[key] = temp;
    return temp;
}

PlanitiaMesh* PlanitiaResourceManager::GetMesh(const std::string& meshname)
{
    std::string key = NormalizePath(meshname);
    auto node = m_MeshList.find(key);
    if (node != m_MeshList.end())
        return node->second;

    auto* temp = new PlanitiaMesh();
    temp->Load(key);
    m_MeshList[key] = temp;
    return temp;
}

void PlanitiaResourceManager::Init(const std::string&) {}
void PlanitiaResourceManager::Update() {}
void PlanitiaResourceManager::Draw() {}

void PlanitiaResourceManager::Shutdown()
{
    for (auto& pair : m_BitmapList) delete pair.second;
    m_BitmapList.clear();
    for (auto& pair : m_MeshList) delete pair.second;
    m_MeshList.clear();
}