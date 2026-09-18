#ifndef _PLANITIA_GAMETYPES_H_
#define _PLANITIA_GAMETYPES_H_

#include <cstdint>
#include <string>

constexpr int kMaxPlayers = 4;
constexpr float kSimTickMs = 33.0f;

enum class UnitType : uint8_t
{
	None = 0,
	Walker,
	Archer,
	Warrior,
	Barbarian,
	General,
	Village,
	Arrow,
	// FX / powers (later)
	Lightning,
	Flamestrike,
	Earthquake,
	Count
};

enum class UnitState : uint8_t
{
	Idle = 0,
	Move,
	Attack,
	GatherFood,
	SupplyFood,
	FlattenLand,
	Build,
	Dead,
	Count
};

enum class PlayerAction : uint8_t
{
	Flatten = 0,
	Raise,
	Lower,
	Bless,
	Swamp,
	StoneRain,
	Lightning,
	Flamestrike,
	Earthquake,
	HealingLight,
	CreateArcher,
	CreateBarbarian,
	CreateWarrior,
	MoveGeneral,
	Count
};

// Classic Planitia mana costs (adapted).
inline float ManaCostFor(PlayerAction action)
{
	switch (action)
	{
	case PlayerAction::Flatten:
	case PlayerAction::Raise:
	case PlayerAction::Lower: return 0.1f;
	case PlayerAction::Bless:
	case PlayerAction::StoneRain:
	case PlayerAction::Lightning:
	case PlayerAction::Earthquake: return 5.0f;
	case PlayerAction::Flamestrike:
	case PlayerAction::HealingLight: return 25.0f;
	case PlayerAction::Swamp: return 75.0f;
	case PlayerAction::CreateArcher:
	case PlayerAction::CreateBarbarian:
	case PlayerAction::CreateWarrior:
	case PlayerAction::MoveGeneral: return 0.0f;
	default: return 0.0f;
	}
}

inline const char* PlayerActionName(PlayerAction action)
{
	switch (action)
	{
	case PlayerAction::Flatten: return "Flatten";
	case PlayerAction::Bless: return "Bless";
	case PlayerAction::StoneRain: return "Stone Rain";
	case PlayerAction::Swamp: return "Swamp";
	case PlayerAction::Lightning: return "Lightning";
	case PlayerAction::Flamestrike: return "Flamestrike";
	case PlayerAction::Earthquake: return "Earthquake";
	case PlayerAction::HealingLight: return "Heal Light";
	case PlayerAction::CreateArcher: return "Archer";
	case PlayerAction::CreateBarbarian: return "Barbarian";
	case PlayerAction::CreateWarrior: return "Swordsman";
	case PlayerAction::MoveGeneral: return "General Move";
	default: return "Action";
	}
}

inline const char* UnitTypeName(UnitType t)
{
	switch (t)
	{
	case UnitType::Walker: return "Walker";
	case UnitType::Archer: return "Archer";
	case UnitType::Warrior: return "Warrior";
	case UnitType::Barbarian: return "Barbarian";
	case UnitType::General: return "General";
	case UnitType::Village: return "Village";
	default: return "Unit";
	}
}

#endif
