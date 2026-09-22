#include "MainHud.h"

#include "GameGlobals.h"
#include "GameSim.h"
#include "GameTypes.h"
#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/ResourceManager.h"
#include "Geist/TooltipSystem.h"
#include "Player.h"
#include "Terrain.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr int kMinimapSize = 64;
	constexpr float kPanelFrac = 0.25f;
	constexpr float kPad = 4.0f;
	constexpr float kManaBarH = 10.0f;
	constexpr float kStatusH = 12.0f; // single-line civ status
	constexpr float kTabH = 16.0f;

	float HudTabsY()
	{
		return kPad + kMinimapSize + kPad + kManaBarH + kPad + kStatusH + kPad;
	}

	// Shared layout for draw + hit-test (must stay in sync).
	Rectangle HudContentRect(float panelX, float panelW, float panelH)
	{
		const float tabsY = HudTabsY();
		const float contentY = tabsY + kTabH + 2.0f;
		return Rectangle{ panelX + 1.0f, contentY, panelW - 2.0f, panelH - contentY - kPad };
	}

	// Icon-only power grid (labels live in bottom-of-panel tooltips).
	// Fixed 26x26 cells/icons so borders stay consistent at 480x270 virtual res.
	constexpr int kPowerCols = 4;
	constexpr float kPowerCell = 26.0f;
	constexpr float kPowerIcon = 26.0f;
	constexpr float kPowerGap = 1.0f;
	constexpr float kPowerOuterPad = 2.0f;
	constexpr float kUnitSlotH = 22.0f;
	constexpr float kUnitHeaderH = 14.0f; // label above unit buttons

	Rectangle PowerSlotRect(const Rectangle& content, int index)
	{
		const int col = index % kPowerCols;
		const int row = index / kPowerCols;
		// Center the 4-cell row in the content so leftover width is split evenly.
		const float rowW = kPowerCell * static_cast<float>(kPowerCols)
			+ kPowerGap * static_cast<float>(kPowerCols - 1);
		const float startX = content.x + (content.width - rowW) * 0.5f;
		return Rectangle{
			startX + col * (kPowerCell + kPowerGap),
			content.y + kPowerOuterPad + row * (kPowerCell + kPowerGap),
			kPowerCell,
			kPowerCell
		};
	}

	Rectangle UnitSlotRect(const Rectangle& content, int index)
	{
		return Rectangle{
			content.x + kPad,
			content.y + kUnitHeaderH + index * (kUnitSlotH + 2.0f),
			content.width - kPad * 2,
			kUnitSlotH
		};
	}

	const char* TabName(MainHud::Tab tab)
	{
		switch (tab)
		{
		case MainHud::Tab::Powers: return "Powers";
		case MainHud::Tab::Units: return "Units";
		case MainHud::Tab::Multiplayer: return "Multi";
		default: return "?";
		}
	}

	struct PowerSlot
	{
		const char* name;
		bool available; // false = HUD stub (gray inactive tile, not selectable)
		int tileX;      // top-left of the 2x2 state block in guiicons.png
		int tileY;
		PlayerAction action; // ignored when !available
	};

	// guiicons.png: each power is a 128x128 block of four 64x64 states
	// (TL active, TR highlight, BL alt, BR inactive/gray). Two powers per row.
	constexpr int kIconSrcSize = 64;
	constexpr int kIconBlock = 128;

	const PowerSlot kPowers[] = {
		// Row 0: Flatten | Earthquake
		{ "Flatten", true, 0, 0, PlayerAction::Flatten },
		{ "Earthquake", true, kIconBlock, 0, PlayerAction::Earthquake },
		// Row 1: Stone Rain | Flamestrike
		{ "Stone Rain", true, 0, kIconBlock, PlayerAction::StoneRain },
		{ "Flamestrike", true, kIconBlock, kIconBlock, PlayerAction::Flamestrike },
		// Row 2: Lightning | Lightning Storm
		{ "Lightning", true, 0, kIconBlock * 2, PlayerAction::Lightning },
		{ "Lightning Storm", false, kIconBlock, kIconBlock * 2, PlayerAction::Flatten },
		// Row 3: Bless Land | Swamp
		{ "Bless Land", true, 0, kIconBlock * 3, PlayerAction::Bless },
		{ "Swamp", true, kIconBlock, kIconBlock * 3, PlayerAction::Swamp },
		// Row 4: Volcano | Meteor
		{ "Volcano", false, 0, kIconBlock * 4, PlayerAction::Flatten },
		{ "Meteor", false, kIconBlock, kIconBlock * 4, PlayerAction::Flatten },
		// Row 5: Healing Rain | Golem
		{ "Healing Rain", true, 0, kIconBlock * 5, PlayerAction::HealingLight },
		{ "Golem", false, kIconBlock, kIconBlock * 5, PlayerAction::Flatten },
		// Row 6: Armageddon
		{ "Armageddon", false, 0, kIconBlock * 6, PlayerAction::Flatten },
	};
	constexpr int kPowerCount = static_cast<int>(sizeof(kPowers) / sizeof(kPowers[0]));

	struct UnitSlot
	{
		const char* name;
		bool available;
		PlayerAction action;
		UnitType unitType; // None for MoveGeneral
	};

	const UnitSlot kUnits[] = {
		{ "Archer", true, PlayerAction::CreateArcher, UnitType::Archer },
		{ "Swordsman", true, PlayerAction::CreateWarrior, UnitType::Warrior },
		{ "Barbarian", true, PlayerAction::CreateBarbarian, UnitType::Barbarian },
		{ "General Move", true, PlayerAction::MoveGeneral, UnitType::None },
	};
	constexpr int kUnitCount = static_cast<int>(sizeof(kUnits) / sizeof(kUnits[0]));

	Color TerrainMinimapColor(int type, float height)
	{
		switch (type)
		{
		case TT_WATER: return Color{ 40, 90, 160, 255 };
		case TT_SAND: return Color{ 194, 178, 128, 255 };
		case TT_BEACH: return Color{ 210, 190, 140, 255 };
		case TT_FARMLAND: return Color{ 160, 140, 60, 255 };
		case TT_HOUSE: return Color{ 120, 90, 60, 255 };
		case TT_BLESSEDLAND: return Color{ 180, 220, 100, 255 };
		case TT_SWAMP: return Color{ 60, 90, 50, 255 };
		case TT_LAVA: return Color{ 200, 60, 20, 255 };
		case TT_RUINEDLAND: return Color{ 90, 80, 70, 255 };
		default:
		{
			// Grass shaded by height.
			const int shade = static_cast<int>(std::clamp(70.0f + height * 25.0f, 40.0f, 180.0f));
			return Color{
				static_cast<unsigned char>(shade / 2),
				static_cast<unsigned char>(shade),
				static_cast<unsigned char>(shade / 3),
				255
			};
		}
		}
	}
}

