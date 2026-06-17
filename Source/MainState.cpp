#include "MainState.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaScene.h"
#include "PlanitiaEngineAdapter.h"
#include "Geist/RNG.h"
#include "Geist/Engine.h"
#include "Geist/Globals.h"

MainState::~MainState()
{
    Shutdown();
}

void MainState::Init(const std::string&)
{
    LoadLevelGeometry("Data/Maps/MainMenuTerrain.txt");
}

void MainState::Shutdown() {}

void MainState::Update()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_Engine->m_Done = true;
        if (gp_Engine) gp_Engine->m_Done = true;
    }
}

void MainState::Draw() {}

void MainState::OnEnter() {}
void MainState::OnExit() {}

void MainState::LoadLevelGeometry(const std::string& levelFileName)
{
    if (g_Terrain)
    {
        delete g_Terrain;
        g_Terrain = nullptr;
    }

    g_Background = new Background();
    g_Background->Init("");

    g_Terrain = new Terrain();
    g_Terrain->Init(levelFileName);

    gp_Scene->m_Camera.m_LookAtPointMax = {
        static_cast<float>(g_Terrain->m_VertexWidth), 0.0f,
        static_cast<float>(g_Terrain->m_VertexHeight)};
}