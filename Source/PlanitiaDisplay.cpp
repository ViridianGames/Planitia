#include "PlanitiaDisplay.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaEngineAdapter.h"
#include "PlanitiaPrimitives.h"
#include "Unit.h"
#include "Geist/Engine.h"
#include "Geist/Globals.h"

#include "rlgl.h"
#include <cmath>

D3DXMATRIX g_Identity;
static bool s_In3D = false;

PlanitiaCamera::PlanitiaCamera()
{
    m_LookAtPoint = {0, 0, 0};
    m_Position = {0, 0, 0};
    m_MovementCurrentSpeed = {0, 0, 0};
    m_LookAtPointMin = {0, 0, 0};
    m_LookAtPointMax = {100, 100, 100};
    m_DidCameraChangeThisFrame = true;
}

void PlanitiaCamera::Update()
{
    m_DidCameraChangeThisFrame = false;

    if (m_DesiredZoom != m_Zoom)
    {
        m_DidCameraChangeThisFrame = true;
        if (std::abs(m_DesiredZoom - m_Zoom) > m_ZoomMaxSpeed)
        {
            if (m_DesiredZoom > m_Zoom) m_Zoom += m_ZoomMaxSpeed;
            else m_Zoom -= m_ZoomMaxSpeed;
        }
        else m_Zoom = m_DesiredZoom;
    }

    if (m_AngleCurrentSpeed != 0)
    {
        m_DidCameraChangeThisFrame = true;
        m_Angle += m_AngleCurrentSpeed * gp_Engine->m_DurationOfLastUpdateInSeconds;
        if (!m_DidAngleChangeThisFrame)
        {
            m_AngleCurrentSpeed *= (0.99f * (gp_Engine->m_DurationOfLastUpdateInSeconds * 10.0f));
            if (std::abs(m_AngleCurrentSpeed) < 0.1f) m_AngleCurrentSpeed = 0;
        }
    }
    m_DidAngleChangeThisFrame = false;

    if (m_MovementCurrentSpeed != D3DXVECTOR3{0, 0, 0})
    {
        m_DidCameraChangeThisFrame = true;
        D3DXVECTOR3 up = {0, 1, 0};
        D3DXMATRIX T;
        D3DXMatrixRotationAxis(&T, &up, gp_Display->m_Camera.m_Angle);
        D3DXVECTOR3 motion;
        D3DXVec3TransformCoord(&motion, &m_MovementCurrentSpeed, &T);
        m_LookAtPoint += motion * gp_Engine->m_DurationOfLastUpdateInSeconds;
        if (!m_DidPositionChangeThisFrame)
        {
            m_MovementCurrentSpeed = m_MovementCurrentSpeed * (0.99f * (gp_Engine->m_DurationOfLastUpdateInSeconds * 10.0f));
            float speed = std::sqrt(m_MovementCurrentSpeed.x * m_MovementCurrentSpeed.x +
                                    m_MovementCurrentSpeed.z * m_MovementCurrentSpeed.z);
            if (speed < 0.1f) m_MovementCurrentSpeed = {0, 0, 0};
        }
    }
    m_DidPositionChangeThisFrame = false;

    if (m_LookAtPoint.x < m_LookAtPointMin.x) m_LookAtPoint.x = m_LookAtPointMin.x;
    if (m_LookAtPoint.z < m_LookAtPointMin.z) m_LookAtPoint.z = m_LookAtPointMin.z;
    if (m_LookAtPoint.x > m_LookAtPointMax.x) m_LookAtPoint.x = m_LookAtPointMax.x;
    if (m_LookAtPoint.z > m_LookAtPointMax.z) m_LookAtPoint.z = m_LookAtPointMax.z;

    m_Position = {m_Zoom, m_Zoom, m_Zoom};
    D3DXVECTOR3 up = {0, 1, 0};
    D3DXMATRIX T;
    D3DXMatrixRotationAxis(&T, &up, m_Angle);
    D3DXVec3TransformCoord(&m_Position, &m_Position, &T);
    m_Position += m_LookAtPoint;

    D3DXMatrixLookAtLH(&gp_Display->m_CurrentCamera, &m_Position, &m_LookAtPoint, &up);
    gp_Display->m_D3DDevice.SetTransform(D3DTS_VIEW, &gp_Display->m_CurrentCamera);
}