void MainHud::Init()
{
	m_ActiveTab = Tab::Powers;
	m_SelectedPower = 0;
	m_SelectedUnitCreate = -1;
	m_PanelTex = g_ResourceManager->GetTexture("Images/GUI/guipanel.png", false);
	m_IconsTex = g_ResourceManager->GetTexture("Images/GUI/guiicons.png", false);
	if (m_IconsTex && m_IconsTex->id != 0)
	{
		SetTextureFilter(*m_IconsTex, TEXTURE_FILTER_POINT);
		SetTextureWrap(*m_IconsTex, TEXTURE_WRAP_CLAMP);
	}
	m_MinimapArrowTex = g_ResourceManager->GetTexture("Images/minimaparrow.png", false);
	if (m_MinimapArrowTex && m_MinimapArrowTex->id != 0)
	{
		SetTextureFilter(*m_MinimapArrowTex, TEXTURE_FILTER_POINT);
		SetTextureWrap(*m_MinimapArrowTex, TEXTURE_WRAP_CLAMP);
	}
	EnsureMinimap();
}

void MainHud::Shutdown()
{
	if (m_MinimapReady)
	{
		UnloadRenderTexture(m_MinimapRT);
		m_MinimapReady = false;
	}
	m_PanelTex = nullptr;
	m_IconsTex = nullptr;
	m_MinimapArrowTex = nullptr;
}

