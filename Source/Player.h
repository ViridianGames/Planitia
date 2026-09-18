#ifndef _PLANITIA_PLAYER_H_
#define _PLANITIA_PLAYER_H_

#include "GameTypes.h"
#include "raylib.h"

#include <string>
#include <vector>

// Fixed god colors: host is always Green; joiners get a random unused color for now.
enum class PlayerColorId : uint8_t
{
	Green = 0,
	Red = 1,
	Blue = 2,
	Yellow = 3,
	Count = 4
};

inline const char* PlayerColorName(PlayerColorId id)
{
	switch (id)
	{
	case PlayerColorId::Green: return "Green";
	case PlayerColorId::Red: return "Red";
	case PlayerColorId::Blue: return "Blue";
	case PlayerColorId::Yellow: return "Yellow";
	default: return "Unknown";
	}
}

inline Color PlayerColorRgb(PlayerColorId id)
{
	static const Color kColors[static_cast<int>(PlayerColorId::Count)] = {
		Color{ 60, 180, 70, 255 },   // Green
		Color{ 200, 60, 50, 255 },   // Red
		Color{ 60, 100, 220, 255 },  // Blue
		Color{ 220, 200, 50, 255 },  // Yellow
	};
	const int i = static_cast<int>(id);
	if (i < 0 || i >= static_cast<int>(PlayerColorId::Count))
		return WHITE;
	return kColors[i];
}

struct Player
{
	bool m_Active = false;
	bool m_IsHuman = false;
	bool m_Eliminated = false;

	float m_Mana = 10.0f;
	float m_ManaMax = 100.0f;
	float m_SpellCooldown = 0.0f; // seconds remaining before next cast
	int m_Population = 0; // living walkers (cap for spawning)
	bool m_DidExpandThisTick = false;

	int m_GeneralId = -1;
	std::vector<int> m_VillageIds;
	std::vector<int> m_ArmyIds;

	PlayerColorId m_Color = PlayerColorId::Green;

	Color TeamColor() const { return PlayerColorRgb(m_Color); }
};

inline Color PlayerTeamColor(int slot)
{
	// Prefer looking up the live player color when possible — kept for call sites
	// that only have a slot index before Players are configured.
	static const Color kFallback[kMaxPlayers] = {
		PlayerColorRgb(PlayerColorId::Green),
		PlayerColorRgb(PlayerColorId::Red),
		PlayerColorRgb(PlayerColorId::Blue),
		PlayerColorRgb(PlayerColorId::Yellow),
	};
	if (slot < 0 || slot >= kMaxPlayers)
		return WHITE;
	return kFallback[slot];
}

#endif
