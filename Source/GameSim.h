#ifndef _PLANITIA_GAMESIM_H_
#define _PLANITIA_GAMESIM_H_

#include "Player.h"
#include "Unit.h"

#include <unordered_map>
#include <vector>

// Deterministic match simulation (lockstep-ready).
class GameSim
{
public:
	void Reset();
	void StartSkirmish(unsigned int mapSeed, int humanSlot = 0, int numPlayers = 2);

	void Update(float dt); // accumulates and runs fixed ticks (offline helper)
	void TickOnce();       // one sim tick (lockstep calls this)
	void DrawUnits(const Camera3D& camera) const;

	// Apply a player command at a lockstep turn boundary (deterministic).
	void ApplyPlayerCommand(PlayerAction action, int playerSlot, int cellX, int cellZ, uint16_t extra = 0);

	int SpawnUnit(UnitType type, int team, float x, float z);
	Unit* GetUnit(int id);
	const Unit* GetUnit(int id) const;
	void DestroyUnit(int id);

	void SetupPlayer(int cellX, int cellZ, int playerSlot, bool isHuman);

	Player& GetPlayer(int slot);
	const Player& GetPlayer(int slot) const;

	int LocalPlayerSlot() const { return m_LocalPlayerSlot; }
	uint32_t Tick() const { return m_Tick; }
	bool MatchStarted() const { return m_Started; }
	bool MatchOver() const { return m_MatchOver; }
	int WinnerSlot() const { return m_WinnerSlot; } // -1 if none / draw

	const std::unordered_map<int, Unit>& Units() const { return m_Units; }

	// Powers / army (will move behind lockstep command queue).
	bool TryFlatten(int playerSlot, int cellX, int cellZ);
	bool TryRaise(int playerSlot, int cellX, int cellZ);
	bool TryLower(int playerSlot, int cellX, int cellZ);
	bool TryCast(PlayerAction action, int playerSlot, int cellX, int cellZ);
	bool TryCreateMilitary(UnitType type, int playerSlot);
	bool TryMoveGeneral(int playerSlot, int cellX, int cellZ);

	// HUD / UI: enough mana, not on spell cooldown, player still active.
	bool CanAfford(const Player& player, PlayerAction action) const;

private:
	void UpdateUnit(Unit& unit, float tickDt);
	void UpdateWalker(Unit& unit, float tickDt);
	void UpdateVillage(Unit& village);
	void UpdateFxUnit(Unit& unit, float tickDt);
	void MoveToward(Unit& unit, float tickDt);
	void ApplyAirPhysics(Unit& unit, float tickDt);
	void TossWalkersInRadius(float x, float z, float radius);
	void JiggleTerrainInRadius(float x, float z, float radius);
	void RegenMana(Player& player);
	void CheckEliminations();
	void RecountPopulation();
	float GroundY(float x, float z) const;
	bool ChargeMana(Player& player, PlayerAction action);
	void DamageEnemiesInRadius(int casterTeam, float x, float z, float radius, float damage);
	void HealFriendliesInRadius(int team, float x, float z, float radius, float amount);
	void ScatterUnitsInRadius(float x, float z, float radius, float strength);
	int SpawnFx(UnitType type, int team, float x, float z, int lifeTicks);
	bool ConvertWalkerToMilitary(UnitType type, int playerSlot);
	void RefreshGeneralSpeed(int playerSlot);

	void LayoutVillageSmall(int cellX, int cellZ);
	void LayoutVillageMedium(int cellX, int cellZ);
	bool VillageHasFarm(const Unit& village) const;
	bool VillageFootprintReady(const Unit& village) const;
	bool FindVillageFarmCell(const Unit& village, int& outX, int& outZ) const;
	bool FindVillageHouseCell(const Unit& village, int& outX, int& outZ) const;
	bool FindUnevenInFootprint(const Unit& village, int& outX, int& outZ) const;
	bool FindUnevenInExpansionSites(const Unit& village, int& outX, int& outZ) const;
	bool TrySpawnDaughterVillage(Unit& village);
	int CountTeamWalkers(int team) const;
	int CountTeamPopulation(int team) const;
	int CellX(const Unit& u) const { return static_cast<int>(u.m_Pos.x); }
	int CellZ(const Unit& u) const { return static_cast<int>(u.m_Pos.z); }

	std::unordered_map<int, Unit> m_Units;
	Player m_Players[kMaxPlayers];
	int m_NextUnitId = 1;
	int m_LocalPlayerSlot = 0;
	uint32_t m_Tick = 0;
	float m_TickAccumulator = 0.0f;
	bool m_Started = false;
	bool m_MatchOver = false;
	int m_WinnerSlot = -1;
	unsigned int m_MapSeed = 0;
};

extern GameSim g_Sim;

#endif