void PlanitiaCamera::IncreaseCameraRotationSpeed() { m_AngleCurrentSpeed = m_AngleMaxSpeed; m_DidAngleChangeThisFrame = true; }
void PlanitiaCamera::DecreaseCameraRotationSpeed() { m_AngleCurrentSpeed = -m_AngleMaxSpeed; m_DidAngleChangeThisFrame = true; }
void PlanitiaCamera::IncreaseCameraXMovementSpeed() { m_MovementCurrentSpeed = {-m_MovementMaxSpeed, 0, m_MovementMaxSpeed}; m_DidPositionChangeThisFrame = true; }
void PlanitiaCamera::DecreaseCameraXMovementSpeed() { m_MovementCurrentSpeed = {m_MovementMaxSpeed, 0, -m_MovementMaxSpeed}; m_DidPositionChangeThisFrame = true; }
void PlanitiaCamera::IncreaseCameraZMovementSpeed() { m_MovementCurrentSpeed = {m_MovementMaxSpeed, 0, m_MovementMaxSpeed}; m_DidPositionChangeThisFrame = true; }
void PlanitiaCamera::DecreaseCameraZMovementSpeed() { m_MovementCurrentSpeed = {-m_MovementMaxSpeed, 0, -m_MovementMaxSpeed}; m_DidPositionChangeThisFrame = true; }

Display::~Display() { Shutdown(); }

void Display::Init(const std::string&)
{
    D3DXMatrixIdentity(&g_Identity);

    // Render resolution is the actual game framebuffer (e.g. 480x270).
    m_HRes = static_cast<int>(g_Engine->m_RenderWidth);
    m_VRes = static_cast<int>(g_Engine->m_RenderHeight);

    // Design resolution: legacy data files (GUI layouts, etc.) were authored at this size.
    m_DesignHRes = 1600;
    m_DesignVRes = 900;
    if (gp_Engine->m_EngineConfig.count("h_res"))
        m_DesignHRes = static_cast<int>(gp_Engine->m_EngineConfig["h_res"].numdata);
    if (gp_Engine->m_EngineConfig.count("v_res"))
        m_DesignVRes = static_cast<int>(gp_Engine->m_EngineConfig["v_res"].numdata);

    m_WindowHRes = m_DesignHRes;
    m_WindowVRes = m_DesignVRes;
    if (gp_Engine->m_EngineConfig.count("window_hres"))
        m_WindowHRes = static_cast<int>(gp_Engine->m_EngineConfig["window_hres"].numdata);
    if (gp_Engine->m_EngineConfig.count("window_vres"))
        m_WindowVRes = static_cast<int>(gp_Engine->m_EngineConfig["window_vres"].numdata);

    m_Camera.m_Zoom = gp_Engine->m_EngineConfig.count("camera_close_limit")
        ? gp_Engine->m_EngineConfig["camera_close_limit"].numdata : 13.0f;
    m_Camera.m_DesiredZoom = m_Camera.m_Zoom;
    m_Camera.m_MovementAcceleration = gp_Engine->m_EngineConfig.count("camera_pan_accelerate")
        ? gp_Engine->m_EngineConfig["camera_pan_accelerate"].numdata : 10.0f;
    m_Camera.m_MovementMaxSpeed = gp_Engine->m_EngineConfig.count("camera_pan_topspeed")
        ? gp_Engine->m_EngineConfig["camera_pan_topspeed"].numdata : 15.0f;
    m_Camera.m_AngleAcceleration = gp_Engine->m_EngineConfig.count("camera_rotate_accelerate")
        ? gp_Engine->m_EngineConfig["camera_rotate_accelerate"].numdata : 10.0f;
    m_Camera.m_AngleMaxSpeed = gp_Engine->m_EngineConfig.count("camera_rotate_topspeed")
        ? gp_Engine->m_EngineConfig["camera_rotate_topspeed"].numdata : 4.0f;
    m_Camera.m_ZoomMaxSpeed = gp_Engine->m_EngineConfig.count("camera_zoom_speed")
        ? gp_Engine->m_EngineConfig["camera_zoom_speed"].numdata : 0.2f;

    SetupProjection();
}