void MainHud::SetMinimapCamera(float lookAtX, float lookAtZ, float cameraAngleRad)
{
	m_MinimapLookX = lookAtX;
	m_MinimapLookZ = lookAtZ;
	m_MinimapCamAngle = cameraAngleRad;
}

float MainHud::PanelWidth() const
{
	if (!g_Engine)
		return static_cast<float>(kMinimapSize);
	// About 1/4 of render width, but never narrower than the minimap.
	return std::max(g_Engine->m_RenderWidth * kPanelFrac, static_cast<float>(kMinimapSize));
}

float MainHud::PanelX() const
{
	if (!g_Engine)
		return 0.0f;
	return g_Engine->m_RenderWidth - PanelWidth();
}

Rectangle MainHud::PanelRect() const
{
	if (!g_Engine)
		return Rectangle{ 0, 0, 0, 0 };
	return Rectangle{ PanelX(), 0.0f, PanelWidth(), g_Engine->m_RenderHeight };
}

Rectangle MainHud::MinimapScreenRect() const
{
	const float px = PanelX();
	const float pw = PanelWidth();
	const float mapX = px + (pw - static_cast<float>(kMinimapSize)) * 0.5f;
	const float mapY = kPad;
	return Rectangle{ mapX, mapY, static_cast<float>(kMinimapSize), static_cast<float>(kMinimapSize) };
}

bool MainHud::IsMouseOver() const
{
	const Vector2 mouse = GetScaledMousePosition();
	return CheckCollisionPointRec(mouse, PanelRect());
}

bool MainHud::ConsumeMinimapCameraJump(float& outWorldX, float& outWorldZ)
{
	if (!m_MinimapJumpPending)
		return false;
	outWorldX = m_MinimapJumpX;
	outWorldZ = m_MinimapJumpZ;
	m_MinimapJumpPending = false;
	return true;
}

void MainHud::EnsureMinimap()
{
	if (m_MinimapReady)
		return;
	m_MinimapRT = LoadRenderTexture(kMinimapSize, kMinimapSize);
	SetTextureFilter(m_MinimapRT.texture, TEXTURE_FILTER_POINT);
	m_MinimapReady = true;
}

