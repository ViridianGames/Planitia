#ifndef _PLANITIA_MAINHUD_H_
#define _PLANITIA_MAINHUD_H_

#include "GameTypes.h"
#include "raylib.h"

#include <string>

// Vertical game HUD strip (~1/4 of virtual render width).
// Layout top->bottom: minimap, mana, civ status, tab bar, tab content.
class MainHud
{
public:
	enum class Tab : int
	{
		Powers = 0,
		Units = 1,
		Multiplayer = 2,
		Count
	};

	void Init();
	void Shutdown();
	void Update();
	void Draw();

	// True if scaled mouse is over the HUD strip (block world clicks).
	bool IsMouseOver() const;

	Rectangle PanelRect() const;
	Tab GetActiveTab() const { return m_ActiveTab; }

	int GetSelectedPower() const { return m_SelectedPower; }
	int GetSelectedUnitCreate() const { return m_SelectedUnitCreate; }

	// Maps HUD selection -> sim action (for MainState casting).
	PlayerAction GetSelectedPowerAction() const;
	PlayerAction GetSelectedUnitAction() const;
	UnitType GetSelectedUnitType() const;

	// Minimap click -> camera jump. True once per click; fills world XZ.
	bool ConsumeMinimapCameraJump(float& outWorldX, float& outWorldZ);

private:
	void EnsureMinimap();
	void RebuildMinimap();
	void DrawMinimap(float x, float y);
	void DrawManaBar(float x, float y, float width);
	void DrawStatus(float x, float y, float width);
	void DrawTabs(float x, float y, float width);
	void DrawTabContent(float x, float y, float width, float height);
	void DrawPowersTab(float x, float y, float width, float height);
	void DrawUnitsTab(float x, float y, float width, float height);
	void DrawMultiplayerTab(float x, float y, float width, float height);

	float PanelWidth() const;
	float PanelX() const;
	Rectangle MinimapScreenRect() const;

	Tab m_ActiveTab = Tab::Powers;
	int m_SelectedPower = 0;      // 0 = Flatten (wired)
	int m_SelectedUnitCreate = -1;

	// Powers-tab hover tooltip (U7-style delay).
	int m_HoveredPower = -1;
	float m_PowerHoverStart = 0.0f;
	static constexpr float kPowerTooltipDelay = 0.5f;

	bool m_MinimapJumpPending = false;
	float m_MinimapJumpX = 0.0f;
	float m_MinimapJumpZ = 0.0f;

	RenderTexture2D m_MinimapRT{};
	bool m_MinimapReady = false;
	int m_MinimapRebuildTick = -1;

	Texture2D* m_PanelTex = nullptr;
	Texture2D* m_IconsTex = nullptr;
};

#endif
