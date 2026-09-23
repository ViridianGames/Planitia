#include "MainState.h"

#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/StateMachine.h"
#include "GameGlobals.h"
#include "GameSim.h"
#include "GodPowerCursor.h"
#include "Terrain.h"
#include "WalkerSprites.h"
#include "Geist/Engine.h"

#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <string>

void MainState::Init(const std::string& configfile)
{
	(void)configfile;
	m_Hud.Init();
	GodPowerCursor::Init();
}

void MainState::Shutdown()
{
	m_Hud.Shutdown();
	GodPowerCursor::Shutdown();
	g_Sim.Reset();
	g_Terrain.reset();
	WalkerSprites::Unload();
}

void MainState::FocusCameraOnLocalTown()
{
	if (!g_Terrain)
		return;

	// Fallback: map center
	m_LookAt = {
		g_Terrain->m_CellWidth * 0.5f,
		0.0f,
		g_Terrain->m_CellHeight * 0.5f
	};

	const Player& local = g_Sim.GetPlayer(g_Sim.LocalPlayerSlot());
	if (!local.m_VillageIds.empty())
	{
		if (const Unit* v = g_Sim.GetUnit(local.m_VillageIds.front()))
		{
			m_LookAt.x = v->m_Pos.x;
			m_LookAt.z = v->m_Pos.z;
		}
	}

	const float ground = g_Terrain->GetHeight(m_LookAt.x, m_LookAt.z);
	m_LookAt.y = (ground > 0.0f) ? ground : 0.0f;
}

void MainState::OnEnter()
{
	m_DrawCursor = true;
	WalkerSprites::EnsureLoaded();

	// Multiplayer lobby already started the match - don't wipe the session.
	if (!g_Lockstep.MatchRunning())
	{
		g_Net.Disconnect();
		g_Lockstep.ResetOffline(0);
		const unsigned int seed = 7777;
		const uint16_t turnLen = static_cast<uint16_t>(
			g_Engine ? std::max(1, static_cast<int>(g_Engine->m_EngineConfig.GetNumber("turn_length"))) : 2);
		const uint16_t inputDelay = static_cast<uint16_t>(
			g_Engine ? std::max(0, static_cast<int>(g_Engine->m_EngineConfig.GetNumber("input_delay"))) : 2);
		g_Lockstep.BeginMatch(g_Sim, seed, /*simPlayers=*/2, /*localSlot=*/0, turnLen, /*inputPlayers=*/1, inputDelay);
	}

	FocusCameraOnLocalTown();

	m_Camera.up = { 0.0f, 1.0f, 0.0f };
	m_Camera.fovy = 10.0f;
	m_Camera.projection = CAMERA_PERSPECTIVE;
}

void MainState::OnExit()
{
	g_Net.Disconnect();
	g_Lockstep.ResetOffline(0);
}