void MainHud::RebuildMinimap()
{
	if (!m_MinimapReady || !g_Terrain)
		return;

	BeginTextureMode(m_MinimapRT);
	ClearBackground(Color{ 20, 30, 40, 255 });

	const int cellsX = g_Terrain->m_CellWidth;
	const int cellsZ = g_Terrain->m_CellHeight;

	auto cellColor = [&](int x, int z) -> Color
	{
		x = std::clamp(x, 0, cellsX - 1);
		z = std::clamp(z, 0, cellsZ - 1);
		return TerrainMinimapColor(g_Terrain->GetTerrainType(x, z), g_Terrain->GetMiddle(x, z));
	};

	auto lerpByte = [](unsigned char a, unsigned char b, float t) -> unsigned char
	{
		return static_cast<unsigned char>(std::lround(a + (b - a) * t));
	};

	auto blend = [&](Color a, Color b, float t) -> Color
	{
		return Color{
			lerpByte(a.r, b.r, t),
			lerpByte(a.g, b.g, t),
			lerpByte(a.b, b.b, t),
			255
		};
	};

	// Fill every minimap pixel by bilinear-sampling the terrain grid so
	// non-integer scales (e.g. 96 from 64) don't leave empty gaps.
	for (int py = 0; py < kMinimapSize; ++py)
	{
		for (int px = 0; px < kMinimapSize; ++px)
		{
			const float fx = (static_cast<float>(px) + 0.5f) * static_cast<float>(cellsX) / static_cast<float>(kMinimapSize) - 0.5f;
			const float fz = (static_cast<float>(py) + 0.5f) * static_cast<float>(cellsZ) / static_cast<float>(kMinimapSize) - 0.5f;
			const int x0 = static_cast<int>(std::floor(fx));
			const int z0 = static_cast<int>(std::floor(fz));
			const float tx = fx - static_cast<float>(x0);
			const float tz = fz - static_cast<float>(z0);

			const Color c00 = cellColor(x0, z0);
			const Color c10 = cellColor(x0 + 1, z0);
			const Color c01 = cellColor(x0, z0 + 1);
			const Color c11 = cellColor(x0 + 1, z0 + 1);
			const Color c = blend(blend(c00, c10, tx), blend(c01, c11, tx), tz);
			DrawPixel(px, py, c);
		}
	}

	// Units / buildings as join-color dots (villages slightly larger).
	const float scale = static_cast<float>(kMinimapSize) / static_cast<float>(cellsX);
	for (const auto& [id, unit] : g_Sim.Units())
	{
		(void)id;
		if (!unit.IsAlive())
			continue;
		Color team = PlayerTeamColor(unit.m_Team);
		if (unit.m_Team >= 0 && unit.m_Team < kMaxPlayers)
			team = g_Sim.GetPlayer(unit.m_Team).TeamColor();
		const float px = unit.m_Pos.x * scale;
		const float py = unit.m_Pos.z * scale;
		const float r = unit.IsVillage() ? 2.5f : 1.5f;
		DrawCircle(static_cast<int>(px), static_cast<int>(py), r, team);
		DrawCircleLines(static_cast<int>(px), static_cast<int>(py), r, BLACK);
	}

	EndTextureMode();
}