void Display::SetupProjection()
{
    float fov = gp_Engine->m_EngineConfig.count("field_of_view")
        ? gp_Engine->m_EngineConfig["field_of_view"].numdata : 30.0f;
    D3DXMATRIX proj;
    D3DXMatrixPerspectiveFovLH(&proj, 3.14159265f * (fov / 180.0f),
        static_cast<float>(m_HRes) / static_cast<float>(m_VRes), 1.0f, 1000.0f);
    m_D3DDevice.SetTransform(D3DTS_PROJECTION, &proj);
}

void Display::Shutdown() {}

void Display::Update()
{
    m_Camera.Update();
}

float Display::UIScaleX() const
{
    return static_cast<float>(m_HRes) / static_cast<float>(m_DesignHRes);
}

float Display::UIScaleY() const
{
    return static_cast<float>(m_VRes) / static_cast<float>(m_DesignVRes);
}

void Display::Begin3D()
{
    if (s_In3D) return;
    s_In3D = true;

    float scaleX = 1.0f;
    float scaleY = 1.0f;

    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    float fov = gp_Engine->m_EngineConfig.count("field_of_view")
        ? gp_Engine->m_EngineConfig["field_of_view"].numdata : 30.0f;
    rlFrustum(-scaleX, scaleX, -scaleY, scaleY, 1.0f, 1000.0f);

    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();

    PlanitiaCamera& cam = m_Camera;
    rlRotatef(cam.m_Angle * 180.0f / 3.14159265f, 0, 1, 0);
    rlTranslatef(-cam.m_LookAtPoint.x, -cam.m_LookAtPoint.y, -cam.m_LookAtPoint.z - cam.m_Zoom);
}

void Display::End3D()
{
    if (!s_In3D) return;
    s_In3D = false;
    rlDrawRenderBatchActive();
}

void Display::Draw()
{
    FlushSprites();
}

void Display::FlushSprites()
{
    rlDrawRenderBatchActive();
    DrawSprites();
}

