#include "PlanitiaFramework.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaEngineAdapter.h"
#include "PlanitiaMeshCache.h"
#include "PlanitiaScene.h"
#include "PlanitiaInput.h"
#include "PlanitiaFonts.h"

static bool s_Active = false;

void PlanitiaFramework::Init(const std::string& configfile)
{
    gp_Engine = new PlanitiaEngineAdapter();
    gp_Engine->Init(configfile);
    gp_Scene = new PlanitiaScene();
    gp_Scene->Init(configfile);
    gp_Input = new PlanitiaInput();
    gp_Input->Init(configfile);
    InitPlanitiaFonts();
    s_Active = true;
}

void PlanitiaFramework::Shutdown()
{
    ShutdownPlanitiaFonts();
    delete gp_Input; gp_Input = nullptr;
    ShutdownPlanitiaMeshes();
    delete gp_Scene; gp_Scene = nullptr;
    delete gp_Engine; gp_Engine = nullptr;
    s_Active = false;
}

void PlanitiaFramework::Update()
{
    if (!s_Active) return;
    gp_Engine->Update();
    gp_Input->Update();
    gp_Scene->Update();
}

void PlanitiaFramework::DrawPost()
{
    if (!s_Active || !gp_Scene) return;
    gp_Scene->FlushSprites();
}

bool PlanitiaFramework::IsActive()
{
    return s_Active;
}