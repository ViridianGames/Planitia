#ifndef PLANITIA_TYPES_H
#define PLANITIA_TYPES_H

#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include "raylib.h"

using DWORD = uint32_t;
using UINT = unsigned int;
using INT = int;

using WORD = uint16_t;
using BOOL = int;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

struct D3DXVECTOR3
{
    float x, y, z;
    D3DXVECTOR3() : x(0), y(0), z(0) {}
    D3DXVECTOR3(float ix, float iy, float iz) : x(ix), y(iy), z(iz) {}
    D3DXVECTOR3 operator+(const D3DXVECTOR3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    D3DXVECTOR3 operator-(const D3DXVECTOR3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    D3DXVECTOR3 operator*(float s) const { return {x * s, y * s, z * s}; }
    D3DXVECTOR3& operator+=(const D3DXVECTOR3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    bool operator==(const D3DXVECTOR3& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const D3DXVECTOR3& o) const { return !(*this == o); }
};

struct D3DXVECTOR2
{
    float x, y;
};

using FLOAT = float;

struct D3DXMATRIX
{
    float _11, _12, _13, _14;
    float _21, _22, _23, _24;
    float _31, _32, _33, _34;
    float _41, _42, _43, _44;

    float operator()(int row, int col) const
    {
        const float* f = reinterpret_cast<const float*>(this);
        return f[row * 4 + col];
    }
};

struct D3DVIEWPORT9
{
    DWORD X = 0;
    DWORD Y = 0;
    DWORD Width = 0;
    DWORD Height = 0;
    float MinZ = 0;
    float MaxZ = 1;
};

inline DWORD F2DW(FLOAT f)
{
    DWORD d;
    std::memcpy(&d, &f, sizeof(DWORD));
    return d;
}

inline void D3DXMatrixIdentity(D3DXMATRIX* out)
{
    std::memset(out, 0, sizeof(D3DXMATRIX));
    out->_11 = out->_22 = out->_33 = out->_44 = 1.0f;
}

inline void D3DXMatrixRotationAxis(D3DXMATRIX* out, const D3DXVECTOR3* axis, float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    float t = 1.0f - c;
    float x = axis->x, y = axis->y, z = axis->z;
    D3DXMatrixIdentity(out);
    out->_11 = t * x * x + c;
    out->_22 = t * y * y + c;
    out->_33 = t * z * z + c;
    out->_12 = t * x * y + s * z;
    out->_21 = t * x * y - s * z;
    out->_13 = t * x * z - s * y;
    out->_31 = t * x * z + s * y;
    out->_23 = t * y * z + s * x;
    out->_32 = t * y * z - s * x;
}

inline void D3DXVec3TransformCoord(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DXMATRIX* mat)
{
    float x = v->x * mat->_11 + v->y * mat->_21 + v->z * mat->_31 + mat->_41;
    float y = v->x * mat->_12 + v->y * mat->_22 + v->z * mat->_32 + mat->_42;
    float z = v->x * mat->_13 + v->y * mat->_23 + v->z * mat->_33 + mat->_43;
    *out = {x, y, z};
}

inline void D3DXMatrixLookAtLH(D3DXMATRIX* out, const D3DXVECTOR3* eye, const D3DXVECTOR3* at, const D3DXVECTOR3* up)
{
    D3DXVECTOR3 zaxis = {eye->x - at->x, eye->y - at->y, eye->z - at->z};
    float len = std::sqrt(zaxis.x * zaxis.x + zaxis.y * zaxis.y + zaxis.z * zaxis.z);
    if (len > 0) { zaxis.x /= len; zaxis.y /= len; zaxis.z /= len; }
    D3DXVECTOR3 xaxis = {
        up->y * zaxis.z - up->z * zaxis.y,
        up->z * zaxis.x - up->x * zaxis.z,
        up->x * zaxis.y - up->y * zaxis.x
    };
    len = std::sqrt(xaxis.x * xaxis.x + xaxis.y * xaxis.y + xaxis.z * xaxis.z);
    if (len > 0) { xaxis.x /= len; xaxis.y /= len; xaxis.z /= len; }
    D3DXVECTOR3 yaxis = {
        zaxis.y * xaxis.z - zaxis.z * xaxis.y,
        zaxis.z * xaxis.x - zaxis.x * xaxis.z,
        zaxis.x * xaxis.y - zaxis.y * xaxis.x
    };
    D3DXMatrixIdentity(out);
    out->_11 = xaxis.x; out->_12 = yaxis.x; out->_13 = zaxis.x;
    out->_21 = xaxis.y; out->_22 = yaxis.y; out->_23 = zaxis.y;
    out->_31 = xaxis.z; out->_32 = yaxis.z; out->_33 = zaxis.z;
    out->_41 = -(xaxis.x * eye->x + xaxis.y * eye->y + xaxis.z * eye->z);
    out->_42 = -(yaxis.x * eye->x + yaxis.y * eye->y + yaxis.z * eye->z);
    out->_43 = -(zaxis.x * eye->x + zaxis.y * eye->y + zaxis.z * eye->z);
}

inline void D3DXMatrixPerspectiveFovLH(D3DXMATRIX* out, float fovy, float aspect, float zn, float zf)
{
    D3DXMatrixIdentity(out);
    float yScale = 1.0f / std::tan(fovy * 0.5f);
    float xScale = yScale / aspect;
    out->_11 = xScale;
    out->_22 = yScale;
    out->_33 = zf / (zf - zn);
    out->_34 = 1.0f;
    out->_43 = -zn * zf / (zf - zn);
    out->_44 = 0.0f;
}

inline void D3DXMatrixOrthoLH(D3DXMATRIX* out, float w, float h, float zn, float zf)
{
    D3DXMatrixIdentity(out);
    out->_11 = 2.0f / w;
    out->_22 = 2.0f / h;
    out->_33 = 1.0f / (zf - zn);
    out->_43 = -zn / (zf - zn);
}

inline void D3DXMatrixRotationY(D3DXMATRIX* out, float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    D3DXMatrixIdentity(out);
    out->_11 = c;
    out->_13 = -s;
    out->_31 = s;
    out->_33 = c;
}

inline void D3DXMatrixInverse(D3DXMATRIX* out, float*, const D3DXMATRIX* in)
{
    // Simplified 4x4 inverse for rigid transforms used by terrain picking
    *out = *in;
    float det = in->_11 * (in->_22 * in->_33 - in->_23 * in->_32)
              - in->_12 * (in->_21 * in->_33 - in->_23 * in->_31)
              + in->_13 * (in->_21 * in->_32 - in->_22 * in->_31);
    if (std::abs(det) < 1e-8f)
    {
        D3DXMatrixIdentity(out);
        return;
    }
    float invDet = 1.0f / det;
    D3DXMATRIX r{};
    r._11 =  (in->_22 * in->_33 - in->_23 * in->_32) * invDet;
    r._12 = -(in->_12 * in->_33 - in->_13 * in->_32) * invDet;
    r._13 =  (in->_12 * in->_23 - in->_13 * in->_22) * invDet;
    r._21 = -(in->_21 * in->_33 - in->_23 * in->_31) * invDet;
    r._22 =  (in->_11 * in->_33 - in->_13 * in->_31) * invDet;
    r._23 = -(in->_11 * in->_23 - in->_13 * in->_21) * invDet;
    r._31 =  (in->_21 * in->_32 - in->_22 * in->_31) * invDet;
    r._32 = -(in->_11 * in->_32 - in->_12 * in->_31) * invDet;
    r._33 =  (in->_11 * in->_22 - in->_12 * in->_21) * invDet;
    r._41 = -(in->_41 * r._11 + in->_42 * r._21 + in->_43 * r._31);
    r._42 = -(in->_41 * r._12 + in->_42 * r._22 + in->_43 * r._32);
    r._43 = -(in->_41 * r._13 + in->_42 * r._23 + in->_43 * r._33);
    r._44 = 1.0f;
    *out = r;
}

inline void D3DXVec3Normalize(D3DXVECTOR3* out, const D3DXVECTOR3* v)
{
    float len = std::sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len > 0)
        *out = {v->x / len, v->y / len, v->z / len};
    else
        *out = {0, 0, 0};
}

inline void D3DXVec3TransformNormal(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DXMATRIX* mat)
{
    float x = v->x * mat->_11 + v->y * mat->_21 + v->z * mat->_31;
    float y = v->x * mat->_12 + v->y * mat->_22 + v->z * mat->_32;
    float z = v->x * mat->_13 + v->y * mat->_23 + v->z * mat->_33;
    D3DXVECTOR3 transformed{x, y, z};
    D3DXVec3Normalize(out, &transformed);
}

inline void D3DXVec3Cross(D3DXVECTOR3* out, const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{
    *out = {
        a->y * b->z - a->z * b->y,
        a->z * b->x - a->x * b->z,
        a->x * b->y - a->y * b->x
    };
}

inline float D3DXVec3Dot(const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

inline void D3DXVec3Project(D3DXVECTOR3* out, const D3DXVECTOR3* obj, const D3DVIEWPORT9* vp,
    const D3DXMATRIX* proj, const D3DXMATRIX* view, const D3DXMATRIX* world)
{
    D3DXVECTOR3 v = *obj;
    D3DXVec3TransformCoord(&v, &v, world);
    D3DXVec3TransformCoord(&v, &v, view);
    D3DXVec3TransformCoord(&v, &v, proj);
    if (vp->Width > 0 && vp->Height > 0)
    {
        out->x = vp->X + (1.0f + v.x) * 0.5f * static_cast<float>(vp->Width);
        out->y = vp->Y + (1.0f - v.y) * 0.5f * static_cast<float>(vp->Height);
        out->z = v.z;
    }
    else
    {
        *out = v;
    }
}

inline DWORD D3DCOLOR_ARGB(int a, int r, int g, int b)
{
    return (static_cast<DWORD>(a) << 24) | (static_cast<DWORD>(r) << 16) |
           (static_cast<DWORD>(g) << 8) | static_cast<DWORD>(b);
}

inline DWORD D3DCOLOR_XRGB(int r, int g, int b)
{
    return D3DCOLOR_ARGB(255, r, g, b);
}

inline Color D3DColorToRaylib(DWORD c)
{
    return Color{
        static_cast<unsigned char>((c >> 16) & 0xFF),
        static_cast<unsigned char>((c >> 8) & 0xFF),
        static_cast<unsigned char>(c & 0xFF),
        static_cast<unsigned char>((c >> 24) & 0xFF)
    };
}

inline Color ModulateColors(Color tex, Color vert)
{
    return Color{
        static_cast<unsigned char>((tex.r * vert.r) / 255),
        static_cast<unsigned char>((tex.g * vert.g) / 255),
        static_cast<unsigned char>((tex.b * vert.b) / 255),
        static_cast<unsigned char>((tex.a * vert.a) / 255)
    };
}

#include "Geist/Primitives.h"

inline Vertex MakeTerrainVertex(float x, float y, float z, DWORD color, float u, float v, float u2 = 0, float v2 = 0)
{
    (void)u2;
    (void)v2;
    return CreateVertex(x, y, z, D3DColorToRaylib(color), u, v);
}

inline std::string NormalizePath(std::string path)
{
    for (char& ch : path)
    {
        if (ch == '\\') ch = '/';
    }
    return path;
}

#define D3DPT_TRIANGLELIST 4
#define D3DPT_TRIANGLESTRIP 5
#define D3DPT_LINESTRIP 3
#define D3DTS_VIEW 2
#define D3DTS_PROJECTION 3
#define D3DTS_WORLD 0
#define D3DTS_TEXTURE0 16
#define D3DFVF_XYZ 0x002
#define D3DFVF_DIFFUSE 0x040
#define D3DFVF_TEX2 0x300
#define D3DRS_ALPHABLENDENABLE 27
#define D3DRS_ALPHATESTENABLE 15
#define D3DRS_ALPHAFUNC 25
#define D3DRS_ALPHAREF 24
#define D3DRS_TEXTUREFACTOR 60
#define D3DRS_CULLMODE 22
#define D3DRS_LIGHTING 137
#define D3DRS_ZENABLE 7
#define D3DRS_SRCBLEND 19
#define D3DRS_DESTBLEND 20
#define D3DCULL_NONE 1
#define D3DBLEND_SRCALPHA 5
#define D3DBLEND_INVSRCALPHA 6
#define D3DCMP_GREATEREQUAL 7
#define D3DTSS_ALPHAARG1 6
#define D3DTSS_ALPHAARG2 7
#define D3DTSS_ALPHAOP 4
#define D3DTSS_COLORARG1 2
#define D3DTSS_COLORARG2 3
#define D3DTSS_COLOROP 1
#define D3DTSS_TEXTURETRANSFORMFLAGS 24
#define D3DTA_TEXTURE 2
#define D3DTA_DIFFUSE 0
#define D3DTA_TFACTOR 3
#define D3DTA_CURRENT 1
#define D3DTOP_SELECTARG1 2
#define D3DTOP_DISABLE 1
#define D3DTOP_MODULATE 4
#define D3DTOP_MODULATE2X 5
#define D3DTTFF_COUNT2 2
#define D3DTTFF_DISABLE 0
#define D3DRS_SLOPESCALEDEPTHBIAS 145
#define D3DRS_DEPTHBIAS 146
#define D3DSAMP_MAGFILTER 1
#define D3DSAMP_MINFILTER 2
#define D3DSAMP_MIPFILTER 3
#define D3DTEXF_LINEAR 2
#define D3DCLEAR_TARGET 1
#define D3DCLEAR_ZBUFFER 2
#define D3DUSAGE_DYNAMIC 0x200
#define D3DUSAGE_WRITEONLY 0x8
#define D3DPOOL_DEFAULT 0
#define D3DPOOL_MANAGED 1
#define D3DFMT_INDEX16 101
#define D3DFMT_A8R8G8B8 21
#define D3DFMT_R5G6B5 23
#define D3DLOCK_DISCARD 0x2000

#endif