#ifndef PLANITIA_DISPLAY_H
#define PLANITIA_DISPLAY_H

#include "PlanitiaObject.h"
#include "PlanitiaD3DDevice.h"
#include "PlanitiaConfig.h"
#include <map>
#include <string>
#include <vector>

class Unit;
class Bitmap;

extern D3DXMATRIX g_Identity;

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
    const Bitmap* image;
    float sourceX, sourceY, sourceWidth, sourceHeight;
    int x, y;
    int r, g, b, a;
};

class Display : public PlanitiaObject
{
public:
    Display() = default;
    ~Display() override;

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;

    void BlitImage(const Bitmap* image, int x, int y, int r = 255, int g = 255, int b = 255, int a = 255);
    void BlitImageRect(const Bitmap* image, float sourceX, float sourceY, float sourceWidth, float sourceHeight,
        int destX, int destY, int r = 255, int g = 255, int b = 255, int a = 255);
    void DrawBox(int posX, int posY, int width, int height, int r, int g, int b, int a, bool filled);
    void AddUnit(Unit* unit);

    void Begin3D();
    void End3D();
    void FlushSprites();

    float UIScaleX() const;
    float UIScaleY() const;

    bool Pick(D3DXVECTOR3 rayOrigin, D3DXVECTOR3 rayDirection,
        D3DXVECTOR3 tri1, D3DXVECTOR3 tri2, D3DXVECTOR3 tri3,
        D3DXMATRIX triTransform, double& distance);
    bool PickWithUV(D3DXVECTOR3 rayOrigin, D3DXVECTOR3 rayDirection,
        D3DXVECTOR3 tri1, D3DXVECTOR3 tri2, D3DXVECTOR3 tri3,
        D3DXMATRIX triTransform, double& distance, double& u, double& v);

    PlanitiaD3DDevice m_D3DDevice;
    PlanitiaCamera m_Camera;
    D3DXMATRIX m_CurrentCamera;

    int m_HRes = 480;
    int m_VRes = 270;
    int m_DesignHRes = 1600;
    int m_DesignVRes = 900;
    int m_WindowHRes = 1600;
    int m_WindowVRes = 900;
    bool m_HardwareVertexProcessingSupported = true;
    bool m_FastTerrain = false;

private:
    std::vector<Unit*> m_UnitList;
    std::vector<DisplaySprite> m_SpriteList;
    void DrawSprites();
    void SetupProjection();
};

#endif