void MainState::Update()
{
	if (IsKeyPressed(KEY_ESCAPE))
	{
		g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
		return;
	}

	if (IsKeyPressed(KEY_R) && g_Terrain && g_Net.GetMode() == NetSession::Mode::Offline)
	{
		const unsigned int seed = static_cast<unsigned int>(GetTime() * 1000.0);
		const uint16_t turnLen = static_cast<uint16_t>(
			g_Engine ? std::max(1, static_cast<int>(g_Engine->m_EngineConfig.GetNumber("turn_length"))) : 2);
		const uint16_t inputDelay = static_cast<uint16_t>(
			g_Engine ? std::max(0, static_cast<int>(g_Engine->m_EngineConfig.GetNumber("input_delay"))) : 2);
		g_Lockstep.BeginMatch(g_Sim, seed, 2, 0, turnLen, 1, inputDelay);
		FocusCameraOnLocalTown();
	}

	if (IsKeyPressed(KEY_G) && g_Terrain)
		g_Terrain->CycleDrawMode();

	if (IsKeyPressed(KEY_F3))
		g_showPerfCounter = !g_showPerfCounter;

	m_Hud.Update();

	// Minimap click jumps the camera look-at to that map position.
	{
		float jumpX = 0.0f;
		float jumpZ = 0.0f;
		if (m_Hud.ConsumeMinimapCameraJump(jumpX, jumpZ) && g_Terrain)
		{
			m_LookAt.x = jumpX;
			m_LookAt.z = jumpZ;
			const float ground = g_Terrain->GetHeight(m_LookAt.x, m_LookAt.z);
			m_LookAt.y = (ground > 0.0f) ? ground : 0.0f;
		}
	}

	// Shared terrain pick for casting and the 3D power cursor (must stay in sync).
	m_TerrainHitValid = false;
	if (g_Terrain && !m_Hud.IsMouseOver())
	{
		Vector3 terrainHit{};
		if (g_Terrain->Raycast(GetTerrainMouseRay(m_Camera), terrainHit))
		{
			m_TerrainHitValid = true;
			m_TerrainHit = terrainHit;
		}
	}

	// World click -> lockstep command (not immediate Try*).
	if (m_TerrainHitValid && g_Lockstep.MatchRunning() && !g_Lockstep.IsDesynced())
	{
		const bool powersTab = (m_Hud.GetActiveTab() == MainHud::Tab::Powers);
		const bool unitsTab = (m_Hud.GetActiveTab() == MainHud::Tab::Units);
		const PlayerAction power = m_Hud.GetSelectedPowerAction();
		const bool holdCast = (power == PlayerAction::Flatten);
		const bool click =
			holdCast ? IsMouseButtonDown(MOUSE_BUTTON_LEFT) : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

		if (click)
		{
			const int cx = static_cast<int>(m_TerrainHit.x);
			const int cz = static_cast<int>(m_TerrainHit.z);
			if (powersTab)
				g_Lockstep.SubmitLocalAction(power, cx, cz);
			else if (unitsTab && m_Hud.GetSelectedUnitAction() == PlayerAction::MoveGeneral)
				g_Lockstep.SubmitLocalAction(PlayerAction::MoveGeneral, cx, cz);
		}
	}

	// Network + lockstep barrier (drives GameSim ticks).
	g_Net.Service([&](Net::PacketType type, const uint8_t* data, size_t size, int peerIndex)
	{
		g_Lockstep.OnNetworkPacket(type, data, size, peerIndex, g_Net);
	});
	g_Lockstep.Update(g_Net, g_Sim);

	const float dt = GetFrameTime();
	constexpr float kOrbitSpeed = 6.0f;
	// Q = orbit one way, E = the other (swapped from the previous binding).
	if (IsKeyDown(KEY_Q))
		m_CameraAngle += kOrbitSpeed * dt;
	if (IsKeyDown(KEY_E))
		m_CameraAngle -= kOrbitSpeed * dt;

	constexpr float kZoomWheel = 5.0f;
	constexpr float kZoomMin = 8.0f;
	constexpr float kZoomMax = 120.0f;
	const float wheel = GetMouseWheelMove();
	if (wheel != 0.0f)
		m_CameraDistance = std::clamp(m_CameraDistance - wheel * kZoomWheel, kZoomMin, kZoomMax);

	constexpr float kPanSpeed = 75.0f;
	const float forwardX = std::sin(m_CameraAngle);
	const float forwardZ = std::cos(m_CameraAngle);
	const float rightX = std::cos(m_CameraAngle);
	const float rightZ = -std::sin(m_CameraAngle);
	Vector3 pan{ 0.0f, 0.0f, 0.0f };
	if (IsKeyDown(KEY_W)) { pan.x -= forwardX; pan.z -= forwardZ; }
	if (IsKeyDown(KEY_S)) { pan.x += forwardX; pan.z += forwardZ; }
	if (IsKeyDown(KEY_A)) { pan.x -= rightX; pan.z -= rightZ; }
	if (IsKeyDown(KEY_D)) { pan.x += rightX; pan.z += rightZ; }
	const float panLen = std::sqrt(pan.x * pan.x + pan.z * pan.z);
	if (panLen > 0.0f && g_Terrain)
	{
		pan.x = (pan.x / panLen) * kPanSpeed * dt;
		pan.z = (pan.z / panLen) * kPanSpeed * dt;
		m_LookAt.x = std::clamp(m_LookAt.x + pan.x, 0.0f, static_cast<float>(g_Terrain->m_CellWidth));
		m_LookAt.z = std::clamp(m_LookAt.z + pan.z, 0.0f, static_cast<float>(g_Terrain->m_CellHeight));
	}

	const float elev = 0.55f;
	m_Camera.position = {
		m_LookAt.x + std::sin(m_CameraAngle) * m_CameraDistance,
		m_LookAt.y + m_CameraDistance * elev,
		m_LookAt.z + std::cos(m_CameraAngle) * m_CameraDistance
	};
	if (g_Terrain)
	{
		const float minCamY = g_Terrain->GetMaxHeight() + 0.5f;
		if (m_Camera.position.y < minCamY)
			m_Camera.position.y = minCamY;
	}
	m_Camera.target = m_LookAt;
	m_Hud.SetMinimapCamera(m_LookAt.x, m_LookAt.z, m_CameraAngle);
}

