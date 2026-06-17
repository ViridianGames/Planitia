#include "PlanitiaInput.h"
#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "PlanitiaScene.h"
#include "PlanitiaGlobals.h"

void PlanitiaInput::Init(const std::string&) {}
void PlanitiaInput::Shutdown() {}
void PlanitiaInput::Draw() {}

void PlanitiaInput::Update()
{
    m_WasLeftButtonDown = m_IsLeftButtonDown;
    m_WasRightButtonDown = m_IsRightButtonDown;
    m_IsLeftButtonDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    m_IsRightButtonDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

    const float windowToRenderX = g_Engine->m_RenderWidth / static_cast<float>(g_Engine->m_ScreenWidth);
    const float windowToRenderY = g_Engine->m_RenderHeight / static_cast<float>(g_Engine->m_ScreenHeight);
    const float renderX = GetMouseX() * windowToRenderX;
    const float renderY = GetMouseY() * windowToRenderY;

    // GUI layout data uses design resolution coordinates.
    m_MouseX = renderX / gp_Scene->UIScaleX();
    m_MouseY = renderY / gp_Scene->UIScaleY();
}