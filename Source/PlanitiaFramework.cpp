#include "PlanitiaFramework.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaEngineAdapter.h"
#include "PlanitiaResourceManager.h"
#include "PlanitiaDisplay.h"
#include "PlanitiaInput.h"
#include "PlanitiaFonts.h"

static bool s_Active = false;

void PlanitiaFramework::Init(const std::string& configfile)
{
    gp_Engine = new PlanitiaEngineAdapter();
    gp_Engine->Init(configfile);
    gp_ResourceManager = new PlanitiaResourceManager();
    gp_ResourceManager->Init(configfile);
    gp_Display = new Display();
    gp_Display->Init(configfile);
    gp_Input = new PlanitiaInput();
    gp_Input->Init(configfile);
    InitPlanitiaFonts();
    s_Active = true;
}

void PlanitiaFramework::Shutdown()
{
    ShutdownPlanitiaFonts();
    delete gp_Input; gp_Input = nullptr;
    delete gp_Display; gp_Display = nullptr;
    delete gp_ResourceManager; gp_ResourceManager = nullptr;
    delete gp_Engine; gp_Engine = nullptr;
    s_Active = false;
}

void PlanitiaFramework::Update()
{
    if (!s_Active) return;
    gp_Engine->Update();
    gp_Input->Update();
    gp_Display->Update();
}

void PlanitiaFramework::DrawPost()
{
    if (!s_Active || !gp_Display) return;
    gp_Display->Draw();
}

bool PlanitiaFramework::IsActive()
{
    return s_Active;
}