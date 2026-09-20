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
#include "Geist/Logging.h"
#include "Geist/ResourceManager.h"
#include "Geist/StateMachine.h"
#include "raylib.h"
#include <string>
#include <memory>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#define planitia_getpid _getpid
#else
#include <unistd.h>
#define planitia_getpid getpid
#endif

#include "llimits.h"
#include "GameGlobals.h"

#include "rlgl.h"

#define RLGL_IMPLEMENTATION
#define RLGL_SOFT_RENDER

#ifdef _WIN32
// Forward declare Windows types and functions we need
typedef void* HWND;
typedef void* HICON;
typedef void* HMODULE;
typedef void* HINSTANCE;
#ifdef _WIN64
typedef long long LONG_PTR;
#else
typedef long LONG_PTR;
#endif
typedef LONG_PTR LRESULT;
typedef LONG_PTR LPARAM;
typedef unsigned int UINT;

#define MAKEINTRESOURCE(i) ((char*)((unsigned long long)((unsigned short)(i))))
#define WM_SETICON 0x0080
#define ICON_SMALL 0
#define ICON_BIG 1
#define IMAGE_ICON 1
#define LR_DEFAULTSIZE 0x0040
#define LR_SHARED 0x8000

extern "C" {
	__declspec(dllimport) HWND __stdcall GetActiveWindow(void);
	__declspec(dllimport) HICON __stdcall LoadIconA(HINSTANCE hInstance, const char* lpIconName);
	__declspec(dllimport) HMODULE __stdcall GetModuleHandleA(const char* lpModuleName);
	__declspec(dllimport) LRESULT __stdcall SendMessageA(HWND hWnd, UINT Msg, LPARAM wParam, LPARAM lParam);
}

#define LoadIcon LoadIconA
#define GetModuleHandle GetModuleHandleA
#define SendMessage SendMessageA
#endif


using namespace std;
using namespace std::filesystem;

int main(int argv, char** argc)
{
	// Separate logs per process so two local instances don't stomp Redist/runlog.txt.
	const int pid = static_cast<int>(planitia_getpid());
	SetLogFileName("runlog_" + std::to_string(pid) + ".txt");
	Log("Planitia starting (pid " + std::to_string(pid) + ")");

   // Create global engine instance
   g_Engine = std::make_unique<Engine>();

   // Initialize with configuration file
   g_Engine->Init("engine.cfg");
	g_Engine->m_useVirtualResolution = true;

	// Alpha cutout for billboard sprites (same as U7Revisited).
	{
		const std::string shaderPath =
			std::string(GetApplicationDirectory()) + "Data/Shaders/alphaDiscard.fs";
		g_alphaDiscard = LoadShader(NULL, shaderPath.c_str());
		if (g_alphaDiscard.id == 0)
			g_alphaDiscard = LoadShader(NULL, "Data/Shaders/alphaDiscard.fs");
		g_alphaDiscardCutoffLoc = GetShaderLocation(g_alphaDiscard, "alphaCutoff");
		float defaultCutoff = 0.5f;
		if (g_alphaDiscard.id != 0 && g_alphaDiscardCutoffLoc >= 0)
			SetShaderValue(g_alphaDiscard, g_alphaDiscardCutoffLoc, &defaultCutoff, SHADER_UNIFORM_FLOAT);
		if (g_alphaDiscard.id == 0)
			Log("WARNING: Failed to load Data/Shaders/alphaDiscard.fs");
		else
			Log("Loaded alphaDiscard shader");
	}

	// Create global objects
	g_drawScale = g_Engine->m_ScreenHeight / g_Engine->m_RenderHeight;

	// Pixel fonts - always bake at draw size and draw at baseSize (see LoadPixelFont).
	g_font = make_shared<Font>(LoadPixelFont("Fonts/softsquare.ttf", 9));
	g_smallFont = make_shared<Font>(LoadPixelFont("Fonts/babyblocks.ttf", 8));

	// Custom mouse cursor is loaded by Engine from engine.cfg (mouse_cursor).

   // Create and register our example state
   TitleState* titleState = new TitleState();
   titleState->Init("");
   g_StateMachine->RegisterState(STATE_TITLESTATE, titleState, "TitleState");

	MainState* mainState = new MainState();
	mainState->Init("");
	g_StateMachine->RegisterState(STATE_MAINSTATE, mainState, "MainState");

	OptionsState* optionsState = new OptionsState();
	optionsState->Init("");
	g_StateMachine->RegisterState(STATE_OPTIONSSTATE, optionsState, "OptionsState");

	MultiplayerMenuState* multiState = new MultiplayerMenuState();
	multiState->Init("");
	g_StateMachine->RegisterState(STATE_MULTIPLAYERMENUSTATE, multiState, "MultiplayerMenuState");

   g_StateMachine->MakeStateTransition(STATE_TITLESTATE);

   // Main game loop
   while (!g_Engine->m_Done && !WindowShouldClose())
   {
      g_Engine->Update();
      g_Engine->Draw();
   }

   // Cleanup
	if (g_alphaDiscard.id != 0)
	{
		UnloadShader(g_alphaDiscard);
		g_alphaDiscard = {};
		g_alphaDiscardCutoffLoc = -1;
	}
   g_Engine->Shutdown();

   return 0;
}
