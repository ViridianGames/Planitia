#include "PlanitiaScene.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaEngineAdapter.h"
#include "PlanitiaInput.h"
#include "Geist/Engine.h"
#include "Geist/Globals.h"

#include "raymath.h"
#include "rlgl.h"
#include <cmath>

static bool s_In3D = false;

static Vector3 ToVector3(const D3DXVECTOR3& v)
{
    return Vector3{ v.x, v.y, v.z };
}

static void ComputeBarycentricUV(Vector3 point, Vector3 a, Vector3 b, Vector3 c, double& u, double& v)
{
    Vector3 v0 = Vector3Subtract(c, a);
    Vector3 v1 = Vector3Subtract(b, a);
    Vector3 v2 = Vector3Subtract(point, a);
    const float d00 = Vector3DotProduct(v0, v0);
    const float d01 = Vector3DotProduct(v0, v1);
    const float d11 = Vector3DotProduct(v1, v1);
    const float d20 = Vector3DotProduct(v2, v0);
    const float d21 = Vector3DotProduct(v2, v1);
    const float denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < 1e-8f)
    {
        u = 0;
        v = 0;
        return;
    }
    const float inv = 1.0f / denom;
    v = (d11 * d20 - d01 * d21) * inv;
    u = (d00 * d21 - d01 * d20) * inv;
}

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
        D3DXMatrixRotationAxis(&T, &up, gp_Scene->m_Camera.m_Angle);
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

    D3DXMatrixLookAtLH(&gp_Scene->m_CurrentCamera, &m_Position, &m_LookAtPoint, &up);
    gp_Scene->m_D3DDevice.SetTransform(D3DTS_VIEW, &gp_Scene->m_CurrentCamera);
}

void PlanitiaCamera::IncreaseCameraRotationSpeed() { m_AngleCurrentSpeed = m_AngleMaxSpeed; m_DidAngleChangeThisFrame = true; }
void PlanitiaCamera::DecreaseCameraRotationSpeed() { m_AngleCurrentSpeed = -m_AngleMaxSpeed; m_DidAngleChangeThisFrame = true; }
void PlanitiaCamera::IncreaseCameraXMovementSpeed() { m_MovementCurrentSpeed = {-m_MovementMaxSpeed, 0, m_MovementMaxSpeed}; m_DidPositionChangeThisFrame = true; }
void PlanitiaCamera::DecreaseCameraXMovementSpeed() { m_MovementCurrentSpeed = {m_MovementMaxSpeed, 0, -m_MovementMaxSpeed}; m_DidPositionChangeThisFrame = true; }
void PlanitiaCamera::IncreaseCameraZMovementSpeed() { m_MovementCurrentSpeed = {m_MovementMaxSpeed, 0, m_MovementMaxSpeed}; m_DidPositionChangeThisFrame = true; }
void PlanitiaCamera::DecreaseCameraZMovementSpeed() { m_MovementCurrentSpeed = {-m_MovementMaxSpeed, 0, -m_MovementMaxSpeed}; m_DidPositionChangeThisFrame = true; }

PlanitiaScene::~PlanitiaScene()
{
    Shutdown();
}

void PlanitiaScene::Init(const std::string&)
{
    m_HRes = static_cast<int>(g_Engine->m_RenderWidth);
    m_VRes = static_cast<int>(g_Engine->m_RenderHeight);

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

    m_FieldOfView = gp_Engine->m_EngineConfig.count("field_of_view")
        ? gp_Engine->m_EngineConfig["field_of_view"].numdata : 30.0f;

    m_Camera3D.up = Vector3{0, 1, 0};
    m_Camera3D.fovy = m_FieldOfView;
    m_Camera3D.projection = CAMERA_PERSPECTIVE;

    SetupProjection();
    m_Camera.Update();
}

void PlanitiaScene::SetupProjection()
{
    D3DXMATRIX proj;
    D3DXMatrixPerspectiveFovLH(&proj, 3.14159265f * (m_FieldOfView / 180.0f),
        static_cast<float>(m_HRes) / static_cast<float>(m_VRes), 1.0f, 1000.0f);
    m_D3DDevice.SetTransform(D3DTS_PROJECTION, &proj);
}

void PlanitiaScene::SyncCamera3D()
{
    m_Camera3D.position = ToVector3(m_Camera.m_Position);
    m_Camera3D.target = ToVector3(m_Camera.m_LookAtPoint);
    m_Camera3D.up = Vector3{0, 1, 0};
    m_Camera3D.fovy = m_FieldOfView;
    m_Camera3D.projection = CAMERA_PERSPECTIVE;
}

void PlanitiaScene::Shutdown() {}

void PlanitiaScene::Update()
{
    m_Camera.Update();
    SyncCamera3D();
}

float PlanitiaScene::UIScaleX() const
{
    return static_cast<float>(m_HRes) / static_cast<float>(m_DesignHRes);
}

