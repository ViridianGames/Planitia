#ifndef PLANITIA_PRIMITIVES_H
#define PLANITIA_PRIMITIVES_H

#include "PlanitiaTypes.h"
#include "PlanitiaObject.h"
#include "PlanitiaD3DDevice.h"
#include <string>
#include <vector>

extern DWORD FVF;

class PlanitiaVertex
{
public:
    PlanitiaVertex();
    PlanitiaVertex(float x, float y, float z, DWORD color, float u, float v, float u2 = 0, float v2 = 0);

    float _x, _y, _z;
    DWORD _color;
    float _u, _v;
    float _u2, _v2;
};

class Bitmap
{
public:
    Texture2D* m_Bitmap = nullptr;
    float m_Width = 0;
    float m_Height = 0;
    bool m_Owned = true;

    Bitmap() = default;
    ~Bitmap();
    void Load(const std::string& fileName);
};

class PlanitiaMesh
{
public:
    PlanitiaVertexBuffer* m_VertexBuffer = nullptr;
    int m_NumberOfVertices = 0;
    std::vector<PlanitiaVertex> m_VertexList;

    PlanitiaMesh() = default;
    ~PlanitiaMesh();
    void Load(const std::string& meshname);
    void UpdateVertexBufferFromData();
};

#endif