void MainHud::Update()
{
	EnsureMinimap();

	// Refresh minimap every few sim ticks (cheap enough to do often).
	const int tick = static_cast<int>(g_Sim.Tick());
	if (tick != m_MinimapRebuildTick && (tick % 2) == 0)
	{
		RebuildMinimap();
		m_MinimapRebuildTick = tick;
	}

	const Vector2 mouse = GetScaledMousePosition();
	const float px = PanelX();
	const float pw = PanelWidth();
	const float ph = g_Engine ? g_Engine->m_RenderHeight : 270.0f;
	const Rectangle content = HudContentRect(px, pw, ph);

	// Track power-icon hover for delayed tooltips (every frame, not only on click).
	int hovered = -1;
	if (m_ActiveTab == Tab::Powers && IsMouseOver() && CheckCollisionPointRec(mouse, content))
	{
		for (int i = 0; i < kPowerCount; ++i)
		{
			if (CheckCollisionPointRec(mouse, PowerSlotRect(content, i)))
			{
				hovered = i;
				break;
			}
		}
	}
	if (hovered != m_HoveredPower)
	{
		m_HoveredPower = hovered;
		m_PowerHoverStart = static_cast<float>(GetTime());
	}

	if (!IsMouseOver() || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		return;

	// Minimap click -> camera jump (handled by MainState via ConsumeMinimapCameraJump).
	const Rectangle mapRect = MinimapScreenRect();
	if (CheckCollisionPointRec(mouse, mapRect) && g_Terrain)
	{
		const float localX = mouse.x - mapRect.x;
		const float localY = mouse.y - mapRect.y;
		const float cellsX = static_cast<float>(g_Terrain->m_CellWidth);
		const float cellsZ = static_cast<float>(g_Terrain->m_CellHeight);
		m_MinimapJumpX = std::clamp(localX * cellsX / static_cast<float>(kMinimapSize), 0.0f, cellsX);
		m_MinimapJumpZ = std::clamp(localY * cellsZ / static_cast<float>(kMinimapSize), 0.0f, cellsZ);
		m_MinimapJumpPending = true;
		return;
	}

	// Tab bar hit-test.
	const float tabsY = HudTabsY();
	if (mouse.y >= tabsY && mouse.y < tabsY + kTabH)
	{
		const float tabW = pw / static_cast<float>(Tab::Count);
		const int idx = static_cast<int>((mouse.x - px) / tabW);
		if (idx >= 0 && idx < static_cast<int>(Tab::Count))
			m_ActiveTab = static_cast<Tab>(idx);
		return;
	}

	if (!CheckCollisionPointRec(mouse, content))
		return;

	if (m_ActiveTab == Tab::Powers)
	{
		for (int i = 0; i < kPowerCount; ++i)
		{
			if (CheckCollisionPointRec(mouse, PowerSlotRect(content, i)) && kPowers[i].available)
			{
				m_SelectedPower = i;
				return;
			}
		}
	}
	else if (m_ActiveTab == Tab::Units)
	{
		for (int i = 0; i < kUnitCount; ++i)
		{
			if (CheckCollisionPointRec(mouse, UnitSlotRect(content, i)) && kUnits[i].available)
			{
				m_SelectedUnitCreate = i;
				// Enqueue military create through lockstep (no world click needed).
				if (kUnits[i].unitType != UnitType::None && g_Lockstep.MatchRunning())
					g_Lockstep.SubmitLocalAction(kUnits[i].action, 0, 0);
				return;
			}
		}
	}
	// Multiplayer lobby lives in MultiplayerMenuState (from Title), not in-game.
}

void MainHud::DrawMinimap(float x, float y)
{
	EnsureMinimap();
	if (!m_MinimapReady)
		return;

	// Border
	DrawRectangle(static_cast<int>(x) - 1, static_cast<int>(y) - 1, kMinimapSize + 2, kMinimapSize + 2, Color{ 10, 14, 20, 255 });
	DrawRectangleLines(static_cast<int>(x) - 1, static_cast<int>(y) - 1, kMinimapSize + 2, kMinimapSize + 2, Color{ 180, 190, 200, 255 });

	// RenderTexture is upside-down in raylib.
	DrawTextureRec(
		m_MinimapRT.texture,
		Rectangle{ 0, 0, static_cast<float>(kMinimapSize), -static_cast<float>(kMinimapSize) },
		Vector2{ x, y },
		WHITE);

	// Camera look-at marker: triangle art faces South (+Z); rotate to view yaw.
	if (m_MinimapArrowTex && m_MinimapArrowTex->id != 0 && g_Terrain)
	{
		const float cellsX = static_cast<float>(g_Terrain->m_CellWidth);
		const float cellsZ = static_cast<float>(g_Terrain->m_CellHeight);
		const float mapScaleX = static_cast<float>(kMinimapSize) / cellsX;
		const float mapScaleZ = static_cast<float>(kMinimapSize) / cellsZ;
		const float ax = x + m_MinimapLookX * mapScaleX;
		const float ay = y + m_MinimapLookZ * mapScaleZ;

		// Camera sits at lookAt + (sinθ, cosθ)*dist and looks toward lookAt, so
		// view dir on XZ is (-sinθ, -cosθ). Art faces +Z (south).
		const float fx = -std::sin(m_MinimapCamAngle);
		const float fz = -std::cos(m_MinimapCamAngle);
		const float rotDeg = -atan2f(fx, fz) * RAD2DEG;

		const float aw = static_cast<float>(m_MinimapArrowTex->width);
		const float ah = static_cast<float>(m_MinimapArrowTex->height);
		constexpr float kArrowDraw = 7.5f;
		const float scale = kArrowDraw / std::max(aw, ah);
		const float dw = aw * scale;
		const float dh = ah * scale;
		DrawTexturePro(
			*m_MinimapArrowTex,
			Rectangle{ 0, 0, aw, ah },
			Rectangle{ ax, ay, dw, dh },
			Vector2{ dw * 0.5f, dh * 0.5f },
			rotDeg,
			WHITE);
	}
}

void MainHud::DrawManaBar(float x, float y, float width)
{
	const Player& p = g_Sim.GetPlayer(g_Sim.LocalPlayerSlot());
	const float pct = (p.m_ManaMax > 0.0f) ? std::clamp(p.m_Mana / p.m_ManaMax, 0.0f, 1.0f) : 0.0f;

	DrawRectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width), static_cast<int>(kManaBarH), Color{ 20, 20, 30, 255 });
	DrawRectangle(
		static_cast<int>(x),
		static_cast<int>(y),
		static_cast<int>(width * pct),
		static_cast<int>(kManaBarH),
		Color{ 60, 120, 220, 255 });
	DrawRectangleLines(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width), static_cast<int>(kManaBarH), Color{ 200, 210, 230, 255 });

	if (g_smallFont)
	{
		DrawOutlinedText(
			g_smallFont,
			TextFormat("Mana %.0f/%.0f", p.m_Mana, p.m_ManaMax),
			{ x + 2.0f, y - 1.0f },
			static_cast<float>(g_smallFont->baseSize),
			1,
			WHITE);
	}
}