void Display::DrawBox(int posX, int posY, int width, int height, int r, int g, int b, int a, bool filled)
{
    const float scaleX = UIScaleX();
    const float scaleY = UIScaleY();
    Rectangle rect = {
        static_cast<float>(posX) * scaleX,
        static_cast<float>(posY) * scaleY,
        static_cast<float>(width) * scaleX,
        static_cast<float>(height) * scaleY
    };
    Color color = {static_cast<unsigned char>(r), static_cast<unsigned char>(g),
        static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
    if (filled) DrawRectangleRec(rect, color);
    else DrawRectangleLinesEx(rect, 1, color);
}

void Display::AddUnit(Unit* unit)
{
    m_UnitList.push_back(unit);
}

void Display::BlitImage(const Bitmap* image, int x, int y, int r, int g, int b, int a)
{
    if (!image) return;
    m_SpriteList.push_back({image, 0, 0, image->m_Width, image->m_Height, x, y, r, g, b, a});
}

void Display::BlitImageRect(const Bitmap* image, float sourceX, float sourceY, float sourceWidth, float sourceHeight,
    int destX, int destY, int r, int g, int b, int a)
{
    if (!image) return;
    m_SpriteList.push_back({image, sourceX, sourceY, sourceWidth, sourceHeight, destX, destY, r, g, b, a});
}

void Display::DrawSprites()
{
    const float scaleX = UIScaleX();
    const float scaleY = UIScaleY();

    for (const DisplaySprite& spr : m_SpriteList)
    {
        if (!spr.image || !spr.image->m_Bitmap) continue;
        Rectangle src = {spr.sourceX, spr.sourceY, spr.sourceWidth, spr.sourceHeight};
        Rectangle dst = {
            static_cast<float>(spr.x) * scaleX,
            static_cast<float>(spr.y) * scaleY,
            spr.sourceWidth * scaleX,
            spr.sourceHeight * scaleY
        };
        Color tint = {static_cast<unsigned char>(spr.r), static_cast<unsigned char>(spr.g),
            static_cast<unsigned char>(spr.b), static_cast<unsigned char>(spr.a)};
        DrawTexturePro(*spr.image->m_Bitmap, src, dst, {0, 0}, 0, tint);
    }
    m_SpriteList.clear();
}

namespace {

#define PICK_EPSILON 0.000001
#define PICK_CROSS(dest, v1, v2) \
    dest[0] = v1[1] * v2[2] - v1[2] * v2[1]; \
    dest[1] = v1[2] * v2[0] - v1[0] * v2[2]; \
    dest[2] = v1[0] * v2[1] - v1[1] * v2[0]
#define PICK_DOT(v1, v2) (v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2])
#define PICK_SUB(dest, v1, v2) \
    dest[0] = v1[0] - v2[0]; \
    dest[1] = v1[1] - v2[1]; \
    dest[2] = v1[2] - v2[2]

int IntersectTriangle(double orig[3], double dir[3],
    double vert0[3], double vert1[3], double vert2[3],
    double* t, double* u, double* v)
{
    double edge1[3], edge2[3], tvec[3], pvec[3], qvec[3];
    double det, invDet;

    PICK_SUB(edge1, vert1, vert0);
    PICK_SUB(edge2, vert2, vert0);
    PICK_CROSS(pvec, dir, edge2);
    det = PICK_DOT(edge1, pvec);

    if (det > PICK_EPSILON)
    {
        PICK_SUB(tvec, orig, vert0);
        *u = PICK_DOT(tvec, pvec);
        if (*u < 0.0 || *u > det) return 0;
        PICK_CROSS(qvec, tvec, edge1);
        *v = PICK_DOT(dir, qvec);
        if (*v < 0.0 || *u + *v > det) return 0;
    }
    else if (det < -PICK_EPSILON)
    {
        PICK_SUB(tvec, orig, vert0);
        *u = PICK_DOT(tvec, pvec);
        if (*u > 0.0 || *u < det) return 0;
        PICK_CROSS(qvec, tvec, edge1);
        *v = PICK_DOT(dir, qvec);
        if (*v > 0.0 || *u + *v < det) return 0;
    }
    else return 0;

    invDet = 1.0 / det;
    *t = PICK_DOT(edge2, qvec) * invDet;
    (*u) *= invDet;
    (*v) *= invDet;
    return 1;
}

} // namespace

bool Display::Pick(D3DXVECTOR3 rayOrigin, D3DXVECTOR3 rayDirection,
    D3DXVECTOR3 tri1, D3DXVECTOR3 tri2, D3DXVECTOR3 tri3,
    D3DXMATRIX, double& distance)
{
    double orig[3] = {rayOrigin.x, rayOrigin.y, rayOrigin.z};
    double dir[3] = {rayDirection.x, rayDirection.y, rayDirection.z};
    double vert0[3] = {tri1.x, tri1.y, tri1.z};
    double vert1[3] = {tri2.x, tri2.y, tri2.z};
    double vert2[3] = {tri3.x, tri3.y, tri3.z};
    double u, v;
    return IntersectTriangle(orig, dir, vert0, vert1, vert2, &distance, &u, &v) != 0;
}

bool Display::PickWithUV(D3DXVECTOR3 rayOrigin, D3DXVECTOR3 rayDirection,
    D3DXVECTOR3 tri1, D3DXVECTOR3 tri2, D3DXVECTOR3 tri3,
    D3DXMATRIX, double& distance, double& u, double& v)
{
    double orig[3] = {rayOrigin.x, rayOrigin.y, rayOrigin.z};
    double dir[3] = {rayDirection.x, rayDirection.y, rayDirection.z};
    double vert0[3] = {tri1.x, tri1.y, tri1.z};
    double vert1[3] = {tri2.x, tri2.y, tri2.z};
    double vert2[3] = {tri3.x, tri3.y, tri3.z};
    return IntersectTriangle(orig, dir, vert0, vert1, vert2, &distance, &u, &v) != 0;
}