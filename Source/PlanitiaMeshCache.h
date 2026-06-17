#ifndef PLANITIA_MESH_CACHE_H
#define PLANITIA_MESH_CACHE_H

#include "PlanitiaD3DDevice.h"
#include "Geist/Primitives.h"
#include <string>
#include <vector>

class LoadedMesh
{
public:
    PlanitiaVertexBuffer* m_VertexBuffer = nullptr;
    int m_NumberOfVertices = 0;
    std::vector<Vertex> m_VertexList;

    ~LoadedMesh();
    void Load(const std::string& meshname);
    void UpdateVertexBufferFromData();
};

LoadedMesh* GetLoadedMesh(const std::string& meshname);
void ShutdownPlanitiaMeshes();

#endif