void MainHud::DrawStatus(float x, float y, float width)
{
	(void)width;
	if (!g_smallFont)
		return;

	const Player& p = g_Sim.GetPlayer(g_Sim.LocalPlayerSlot());
	DrawOutlinedText(
		g_smallFont,
		TextFormat("Population %d", p.m_Population),
		{ x, y },
		static_cast<float>(g_smallFont->baseSize),
		1,
		WHITE);
}

void MainHud::DrawTabs(float x, float y, float width)
{
	const float tabW = width / static_cast<float>(Tab::Count);
	for (int i = 0; i < static_cast<int>(Tab::Count); ++i)
	{
		const bool active = (static_cast<int>(m_ActiveTab) == i);
		const Rectangle r{ x + i * tabW, y, tabW, kTabH };
		DrawRectangleRec(r, active ? Color{ 50, 70, 100, 255 } : Color{ 25, 35, 50, 255 });
		DrawRectangleLinesEx(r, 1.0f, active ? Color{ 220, 230, 240, 255 } : Color{ 80, 90, 110, 255 });
		if (g_smallFont)
		{
			const char* name = TabName(static_cast<Tab>(i));
			const Vector2 dim = MeasureTextEx(*g_smallFont, name, static_cast<float>(g_smallFont->baseSize), 1);
			DrawOutlinedText(
				g_smallFont,
				name,
				{ r.x + (r.width - dim.x) * 0.5f, r.y + (r.height - dim.y) * 0.5f },
				static_cast<float>(g_smallFont->baseSize),
				1,
				active ? WHITE : Color{ 160, 170, 180, 255 });
		}
	}
}

