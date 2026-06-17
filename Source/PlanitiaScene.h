#ifndef PLANITIA_SCENE_H
#define PLANITIA_SCENE_H

#include "PlanitiaD3DDevice.h"
#include "PlanitiaTypes.h"
#include "raylib.h"
#include <string>
#include <vector>

class PlanitiaCamera
{
public:
    PlanitiaCamera();
    void Update();
    void IncreaseCameraRotationSpeed();
    void DecreaseCameraRotationSpeed();
    void IncreaseCameraXMovementSpeed();
    void DecreaseCameraXMovementSpeed();
    void IncreaseCameraZMovementSpeed();
    void DecreaseCameraZMovementSpeed();

    D3DXVECTOR3 m_LookAtPoint;
    D3DXVECTOR3 m_Position;
    D3DXVECTOR3 m_MovementCurrentSpeed;
    float m_MovementMaxSpeed = 15;
    float m_MovementAcceleration = 10;
    bool m_DidPositionChangeThisFrame = false;

    float m_Angle = 0;
    float m_AngleCurrentSpeed = 0;
    float m_AngleAcceleration = 10;
    float m_AngleMaxSpeed = 4;
    bool m_DidAngleChangeThisFrame = false;

    float m_Zoom = 13;
    float m_DesiredZoom = 13;
    float m_ZoomMaxSpeed = 0.2f;
    bool m_DidCameraChangeThisFrame = false;

    D3DXVECTOR3 m_LookAtPointMin;
    D3DXVECTOR3 m_LookAtPointMax;
};

struct DisplaySprite
{
    const Texture* image;
    float sourceX, sourceY, sourceWidth, sourceHeight;
    int x, y;
    int r, g, b, a;
};

class PlanitiaScene
{
public:
    PlanitiaScene() = default;
    ~PlanitiaScene();

    void Init(const std::string& configfile);
    void Shutdown();
    void Update();
    void FlushSprites();

    void BlitImage(const Texture* image, int x, int y, int r = 255, int g = 255, int b = 255, int a = 255);
    void BlitImageRect(const Texture* image, float sourceX, float sourceY, float sourceWidth, float sourceHeight,
        int destX, int destY, int r = 255, int g = 255, int b = 255, int a = 255);

    void Begin3D();
    void End3D();

    float UIScaleX() const;
    float UIScaleY() const;

    Ray GetPickRay() const;
    bool PickTriangle(const D3DXVECTOR3& v1, const D3DXVECTOR3& v2, const D3DXVECTOR3& v3, double& distance) const;
    bool PickTriangleUV(const D3DXVECTOR3& v1, const D3DXVECTOR3& v2, const D3DXVECTOR3& v3,
        double& distance, double& u, double& v) const;

    PlanitiaD3DDevice m_D3DDevice;
    PlanitiaCamera m_Camera;
    Camera3D m_Camera3D{};
    D3DXMATRIX m_CurrentCamera{};

    int m_HRes = 480;
    int m_VRes = 270;
    int m_DesignHRes = 1600;
    int m_DesignVRes = 900;
    int m_WindowHRes = 1600;
    int m_WindowVRes = 900;
    bool m_HardwareVertexProcessingSupported = true;
    bool m_FastTerrain = false;
    float m_FieldOfView = 30.0f;

private:
    void SetupProjection();
    void SyncCamera3D();
    void DrawSprites();

    std::vector<DisplaySprite> m_SpriteList;
};

#endif