void MainState::Draw()
{
	ClearBackground(Color{ 24, 40, 64, 255 });

	BeginMode3D(m_Camera);
	rlDisableBackfaceCulling();
	if (g_Terrain)
		g_Terrain->Draw();
	g_Sim.DrawUnits(m_Camera);
	// 3D power marker only over the world — do not toggle OS cursor visibility
	// (HideCursor/EnableCursor was snapping the mouse when crossing the HUD).
	if (m_Hud.GetActiveTab() == MainHud::Tab::Powers && g_Lockstep.MatchRunning())
	{
		GodPowerCursor::Draw(
			m_Camera,
			m_Hud.GetSelectedPowerAction(),
			m_TerrainHit,
			m_TerrainHitValid);
	}
	rlEnableBackfaceCulling();
	EndMode3D();

	m_Hud.Draw();

	DrawConsole();

	if (g_showPerfCounter && g_smallFont)
		DrawPerfCounter(g_smallFont.get(), 0);

	if (g_smallFont)
	{
		// Help top-left (console is bottom-left above the perf panel).
		const char* mode = g_Terrain ? g_Terrain->GetDrawModeName() : "";
		const float fs = static_cast<float>(g_smallFont->baseSize);
		DrawOutlinedText(g_smallFont, "WASD pan | QE orbit | Wheel zoom",
			{ 8.0f, 8.0f }, fs, 1, WHITE);
		DrawOutlinedText(g_smallFont, TextFormat("LMB cast | G %s | R regen | F3 perf | Esc", mode),
			{ 8.0f, 8.0f + fs + 2.0f }, fs, 1, WHITE);

		if (g_Sim.MatchOver() && g_font)
		{
			const int winner = g_Sim.WinnerSlot();
			const char* msg = (winner < 0)
				? "DRAW - no walkers remain"
				: (winner == g_Sim.LocalPlayerSlot() ? "VICTORY!" : "DEFEAT");
			const Color col = (winner == g_Sim.LocalPlayerSlot())
				? Color{ 80, 220, 100, 255 }
				: Color{ 220, 80, 80, 255 };
			const float fs = static_cast<float>(g_font->baseSize);
			const Vector2 dim = MeasureTextEx(*g_font, msg, fs, 1);
			const float cx = (g_Engine->m_RenderWidth - m_Hud.PanelRect().width) * 0.5f;
			DrawOutlinedText(g_font, msg, { cx - dim.x * 0.5f, g_Engine->m_RenderHeight * 0.4f }, fs, 1, col);
		}
	}
}
