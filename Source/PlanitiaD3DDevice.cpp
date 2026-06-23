#include "PlanitiaD3DDevice.h"
#include "PlanitiaScene.h"
#include "PlanitiaGlobals.h"
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"

#include "rlgl.h"
#include <cstring>

bool PlanitiaVertexBuffer::Lock(UINT, UINT, void** data, DWORD)
{
    *data = vertices.data();
    return true;
}

void PlanitiaVertexBuffer::Unlock() {}

bool PlanitiaIndexBuffer::Lock(UINT, UINT, void** data, DWORD)
{
    *data = indices.data();
    return true;
}

void PlanitiaIndexBuffer::Unlock() {}

bool PlanitiaD3DDevice::CreateVertexBuffer(UINT length, DWORD, DWORD, DWORD, PlanitiaVertexBuffer** out, void*)
{
    *out = new PlanitiaVertexBuffer();
    (*out)->vertices.resize(length / sizeof(Vertex));
    return true;
}

bool PlanitiaD3DDevice::CreateIndexBuffer(UINT length, DWORD, DWORD, DWORD, PlanitiaIndexBuffer** out, void*)
{
    *out = new PlanitiaIndexBuffer();
    (*out)->indices.resize(length / sizeof(WORD));
    return true;
}

bool PlanitiaD3DDevice::CreateTexture(UINT width, UINT height, UINT, DWORD, DWORD, DWORD, Texture2D** out, void*)
{
    Image img = GenImageColor(static_cast<int>(width), static_cast<int>(height), BLANK);
    *out = new Texture2D(LoadTextureFromImage(img));
    UnloadImage(img);
    return true;
}

void PlanitiaD3DDevice::SetStreamSource(UINT, PlanitiaVertexBuffer* vb, UINT, UINT)
{
    m_CurrentVB = vb;
}

void PlanitiaD3DDevice::SetIndices(PlanitiaIndexBuffer* ib)
{
    m_CurrentIB = ib;
}

void PlanitiaD3DDevice::SetTexture(UINT stage, Texture2D* tex)
{
    if (stage < 2) m_Textures[stage] = tex;
}

void PlanitiaD3DDevice::SetFVF(DWORD) {}
void PlanitiaD3DDevice::SetSamplerState(DWORD, DWORD, DWORD) {}

void PlanitiaD3DDevice::SetRenderState(DWORD state, DWORD value)
{
    switch (state)
    {
    case D3DRS_ALPHABLENDENABLE: m_AlphaBlend = value != 0; break;
    case D3DRS_ALPHATESTENABLE: m_AlphaTest = value != 0; break;
    case D3DRS_ALPHAREF: m_AlphaRef = value; break;
    case D3DRS_TEXTUREFACTOR: m_TextureFactor = value; break;
    default: break;
    }
}

void PlanitiaD3DDevice::SetTextureStageState(DWORD, DWORD type, DWORD value)
{
    switch (type)
    {
    case D3DTSS_COLOROP: m_ColorOp = static_cast<int>(value); break;
    case D3DTSS_COLORARG1: m_ColorArg1 = static_cast<int>(value); break;
    case D3DTSS_COLORARG2: m_ColorArg2 = static_cast<int>(value); break;
    case D3DTSS_ALPHAOP: m_AlphaOp = static_cast<int>(value); break;
    case D3DTSS_ALPHAARG1: m_AlphaArg1 = static_cast<int>(value); break;
    case D3DTSS_TEXTURETRANSFORMFLAGS: m_TextureTransformFlags = static_cast<int>(value); break;
    default: break;
    }
}

void PlanitiaD3DDevice::SetTransform(DWORD state, const D3DXMATRIX* matrix)
{
    if (!matrix) return;
    switch (state)
    {
    case D3DTS_WORLD: m_World = *matrix; break;
    case D3DTS_VIEW: m_View = *matrix; break;
    case D3DTS_PROJECTION: m_Projection = *matrix; break;
    case D3DTS_TEXTURE0: m_Texture0 = *matrix; break;
    default: break;
    }
}

void PlanitiaD3DDevice::GetTransform(DWORD state, D3DXMATRIX* matrix)
{
    if (!matrix) return;
    switch (state)
    {
    case D3DTS_WORLD: *matrix = m_World; break;
    case D3DTS_VIEW: *matrix = m_View; break;
    case D3DTS_PROJECTION: *matrix = m_Projection; break;
    default: D3DXMatrixIdentity(matrix); break;
    }
}