void MainHud::DrawPowersTab(float x, float y, float width, float height)
{
	(void)height;
	const Rectangle content{ x, y, width, height };
	const Player& local = g_Sim.GetPlayer(g_Sim.LocalPlayerSlot());

	for (int i = 0; i < kPowerCount; ++i)
	{
		const Rectangle r = PowerSlotRect(content, i);
		const bool selected = (m_SelectedPower == i);
		const bool implemented = kPowers[i].available;
		const bool canCast = implemented && g_Sim.CanAfford(local, kPowers[i].action);
		const bool hovered = (m_HoveredPower == i);
		const bool showGray = !canCast; // stub, or not enough mana / on cooldown

		const float iconSize = kPowerIcon;

		Color bg = Color{ 30, 30, 35, 255 };
		if (canCast)
			bg = selected ? Color{ 70, 100, 70, 255 }
				: (hovered ? Color{ 55, 75, 100, 255 } : Color{ 40, 55, 75, 255 });
		else if (selected)
			bg = Color{ 50, 55, 45, 255 };
		DrawRectangleRec(r, bg);

		if (m_IconsTex && m_IconsTex->id != 0)
		{
			// Active = top-left of 2x2; inactive/gray = bottom-right.
			int srcX = kPowers[i].tileX;
			int srcY = kPowers[i].tileY;
			if (showGray)
			{
				srcX += kIconSrcSize;
				srcY += kIconSrcSize;
			}
			const Rectangle src{
				static_cast<float>(srcX),
				static_cast<float>(srcY),
				static_cast<float>(kIconSrcSize),
				static_cast<float>(kIconSrcSize)
			};
			const Rectangle dst{
				r.x + (r.width - iconSize) * 0.5f,
				r.y + (r.height - iconSize) * 0.5f,
				iconSize,
				iconSize
			};
			DrawTexturePro(*m_IconsTex, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
		}

		// Draw selection/hover chrome AFTER the icon so a full-cell sprite can't cover it.
		Color border = Color{ 90, 100, 120, 255 };
		float borderThick = 1.0f;
		if (selected)
		{
			border = Color{ 180, 255, 180, 255 };
			borderThick = 2.0f;
		}
		else if (hovered && implemented)
		{
			border = Color{ 200, 210, 230, 255 };
		}
		DrawRectangleLinesEx(r, borderThick, border);
	}
}

void MainHud::DrawUnitsTab(float x, float y, float width, float height)
{
	const Rectangle content{ x, y, width, height };
	if (g_smallFont)
	{
		DrawOutlinedText(
			g_smallFont,
			"Convert walker at General:",
			{ x + kPad, y + 2.0f },
			static_cast<float>(g_smallFont->baseSize),
			1,
			Color{ 180, 190, 200, 255 });
	}

	for (int i = 0; i < kUnitCount; ++i)
	{
		const Rectangle r = UnitSlotRect(content, i);
		const bool selected = (m_SelectedUnitCreate == i);
		const bool avail = kUnits[i].available;
		Color bg = avail ? Color{ 40, 55, 75, 255 } : Color{ 30, 30, 35, 255 };
		if (selected && avail)
			bg = Color{ 70, 100, 70, 255 };
		DrawRectangleRec(r, bg);
		DrawRectangleLinesEx(r, 1.0f, selected ? Color{ 180, 255, 180, 255 } : Color{ 90, 100, 120, 255 });
		if (g_smallFont)
		{
			DrawOutlinedText(
				g_smallFont,
				kUnits[i].name,
				{ r.x + 4.0f, r.y + (r.height - g_smallFont->baseSize) * 0.5f },
				static_cast<float>(g_smallFont->baseSize),
				1,
				avail ? WHITE : Color{ 120, 120, 130, 255 });
		}
	}
}

PlayerAction MainHud::GetSelectedPowerAction() const
{
	if (m_SelectedPower < 0 || m_SelectedPower >= kPowerCount)
		return PlayerAction::Flatten;
	return kPowers[m_SelectedPower].action;
}

PlayerAction MainHud::GetSelectedUnitAction() const
{
	if (m_SelectedUnitCreate < 0 || m_SelectedUnitCreate >= kUnitCount)
		return PlayerAction::MoveGeneral;
	return kUnits[m_SelectedUnitCreate].action;
}

UnitType MainHud::GetSelectedUnitType() const
{
	if (m_SelectedUnitCreate < 0 || m_SelectedUnitCreate >= kUnitCount)
		return UnitType::None;
	return kUnits[m_SelectedUnitCreate].unitType;
}

void MainHud::DrawMultiplayerTab(float x, float y, float width, float height)
{
	(void)width;
	(void)height;
	if (!g_smallFont)
		return;
	const float fs = static_cast<float>(g_smallFont->baseSize);
	float yy = y + kPad;
	DrawOutlinedText(g_smallFont, "Multiplayer", { x + kPad, yy }, fs, 1, WHITE);
	yy += fs + 4.0f;

	const char* modeStr = "Local skirmish";
	if (g_Net.GetMode() == NetSession::Mode::Host) modeStr = "HOST";
	else if (g_Net.GetMode() == NetSession::Mode::Client) modeStr = "CLIENT";
	DrawOutlinedText(g_smallFont, TextFormat("Mode: %s", modeStr), { x + kPad, yy }, fs, 1, Color{ 200, 210, 220, 255 });
	yy += fs + 2.0f;
	DrawOutlinedText(g_smallFont, g_Net.Status().c_str(), { x + kPad, yy }, fs, 1, Color{ 200, 210, 220, 255 });
	yy += fs + 2.0f;
	DrawOutlinedText(g_smallFont, g_Lockstep.Status().c_str(), { x + kPad, yy }, fs, 1,
		g_Lockstep.IsDesynced() ? RED : Color{ 200, 210, 220, 255 });
	yy += fs + 2.0f;
	DrawOutlinedText(
		g_smallFont,
		TextFormat("Slot %d | Turn %u | Peers %d",
			g_Lockstep.LocalSlot(),
			g_Lockstep.CurrentTurn(),
			g_Net.ConnectedPeerCount()),
		{ x + kPad, yy },
		fs,
		1,
		Color{ 200, 210, 220, 255 });
	yy += fs + 6.0f;
	DrawOutlinedText(g_smallFont, "Host/Join from Title (M).", { x + kPad, yy }, fs, 1, Color{ 160, 170, 180, 255 });
}

void MainHud::DrawTabContent(float x, float y, float width, float height)
{
	DrawRectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width), static_cast<int>(height), Color{ 18, 24, 36, 230 });
	DrawRectangleLines(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width), static_cast<int>(height), Color{ 80, 90, 110, 255 });

	switch (m_ActiveTab)
	{
	case Tab::Powers: DrawPowersTab(x, y, width, height); break;
	case Tab::Units: DrawUnitsTab(x, y, width, height); break;
	case Tab::Multiplayer: DrawMultiplayerTab(x, y, width, height); break;
	default: break;
	}
}

