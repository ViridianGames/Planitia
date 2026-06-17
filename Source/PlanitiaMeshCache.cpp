#include "PlanitiaMeshCache.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaScene.h"
#include "PlanitiaTypes.h"

#include <cstring>
#include <fstream>
#include <map>

static std::map<std::string, LoadedMesh*> s_MeshList;

LoadedMesh::~LoadedMesh()
{
    delete m_VertexBuffer;
    m_VertexBuffer = nullptr;
}

void LoadedMesh::Load(const std::string& meshname)
{
    m_VertexList.clear();
    std::ifstream instream(NormalizePath(meshname));
    if (instream.fail())
        throw std::string("Could not load mesh " + meshname);

    float x, y, z, u, v, u2, v2;
    int r, g, b, a;
    while (instream >> x >> y >> z >> r >> g >> b >> a >> u >> v >> u2 >> v2)
    {
        m_VertexList.push_back(MakeTerrainVertex(x, y, z, D3DCOLOR_ARGB(r, g, b, a), u, v, u2, v2));
    }

    if (m_VertexList.empty())
        throw std::string("Vertex count of zero in mesh load");

    m_NumberOfVertices = static_cast<int>(m_VertexList.size());
    gp_Scene->m_D3DDevice.CreateVertexBuffer(
        static_cast<UINT>(sizeof(Vertex) * m_NumberOfVertices), 0, 0, 0, &m_VertexBuffer, nullptr);
    UpdateVertexBufferFromData();
}

void LoadedMesh::UpdateVertexBufferFromData()
{
    if (!m_VertexBuffer) return;
    void* data = nullptr;
    m_VertexBuffer->Lock(0, static_cast<UINT>(m_VertexList.size() * sizeof(Vertex)), &data, 0);
    std::memcpy(data, m_VertexList.data(), m_VertexList.size() * sizeof(Vertex));
    m_VertexBuffer->Unlock();
}

LoadedMesh* GetLoadedMesh(const std::string& meshname)
{
    std::string key = NormalizePath(meshname);
    auto node = s_MeshList.find(key);
    if (node != s_MeshList.end())
        return node->second;

    auto* temp = new LoadedMesh();
    temp->Load(key);
    s_MeshList[key] = temp;
    return temp;
}

void ShutdownPlanitiaMeshes()
{
    for (auto& pair : s_MeshList)
        delete pair.second;
    s_MeshList.clear();
}