void PlanitiaD3DDevice::Clear(DWORD, void*, DWORD, DWORD, float, DWORD) {}

bool PlanitiaD3DDevice::BeginScene()
{
    return true;
}

void PlanitiaD3DDevice::EndScene() {}

static Color VertexToColor(const Vertex& v)
{
    return Color{
        static_cast<unsigned char>(v.r),
        static_cast<unsigned char>(v.g),
        static_cast<unsigned char>(v.b),
        static_cast<unsigned char>(v.a)
    };
}

static void ApplyTexCoords(const PlanitiaD3DDevice* dev, const Vertex& v, float& u, float& vOut)
{
    u = v.u;
    vOut = v.v;
    if (dev->m_TextureTransformFlags != D3DTTFF_DISABLE)
    {
        const float tu = u * dev->m_Texture0._11 + vOut * dev->m_Texture0._21 + dev->m_Texture0._31;
        const float tv = u * dev->m_Texture0._12 + vOut * dev->m_Texture0._22 + dev->m_Texture0._32;
        u = tu;
        vOut = tv;
    }
}

static Color ResolveVertexColor(PlanitiaD3DDevice* dev, const Vertex& v)
{
    Color vert = VertexToColor(v);
    if (dev->m_ColorOp == D3DTOP_SELECTARG1 && dev->m_ColorArg1 == D3DTA_TEXTURE)
        return WHITE;
    if (dev->m_ColorArg2 == D3DTA_TFACTOR)
    {
        Color factor = D3DColorToRaylib(dev->m_TextureFactor);
        return ModulateColors(vert, factor);
    }
    if (dev->m_ColorOp == D3DTOP_MODULATE && dev->m_ColorArg1 == D3DTA_TEXTURE)
        return vert;
    return vert;
}

void PlanitiaD3DDevice::DrawPrimitive(DWORD type, UINT start, UINT count)
{
    if (!m_CurrentVB) return;
    rlBegin(type == D3DPT_TRIANGLESTRIP ? RL_TRIANGLES : RL_TRIANGLES);
    if (m_Textures[0]) rlSetTexture(m_Textures[0]->id);

    for (UINT i = 0; i < count + 2 && start + i < m_CurrentVB->vertices.size(); ++i)
    {
        const Vertex& vtx = m_CurrentVB->vertices[start + i];
        Color c = ResolveVertexColor(this, vtx);
        float tu, tv;
        ApplyTexCoords(this, vtx, tu, tv);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlTexCoord2f(tu, tv);
        rlVertex3f(vtx.x, vtx.y, vtx.z);
    }
    rlEnd();
    rlSetTexture(0);
}

void PlanitiaD3DDevice::DrawIndexedPrimitive(DWORD, INT, UINT, UINT, UINT startIndex, UINT primCount)
{
    if (!m_CurrentVB || !m_CurrentIB) return;
    if (m_Textures[0]) rlSetTexture(m_Textures[0]->id);
    rlBegin(RL_TRIANGLES);

    for (UINT t = 0; t < primCount; ++t)
    {
        for (int corner = 0; corner < 3; ++corner)
        {
            UINT idx = startIndex + t * 3 + corner;
            if (idx >= m_CurrentIB->indices.size()) continue;
            WORD vi = m_CurrentIB->indices[idx];
            if (vi >= m_CurrentVB->vertices.size()) continue;
            const Vertex& vtx = m_CurrentVB->vertices[vi];
            Color c = ResolveVertexColor(this, vtx);
            if (m_AlphaTest && c.a < m_AlphaRef) c.a = 0;
            float tu, tv;
            ApplyTexCoords(this, vtx, tu, tv);
            rlColor4ub(c.r, c.g, c.b, c.a);
            rlTexCoord2f(tu, tv);
            rlVertex3f(vtx.x, vtx.y, vtx.z);
        }
    }
    rlEnd();
    rlSetTexture(0);
}

void PlanitiaD3DDevice::GetViewport(D3DVIEWPORT9* vp)
{
    if (!vp) return;
    if (gp_Scene)
    {
        vp->Width = static_cast<DWORD>(gp_Scene->m_HRes);
        vp->Height = static_cast<DWORD>(gp_Scene->m_VRes);
    }
}