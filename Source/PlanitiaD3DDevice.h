#ifndef PLANITIA_D3D_DEVICE_H
#define PLANITIA_D3D_DEVICE_H

#include "PlanitiaTypes.h"
#include "Geist/Primitives.h"
#include <vector>

class PlanitiaVertexBuffer
{
public:
    std::vector<Vertex> vertices;
    bool Lock(UINT offset, UINT size, void** data, DWORD flags);
    void Unlock();
};

class PlanitiaIndexBuffer
{
public:
    std::vector<WORD> indices;
    bool Lock(UINT offset, UINT size, void** data, DWORD flags);
    void Unlock();
};

class PlanitiaD3DDevice
{
public:
    PlanitiaVertexBuffer* m_CurrentVB = nullptr;
    PlanitiaIndexBuffer* m_CurrentIB = nullptr;
    Texture2D* m_Textures[2] = {nullptr, nullptr};
    DWORD m_TextureFactor = 0xFFFFFFFF;
    bool m_AlphaBlend = false;
    bool m_AlphaTest = false;
    DWORD m_AlphaRef = 8;
    int m_ColorOp = D3DTOP_MODULATE;
    int m_ColorArg1 = D3DTA_TEXTURE;
    int m_ColorArg2 = D3DTA_DIFFUSE;
    int m_AlphaOp = D3DTOP_SELECTARG1;
    int m_AlphaArg1 = D3DTA_TEXTURE;
    int m_TextureTransformFlags = D3DTTFF_DISABLE;
    D3DXMATRIX m_World;
    D3DXMATRIX m_View;
    D3DXMATRIX m_Projection;
    D3DXMATRIX m_Texture0;

    bool CreateVertexBuffer(UINT length, DWORD usage, DWORD fvf, DWORD pool, PlanitiaVertexBuffer** out, void* handle);
    bool CreateIndexBuffer(UINT length, DWORD usage, DWORD format, DWORD pool, PlanitiaIndexBuffer** out, void* handle);
    bool CreateTexture(UINT width, UINT height, UINT levels, DWORD usage, DWORD format, DWORD pool, Texture2D** out, void* handle);

    void SetStreamSource(UINT stream, PlanitiaVertexBuffer* vb, UINT offset, UINT stride);
    void SetIndices(PlanitiaIndexBuffer* ib);
    void SetTexture(UINT stage, Texture2D* tex);
    void SetFVF(DWORD fvf);
    void SetRenderState(DWORD state, DWORD value);
    void SetTextureStageState(DWORD stage, DWORD type, DWORD value);
    void SetTransform(DWORD state, const D3DXMATRIX* matrix);
    void GetTransform(DWORD state, D3DXMATRIX* matrix);
    void SetSamplerState(DWORD sampler, DWORD type, DWORD value);
    void Clear(DWORD count, void* rects, DWORD flags, DWORD color, float z, DWORD stencil);
    bool BeginScene();
    void EndScene();
    void DrawPrimitive(DWORD type, UINT start, UINT count);
    void DrawIndexedPrimitive(DWORD type, INT baseVertex, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount);
    void GetViewport(D3DVIEWPORT9* vp);
};

#endif