///////////////////////////////////////////////////////////////////////////
//
// Name:     MAIN.CPP
// Author:   Anthony Salter
// Date:     2/03/05
// Purpose:  Contains the entry point for the program.
//
///////////////////////////////////////////////////////////////////////////

#include "Geist/Globals.h"
#include "Geist/Engine.h"
#include "Geist/StateMachine.h"
#include "PlanitiaFramework.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaEngineAdapter.h"

#include "LoadingState.h"
#include "MainMenuState.h"
#include "MainState.h"
#include "StubState.h"

#include "raylib.h"
#include <memory>

#include "rlgl.h"

#define RLGL_IMPLEMENTATION
#define RLGL_SOFT_RENDER

int main(int, char**)
{
    g_Engine = std::make_unique<Engine>();
    g_Engine->Init("engine.cfg");
    g_Engine->m_useVirtualResolution = true;

    PlanitiaFramework::Init("Data/engine.cfg");

    auto* mainState = new MainState();
    mainState->Init("Data/engine.cfg");
    g_StateMachine->RegisterState(STATE_MAINSTATE, mainState, "MainState");

    auto* pauseState = CreateStubState();
    pauseState->Init("");
    g_StateMachine->RegisterState(STATE_PAUSESTATE, pauseState, "PauseState");

    auto* mainMenuState = new MainMenuState();
    mainMenuState->Init("Data/engine.cfg");
    g_StateMachine->RegisterState(STATE_MAINMENUSTATE, mainMenuState, "MainMenuState");

    auto* setUpMultiplayerState = CreateStubState();
    setUpMultiplayerState->Init("");
    g_StateMachine->RegisterState(STATE_SETUPMULTIPLAYERSTATE, setUpMultiplayerState, "SetUpMultiplayerState");

    auto* setUpStoryState = CreateStubState();
    setUpStoryState->Init("");
    g_StateMachine->RegisterState(STATE_SETUPSTORYSTATE, setUpStoryState, "SetUpStoryState");

    auto* setUpSkirmishState = CreateStubState();
    setUpSkirmishState->Init("");
    g_StateMachine->RegisterState(STATE_SETUPSKIRMISHSTATE, setUpSkirmishState, "SetUpSkirmishState");

    auto* optionsState = CreateStubState();
    optionsState->Init("");
    g_StateMachine->RegisterState(STATE_OPTIONSSTATE, optionsState, "OptionsState");

    auto* loadingState = new LoadingState();
    loadingState->Init("Data/engine.cfg");
    g_StateMachine->RegisterState(STATE_LOADINGSTATE, loadingState, "LoadingState");

    auto* setUpTutorialState = CreateStubState();
    setUpTutorialState->Init("");
    g_StateMachine->RegisterState(STATE_SETUPTUTORIALSTATE, setUpTutorialState, "SetUpTutorialState");

    g_StateMachine->MakeStateTransition(STATE_LOADINGSTATE);

    while (!g_Engine->m_Done && !WindowShouldClose())
    {
        if (gp_Engine && gp_Engine->m_Done)
            g_Engine->m_Done = true;

        PlanitiaFramework::Update();
        g_Engine->Update();
        g_Engine->Draw();
    }

    PlanitiaFramework::Shutdown();
    g_Engine->Shutdown();

    return 0;
}