void MainHud::Draw()
{
	if (!g_Engine)
		return;

	const float px = PanelX();
	const float pw = PanelWidth();
	const float ph = g_Engine->m_RenderHeight;

	// Panel background
	DrawRectangle(static_cast<int>(px), 0, static_cast<int>(pw), static_cast<int>(ph), Color{ 22, 28, 40, 240 });
	if (m_PanelTex && m_PanelTex->id != 0)
	{
		DrawTexturePro(
			*m_PanelTex,
			Rectangle{ 0, 0, static_cast<float>(m_PanelTex->width), static_cast<float>(m_PanelTex->height) },
			Rectangle{ px, 0, pw, ph },
			Vector2{ 0, 0 },
			0.0f,
			Color{ 255, 255, 255, 40 });
	}
	DrawRectangleLines(static_cast<int>(px), 0, static_cast<int>(pw), static_cast<int>(ph), Color{ 100, 110, 130, 255 });

	float y = kPad;
	const float mapX = px + (pw - kMinimapSize) * 0.5f;
	DrawMinimap(mapX, y);
	y += kMinimapSize + kPad;

	DrawManaBar(px + kPad, y, pw - kPad * 2);
	y += kManaBarH + kPad;

	DrawStatus(px + kPad, y, pw - kPad * 2);
	y += kStatusH + kPad;

	DrawTabs(px, y, pw);

	const Rectangle content = HudContentRect(px, pw, ph);
	DrawTabContent(content.x, content.y, content.width, content.height);

	// Power name tooltip: bottom of screen, flush with left edge of the HUD panel.
	if (m_ActiveTab == Tab::Powers
		&& m_HoveredPower >= 0
		&& m_HoveredPower < kPowerCount
		&& g_smallFont
		&& (static_cast<float>(GetTime()) - m_PowerHoverStart) >= kPowerTooltipDelay)
	{
		const float fs = static_cast<float>(g_smallFont->baseSize);
		// Lower-right anchor at panel left edge => tooltip sits over the world,
		// with its right edge flush against the HUD.
		DrawToolTip(
			g_smallFont.get(),
			fs,
			kPowers[m_HoveredPower].name,
			static_cast<int>(px),
			static_cast<int>(ph),
			1.0f,
			2, // lower-right anchor
			WHITE);
	}
}