float PlanitiaScene::UIScaleY() const
{
    return static_cast<float>(m_VRes) / static_cast<float>(m_DesignVRes);
}

void PlanitiaScene::Begin3D()
{
    if (s_In3D) return;
    s_In3D = true;

    rlDrawRenderBatchActive();
    SyncCamera3D();

    // Planitia's camera was authored for this frustum/translate path. DrawModel()
    // reads the current rlgl matrices, so keep this instead of BeginMode3D().
    // Pull-back must be (-zoom,-zoom,-zoom) in rotated space, not just on Z — otherwise
    // the eye sits at ground level and clips through the heightfield.
    const float aspect = static_cast<float>(m_HRes) / static_cast<float>(m_VRes);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlFrustum(-aspect, aspect, -1.0f, 1.0f, 1.0f, 1000.0f);

    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();

    PlanitiaCamera& cam = m_Camera;
    rlRotatef(cam.m_Angle * 180.0f / 3.14159265f, 0, 1, 0);
    rlTranslatef(-cam.m_LookAtPoint.x, -cam.m_LookAtPoint.y, -cam.m_LookAtPoint.z);
    rlTranslatef(-cam.m_Zoom, -cam.m_Zoom, -cam.m_Zoom);

    rlEnableDepthTest();
    rlEnableDepthMask();
    rlDisableBackfaceCulling();
}

void PlanitiaScene::End3D()
{
    if (!s_In3D) return;
    s_In3D = false;
    rlDrawRenderBatchActive();

    // Restore 2D orthographic projection for UI/text (same as EndMode3D()).
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, m_HRes, m_VRes, 0, 0.0f, 1.0f);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();

    rlDisableDepthTest();
}

void PlanitiaScene::FlushSprites()
{
    rlDrawRenderBatchActive();
    DrawSprites();
}

void PlanitiaScene::BlitImage(const Texture* image, int x, int y, int r, int g, int b, int a)
{
    if (!image) return;
    m_SpriteList.push_back({image, 0, 0, static_cast<float>(image->width), static_cast<float>(image->height), x, y, r, g, b, a});
}

void PlanitiaScene::BlitImageRect(const Texture* image, float sourceX, float sourceY, float sourceWidth, float sourceHeight,
    int destX, int destY, int r, int g, int b, int a)
{
    if (!image) return;
    m_SpriteList.push_back({image, sourceX, sourceY, sourceWidth, sourceHeight, destX, destY, r, g, b, a});
}

void PlanitiaScene::DrawSprites()
{
    const float scaleX = UIScaleX();
    const float scaleY = UIScaleY();

    for (const DisplaySprite& spr : m_SpriteList)
    {
        if (!spr.image || spr.image->id == 0) continue;
        Rectangle src = {spr.sourceX, spr.sourceY, spr.sourceWidth, spr.sourceHeight};
        Rectangle dst = {
            static_cast<float>(spr.x) * scaleX,
            static_cast<float>(spr.y) * scaleY,
            spr.sourceWidth * scaleX,
            spr.sourceHeight * scaleY
        };
        Color tint = {static_cast<unsigned char>(spr.r), static_cast<unsigned char>(spr.g),
            static_cast<unsigned char>(spr.b), static_cast<unsigned char>(spr.a)};
        DrawTexturePro(*spr.image, src, dst, {0, 0}, 0, tint);
    }
    m_SpriteList.clear();
}

Ray PlanitiaScene::GetPickRay() const
{
    if (!gp_Input)
        return Ray{ Vector3{0, 0, 0}, Vector3{0, 0, -1} };

    const Vector2 mouse = {
        gp_Input->m_MouseX * UIScaleX(),
        gp_Input->m_MouseY * UIScaleY()
    };
    return GetMouseRay(mouse, m_Camera3D);
}

bool PlanitiaScene::PickTriangle(const D3DXVECTOR3& v1, const D3DXVECTOR3& v2, const D3DXVECTOR3& v3, double& distance) const
{
    const Ray ray = GetPickRay();
    const RayCollision hit = GetRayCollisionTriangle(ray, ToVector3(v1), ToVector3(v2), ToVector3(v3));
    if (!hit.hit)
        return false;
    distance = hit.distance;
    return true;
}

bool PlanitiaScene::PickTriangleUV(const D3DXVECTOR3& v1, const D3DXVECTOR3& v2, const D3DXVECTOR3& v3,
    double& distance, double& u, double& v) const
{
    const Ray ray = GetPickRay();
    const RayCollision hit = GetRayCollisionTriangle(ray, ToVector3(v1), ToVector3(v2), ToVector3(v3));
    if (!hit.hit)
        return false;
    distance = hit.distance;
    ComputeBarycentricUV(hit.point, ToVector3(v1), ToVector3(v2), ToVector3(v3), u, v);
    return true;
}