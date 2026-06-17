#include "PlanitiaPrimitives.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaDisplay.h"

#include <cstring>
#include <fstream>

DWORD FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2;

PlanitiaVertex::PlanitiaVertex()
    : _x(0), _y(0), _z(0), _color(D3DCOLOR_ARGB(255, 255, 255, 255)), _u(0), _v(0), _u2(0), _v2(0)
{
}

PlanitiaVertex::PlanitiaVertex(float x, float y, float z, DWORD color, float u, float v, float u2, float v2)
    : _x(x), _y(y), _z(z), _color(color), _u(u), _v(v), _u2(u2), _v2(v2)
{
}

Bitmap::~Bitmap()
{
    if (m_Owned && m_Bitmap)
    {
        UnloadTexture(*m_Bitmap);
        delete m_Bitmap;
        m_Bitmap = nullptr;
    }
}

void Bitmap::Load(const std::string& fileName)
{
    std::string path = NormalizePath(fileName);
    m_Bitmap = new Texture2D(LoadTexture(path.c_str()));
    if (m_Bitmap->id == 0)
    {
        throw std::string("Could not open bitmap " + path);
    }
    SetTextureFilter(*m_Bitmap, TEXTURE_FILTER_POINT);
    SetTextureWrap(*m_Bitmap, TEXTURE_WRAP_REPEAT);
    m_Width = static_cast<float>(m_Bitmap->width);
    m_Height = static_cast<float>(m_Bitmap->height);
}

PlanitiaMesh::~PlanitiaMesh()
{
    delete m_VertexBuffer;
    m_VertexBuffer = nullptr;
}

void PlanitiaMesh::Load(const std::string& meshname)
{
    m_VertexList.clear();
    std::ifstream instream(NormalizePath(meshname));
    if (instream.fail())
    {
        throw std::string("Could not load mesh " + meshname);
    }

    PlanitiaVertex temp;
    int r, g, b, a;
    while (instream >> temp._x >> temp._y >> temp._z >> r >> g >> b >> a >> temp._u >> temp._v >> temp._u2 >> temp._v2)
    {
        temp._color = D3DCOLOR_ARGB(r, g, b, a);
        m_VertexList.push_back(temp);
    }

    if (m_VertexList.empty())
    {
        throw std::string("Vertex count of zero in mesh load");
    }

    m_NumberOfVertices = static_cast<int>(m_VertexList.size());
    gp_Display->m_D3DDevice.CreateVertexBuffer(
        sizeof(PlanitiaVertex) * m_NumberOfVertices, 0, FVF, D3DPOOL_MANAGED, &m_VertexBuffer, nullptr);
    UpdateVertexBufferFromData();
}

void PlanitiaMesh::UpdateVertexBufferFromData()
{
    if (!m_VertexBuffer) return;
    void* data = nullptr;
    m_VertexBuffer->Lock(0, static_cast<UINT>(m_VertexList.size() * sizeof(PlanitiaVertex)), &data, 0);
    std::memcpy(data, m_VertexList.data(), m_VertexList.size() * sizeof(PlanitiaVertex));
    m_VertexBuffer->Unlock();
}