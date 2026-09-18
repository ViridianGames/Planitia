#ifndef _PLANITIA_UNIT_H_
#define _PLANITIA_UNIT_H_

#include "GameTypes.h"
#include "raylib.h"

// Lightweight sim unit (adapted from classic PlanitiaUnit - no D3D).
// Walker jobs use UnitState (Idle/Move/GatherFood/...) - not Geist StateMachine
// (that system is for Title/Main/Options screens).
struct Unit
{
	int m_Id = -1;
	UnitType m_Type = UnitType::None;
	UnitState m_State = UnitState::Idle;
	int m_Team = -1; // player slot 0..3

	Vector3 m_Pos{};
	Vector3 m_Target{};
	bool m_HasTarget = false;
	float m_VelY = 0.0f; // airborne vertical velocity (earthquake toss, etc.)

	float m_Speed = 1.5f;

	bool IsAirborne() const { return m_VelY != 0.0f; }
	float m_HitPoints = 10.0f;
	float m_MaxHitPoints = 10.0f;
	float m_AttackPower = 1.0f;
	float m_AttackRange = 1.5f;
	float m_AttackCooldown = 0.0f;

	int m_VillageId = -1; // owning village unit id (walkers)
	int m_MasterId = -1;  // general id for military units
	int m_JobTimer = 0;   // ticks remaining on current work step
	int m_EatTimer = 150; // ticks until this walker eats from the village bucket
	int m_LifetimeTicks = 0; // FX powers (lightning/flamestrike/...) die when this hits 0
	UnitState m_NextState = UnitState::Idle; // state to enter when Move arrives
	bool m_CarryFood = false;
	float m_AnimTimer = 0.0f;
	int m_Facing = 0; // 0..3 for sprite dir later

	// Village-only fields (ignored for other types).
	int m_FoodBucket = 0;
	int m_VillageSize = 0; // 0 = small, 1 = medium
	int m_VillagerCount = 0;
	bool m_HasExpanded = false; // each village may spawn at most one daughter

	bool IsFx() const
	{
		return m_Type == UnitType::Lightning
			|| m_Type == UnitType::Flamestrike
			|| m_Type == UnitType::Earthquake;
	}

	bool IsAlive() const { return m_State != UnitState::Dead && m_HitPoints > 0.0f; }
	bool IsMilitary() const
	{
		return m_Type == UnitType::Archer
			|| m_Type == UnitType::Warrior
			|| m_Type == UnitType::Barbarian
			|| m_Type == UnitType::General;
	}
	// Cap of 100: walkers + army. General / Hero do not count.
	bool CountsTowardPopulation() const
	{
		return m_Type == UnitType::Walker
			|| m_Type == UnitType::Archer
			|| m_Type == UnitType::Warrior
			|| m_Type == UnitType::Barbarian;
	}
	bool IsWalker() const { return m_Type == UnitType::Walker; }
	bool IsVillage() const { return m_Type == UnitType::Village; }
};

#endif
