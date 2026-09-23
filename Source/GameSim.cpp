#include "GameSim.h"

#include "GameGlobals.h"
#include "Geist/Logging.h"
#include "Terrain.h"
#include "WalkerSprites.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>

GameSim g_Sim;

namespace
{
	constexpr float kTickDt = kSimTickMs / 1000.0f;
	constexpr float kFlattenTarget = 2.0f;
	constexpr int kFarmWorkTicks = 15;
	constexpr int kEatIntervalTicks = 150;
	constexpr int kFoodPerTrip = 2;
	constexpr int kMaxWalkersHouse = 5;
	constexpr int kMaxWalkersManor = 10;
	constexpr int kMaxPopulation = 100;
	constexpr float kArriveDist = 0.12f;

	// Keep ground-moving units slightly inside the heightfield so GetHeight stays valid.
	// (Air knockback may still leave the map later - that kills the unit.)
	void ClampToMap(float& x, float& z)
	{
		if (!g_Terrain)
			return;
		const float minX = 0.5f;
		const float minZ = 0.5f;
		const float maxX = static_cast<float>(g_Terrain->m_CellWidth) - 0.5f;
		const float maxZ = static_cast<float>(g_Terrain->m_CellHeight) - 0.5f;
		x = std::clamp(x, minX, maxX);
		z = std::clamp(z, minZ, maxZ);
	}

	int RandOffset()
	{
		if (!g_vitalRNG)
			return 0;
		return static_cast<int>(g_vitalRNG->RandomRange(0, 2)) - 1; // -1, 0, 1
	}

	// Map world XZ delta → U7 walk facing (0=SW .. 7=S).
	int FacingFromDelta(float dx, float dz)
	{
		if (dx * dx + dz * dz < 1e-8f)
			return -1;
		float a = atan2f(dx, dz); // 0 = +Z (S)
		a += PI / 8.0f;
		if (a < 0.0f)
			a += 2.0f * PI;
		const int sector = static_cast<int>(a / (PI / 4.0f)) % 8;
		static const int kSectorToFacing[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };
		return kSectorToFacing[sector];
	}
}

void GameSim::Reset()
{
	m_Units.clear();
	for (int i = 0; i < kMaxPlayers; ++i)
		m_Players[i] = Player{};
	m_NextUnitId = 1;
	m_Tick = 0;
	m_TickAccumulator = 0.0f;
	m_Started = false;
	m_MatchOver = false;
	m_WinnerSlot = -1;
}

void GameSim::StartSkirmish(unsigned int mapSeed, int humanSlot, int numPlayers)
{
	Reset();
	m_MapSeed = mapSeed;
	m_LocalPlayerSlot = humanSlot;
	m_Started = true;

	if (!g_vitalRNG)
		g_vitalRNG = std::make_unique<RNG>();
	g_vitalRNG->SeedRNG(mapSeed);

	if (!g_Terrain)
	{
		g_Terrain = std::make_unique<Terrain>();
		g_Terrain->Init("Data/Maps/MainMenuTerrain.txt");
	}
	g_Terrain->InitializeMap(mapSeed);

	numPlayers = std::clamp(numPlayers, 1, kMaxPlayers);
	const int corners[kMaxPlayers][2] = {
		{ 16, 16 },
		{ 48, 48 },
		{ 16, 48 },
		{ 48, 16 },
	};

	for (int i = 0; i < numPlayers; ++i)
	{
		const bool human = (i == humanSlot);
		SetupPlayer(corners[i][0], corners[i][1], i, human);
	}

	// Rebuild after settlement pads so the flat 5x5 is visible immediately.
	g_Terrain->RebuildMesh();
	RecountPopulation();

	Log("Skirmish started seed=" + std::to_string(mapSeed)
		+ " players=" + std::to_string(numPlayers)
		+ " local=" + std::to_string(humanSlot));
}

Player& GameSim::GetPlayer(int slot)
{
	return m_Players[std::clamp(slot, 0, kMaxPlayers - 1)];
}

const Player& GameSim::GetPlayer(int slot) const
{
	return m_Players[std::clamp(slot, 0, kMaxPlayers - 1)];
}

float GameSim::GroundY(float x, float z) const
{
	if (!g_Terrain)
		return 0.0f;
	const float h = g_Terrain->GetHeight(x, z);
	return (h < 0.0f) ? 0.0f : h;
}

int GameSim::CountTeamWalkers(int team) const
{
	int n = 0;
	for (const auto& [id, unit] : m_Units)
	{
		(void)id;
		if (unit.IsAlive() && unit.m_Team == team && unit.IsWalker())
			++n;
	}
	return n;
}

// Living followers: walkers + army + general. Villages alone do not keep you alive.
int GameSim::CountTeamSurvivors(int team) const
{
	int n = 0;
	for (const auto& [id, unit] : m_Units)
	{
		(void)id;
		if (!unit.IsAlive() || unit.m_Team != team)
			continue;
		if (unit.IsWalker() || unit.IsMilitary())
			++n;
	}
	return n;
}

int GameSim::CountTeamPopulation(int team) const
{
	// Walkers + archers/swordsmen/barbarians. General/Hero excluded.
	int n = 0;
	for (const auto& [id, unit] : m_Units)
	{
		(void)id;
		if (unit.IsAlive() && unit.m_Team == team && unit.CountsTowardPopulation())
			++n;
	}
	return n;
}

void GameSim::RecountPopulation()
{
	for (int i = 0; i < kMaxPlayers; ++i)
		m_Players[i].m_Population = CountTeamPopulation(i);

	for (auto& [id, unit] : m_Units)
	{
		(void)id;
		if (!unit.IsAlive() || !unit.IsVillage())
			continue;
		int count = 0;
		for (const auto& [wid, walker] : m_Units)
		{
			(void)wid;
			if (walker.IsAlive() && walker.IsWalker() && walker.m_VillageId == unit.m_Id)
				++count;
		}
		unit.m_VillagerCount = count;
	}
}

int GameSim::SpawnUnit(UnitType type, int team, float x, float z)
{
	Unit unit;
	unit.m_Id = m_NextUnitId++;
	unit.m_Type = type;
	unit.m_Team = team;
	unit.m_State = UnitState::Idle;
	unit.m_Pos = { x, GroundY(x, z), z };
	unit.m_Target = unit.m_Pos;
	unit.m_EatTimer = kEatIntervalTicks;

	switch (type)
	{
	case UnitType::Walker:
		unit.m_Speed = 1.6f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 10.0f;
		break;
	case UnitType::Archer:
		unit.m_Speed = 1.8f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 8.0f;
		unit.m_AttackRange = 6.0f;
		break;
	case UnitType::Warrior:
		unit.m_Speed = 1.2f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 18.0f;
		break;
	case UnitType::Barbarian:
		unit.m_Speed = 2.4f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 12.0f;
		break;
	case UnitType::General:
		unit.m_Speed = 1.7f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 25.0f;
		break;
	case UnitType::Village:
		unit.m_Speed = 0.0f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 100.0f;
		unit.m_FoodBucket = 15;
		unit.m_VillageSize = 0;
		unit.m_VillagerCount = 0;
		break;
	case UnitType::Lightning:
		unit.m_Speed = 0.0f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 1.0f;
		unit.m_LifetimeTicks = 8;
		break;
	case UnitType::Flamestrike:
		unit.m_Speed = 1.2f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 1.0f;
		unit.m_LifetimeTicks = 90; // ~3s
		unit.m_AttackPower = 2.0f;
		break;
	case UnitType::Earthquake:
		unit.m_Speed = 0.0f;
		unit.m_HitPoints = unit.m_MaxHitPoints = 1.0f;
		unit.m_LifetimeTicks = 45; // ~1.5s
		unit.m_AttackPower = 5.0f; // one-shot chip damage (see UpdateFxUnit)
		break;
	default:
		break;
	}

	const int id = unit.m_Id;
	m_Units.emplace(id, unit);

	if (team >= 0 && team < kMaxPlayers)
	{
		if (type == UnitType::Village)
			m_Players[team].m_VillageIds.push_back(id);
		else if (unit.IsMilitary())
			m_Players[team].m_ArmyIds.push_back(id);
		if (type == UnitType::General)
			m_Players[team].m_GeneralId = id;
	}
	return id;
}

Unit* GameSim::GetUnit(int id)
{
	auto it = m_Units.find(id);
	return (it == m_Units.end()) ? nullptr : &it->second;
}

const Unit* GameSim::GetUnit(int id) const
{
	auto it = m_Units.find(id);
	return (it == m_Units.end()) ? nullptr : &it->second;
}

void GameSim::DestroyUnit(int id)
{
	auto it = m_Units.find(id);
	if (it == m_Units.end())
		return;
	it->second.m_State = UnitState::Dead;
	it->second.m_HitPoints = 0.0f;
}

void GameSim::LayoutVillageSmall(int cellX, int cellZ)
{
	if (!g_Terrain)
		return;
	// House in the center; four farms in the cardinal directions only.
	static const int kFarms[4][2] = {
		{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
	};
	auto placeFarm = [&](int x, int z)
	{
		const int existing = g_Terrain->GetTerrainType(x, z);
		if (existing == TT_BLESSEDLAND)
			return; // never overwrite bless
		g_Terrain->SetTerrainType(x, z, TT_FARMLAND);
	};
	g_Terrain->SetTerrainType(cellX, cellZ, TT_HOUSE);
	for (const auto& d : kFarms)
		placeFarm(cellX + d[0], cellZ + d[1]);
	g_Terrain->MarkMeshDirty();
}

void GameSim::LayoutVillageMedium(int cellX, int cellZ)
{
	if (!g_Terrain)
		return;
	// Manor: keep a single house tile; all 8 neighbors become farmland.
	// Blessed land is preserved (still farmable at 2x yield).
	g_Terrain->SetTerrainType(cellX, cellZ, TT_HOUSE);
	for (int i = -1; i <= 1; ++i)
	{
		for (int j = -1; j <= 1; ++j)
		{
			if (i == 0 && j == 0)
				continue;
			const int x = cellX + i;
			const int z = cellZ + j;
			const int existing = g_Terrain->GetTerrainType(x, z);
			if (existing == TT_BLESSEDLAND)
				continue;
			g_Terrain->SetTerrainType(x, z, TT_FARMLAND);
		}
	}
	g_Terrain->MarkMeshDirty();
}

void GameSim::SetupPlayer(int cellX, int cellZ, int playerSlot, bool isHuman)
{
	if (playerSlot < 0 || playerSlot >= kMaxPlayers || !g_Terrain)
		return;

	Player& player = m_Players[playerSlot];
	player = Player{};
	player.m_Active = true;
	player.m_IsHuman = isHuman;
	player.m_Mana = 0.0f;

	// Carve a flat 5x5 settlement pad (cells cell+/-2), then drop the 3x3 farm in the middle.
	// Don't raise a hill first - that buried villages inside peaks.
	constexpr float kPadHeight = 2.0f;
	for (int k = cellX - 2; k <= cellX + 3; ++k)
		for (int l = cellZ - 2; l <= cellZ + 3; ++l)
			g_Terrain->SetValue(k, l, kPadHeight);

	for (int k = cellX - 2; k <= cellX + 2; ++k)
		for (int l = cellZ - 2; l <= cellZ + 2; ++l)
			g_Terrain->ScrubTerrainCell(k, l);

	LayoutVillageSmall(cellX, cellZ);

	const float wx = static_cast<float>(cellX) + 0.5f;
	const float wz = static_cast<float>(cellZ) + 0.5f;
	const int villageId = SpawnUnit(UnitType::Village, playerSlot, wx, wz);
	if (Unit* village = GetUnit(villageId))
	{
		village->m_FoodBucket = 15;
		village->m_VillageSize = 0;
		village->m_VillagerCount = 3;
	}

	// Three starting walkers (design doc / classic Village::Update).
	for (int i = 0; i < 3; ++i)
	{
		const float ox = wx + static_cast<float>((i % 2) * 2 - 1) * 0.8f;
		const float oz = wz + static_cast<float>((i / 2) * 2 - 1) * 0.8f;
		const int wid = SpawnUnit(UnitType::Walker, playerSlot, ox, oz);
		if (Unit* w = GetUnit(wid))
			w->m_VillageId = villageId;
	}
}

void GameSim::Update(float dt)
{
	if (!m_Started)
		return;

	m_TickAccumulator += dt;
	while (m_TickAccumulator >= kTickDt)
	{
		m_TickAccumulator -= kTickDt;
		TickOnce();
	}

	if (g_Terrain)
		g_Terrain->Update();
}

void GameSim::TickOnce()
{
	++m_Tick;

	for (int i = 0; i < kMaxPlayers; ++i)
	{
		m_Players[i].m_DidExpandThisTick = false;
		if (m_Players[i].m_SpellCooldown > 0.0f)
			m_Players[i].m_SpellCooldown = std::max(0.0f, m_Players[i].m_SpellCooldown - kTickDt);
		if (m_Players[i].m_GeneralId >= 0)
			RefreshGeneralSpeed(i);
	}

	// Snapshot + sort ids so update order is deterministic across peers
	// (std::unordered_map iteration order is not portable).
	std::vector<int> ids;
	ids.reserve(m_Units.size());
	for (const auto& [id, unit] : m_Units)
	{
		(void)unit;
		ids.push_back(id);
	}
	std::sort(ids.begin(), ids.end());

	for (int id : ids)
	{
		Unit* unit = GetUnit(id);
		if (!unit || !unit->IsAlive())
			continue;
		UpdateUnit(*unit, kTickDt);
	}

	// Found new villages in founding order. Always prefer open pads around the
	// oldest village; only when it has no free neighbor do we try the next, etc.
	// (One expand per team per tick; m_DidExpandThisTick is set inside TrySpawnDaughterVillage.)
	for (int p = 0; p < kMaxPlayers; ++p)
	{
		Player& player = m_Players[p];
		if (!player.m_Active || player.m_Eliminated || player.m_DidExpandThisTick)
			continue;
		for (int villageId : player.m_VillageIds)
		{
			Unit* village = GetUnit(villageId);
			if (!village || !village->IsAlive() || !village->IsVillage())
				continue;
			if (village->m_VillageSize != 1)
				continue;
			if (!HasOpenDaughterSite(*village))
				continue; // surrounded / blocked — try the next oldest village
			// Oldest medium village that still has a free pad: wait on it (don't branch
			// from younger towns) until it can afford to found, then found here.
			if (village->m_VillagerCount >= kMaxWalkersManor)
				TrySpawnDaughterVillage(*village);
			break;
		}
	}

	for (int i = 0; i < kMaxPlayers; ++i)
	{
		if (m_Players[i].m_Active && !m_Players[i].m_Eliminated)
			RegenMana(m_Players[i]);
	}

	RecountPopulation();
	CheckEliminations();

	// Purge dead (and drop village links / lists).
	// Note: villager counts were already fixed by RecountPopulation - don't decrement again.
	for (auto it = m_Units.begin(); it != m_Units.end();)
	{
		if (!it->second.IsAlive())
		{
			const Unit& dead = it->second;
			if (dead.IsVillage() && dead.m_Team >= 0 && dead.m_Team < kMaxPlayers)
			{
				auto& list = m_Players[dead.m_Team].m_VillageIds;
				list.erase(std::remove(list.begin(), list.end(), dead.m_Id), list.end());
			}
			if (dead.m_Type == UnitType::General && dead.m_Team >= 0 && dead.m_Team < kMaxPlayers)
			{
				if (m_Players[dead.m_Team].m_GeneralId == dead.m_Id)
					m_Players[dead.m_Team].m_GeneralId = -1;
			}
			if (dead.IsMilitary() && dead.m_Team >= 0 && dead.m_Team < kMaxPlayers)
			{
				auto& army = m_Players[dead.m_Team].m_ArmyIds;
				army.erase(std::remove(army.begin(), army.end(), dead.m_Id), army.end());
			}
			it = m_Units.erase(it);
		}
		else
			++it;
	}
}

void GameSim::RegenMana(Player& player)
{
	const int team = static_cast<int>(&player - m_Players);
	const int walkers = CountTeamWalkers(team);
	// Villagers / 1000 per sim tick (~1/5 of the old /200 rate).
	player.m_Mana = std::min(player.m_ManaMax, player.m_Mana + static_cast<float>(walkers) / 1000.0f);
}

void GameSim::CheckEliminations()
{
	if (m_MatchOver)
		return;

	for (int p = 0; p < kMaxPlayers; ++p)
	{
		if (!m_Players[p].m_Active || m_Players[p].m_Eliminated)
			continue;
		// Must use survivors (walkers + military), not walkers alone — converting
		// your last peasants into troops used to wipe you while the army lived.
		if (CountTeamSurvivors(p) > 0)
			continue;

		m_Players[p].m_Eliminated = true;
		// Empty houses still drew as team dots and looked like "1 villager left".
		for (auto& [id, unit] : m_Units)
		{
			(void)id;
			if (unit.IsAlive() && unit.IsVillage() && unit.m_Team == p)
				DestroyUnit(unit.m_Id);
		}
		Log("Player " + std::to_string(p) + " eliminated (no survivors).");
	}

	int alive = 0;
	int lastAlive = -1;
	for (int p = 0; p < kMaxPlayers; ++p)
	{
		if (!m_Players[p].m_Active)
			continue;
		if (!m_Players[p].m_Eliminated)
		{
			++alive;
			lastAlive = p;
		}
	}

	if (alive <= 1)
	{
		m_MatchOver = true;
		m_WinnerSlot = lastAlive; // -1 if everyone wiped
		if (m_WinnerSlot >= 0)
			Log("Match over - winner slot " + std::to_string(m_WinnerSlot));
		else
			Log("Match over - draw (no survivors left).");
	}
}

bool GameSim::VillageHasFarm(const Unit& village) const
{
	if (!g_Terrain)
		return false;
	const int vx = CellX(village);
	const int vz = CellZ(village);
	for (int i = -1; i <= 1; ++i)
	{
		for (int j = -1; j <= 1; ++j)
		{
			const int t = g_Terrain->GetTerrainType(vx + i, vz + j);
			if (t == TT_FARMLAND || t == TT_BLESSEDLAND)
				return true;
		}
	}
	return false;
}

bool GameSim::IsFarmCellClaimed(int villageId, int cellX, int cellZ, int excludeWalkerId) const
{
	for (const auto& [id, u] : m_Units)
	{
		if (id == excludeWalkerId || !u.IsAlive() || !u.IsWalker())
			continue;
		if (u.m_VillageId != villageId)
			continue;
		// Already working that cell.
		if (u.m_State == UnitState::GatherFood
			&& static_cast<int>(u.m_Pos.x) == cellX
			&& static_cast<int>(u.m_Pos.z) == cellZ)
			return true;
		// En route to gather there.
		if (u.m_State == UnitState::Move
			&& u.m_NextState == UnitState::GatherFood
			&& u.m_HasTarget
			&& static_cast<int>(u.m_Target.x) == cellX
			&& static_cast<int>(u.m_Target.z) == cellZ)
			return true;
	}
	return false;
}

bool GameSim::FindVillageFarmCell(const Unit& village, int& outX, int& outZ, int excludeWalkerId) const
{
	if (!g_Terrain)
		return false;
	const int vx = CellX(village);
	const int vz = CellZ(village);

	auto tryType = [&](int want) -> bool
	{
		for (int attempt = 0; attempt < 12; ++attempt)
		{
			const int x = vx + RandOffset();
			const int z = vz + RandOffset();
			if (g_Terrain->GetTerrainType(x, z) != want)
				continue;
			if (IsFarmCellClaimed(village.m_Id, x, z, excludeWalkerId))
				continue;
			outX = x;
			outZ = z;
			return true;
		}
		for (int i = -1; i <= 1; ++i)
		{
			for (int j = -1; j <= 1; ++j)
			{
				const int x = vx + i;
				const int z = vz + j;
				if (g_Terrain->GetTerrainType(x, z) != want)
					continue;
				if (IsFarmCellClaimed(village.m_Id, x, z, excludeWalkerId))
					continue;
				outX = x;
				outZ = z;
				return true;
			}
		}
		return false;
	};

	// Prefer free blessed tiles; never pick ruined.
	if (tryType(TT_BLESSEDLAND))
		return true;
	return tryType(TT_FARMLAND);
}

bool GameSim::FindVillageHouseCell(const Unit& village, int& outX, int& outZ) const
{
	if (!g_Terrain)
		return false;
	const int vx = CellX(village);
	const int vz = CellZ(village);
	for (int attempt = 0; attempt < 12; ++attempt)
	{
		const int x = vx + RandOffset();
		const int z = vz + RandOffset();
		if (g_Terrain->GetTerrainType(x, z) == TT_HOUSE)
		{
			outX = x;
			outZ = z;
			return true;
		}
	}
	for (int i = -1; i <= 1; ++i)
	{
		for (int j = -1; j <= 1; ++j)
		{
			if (g_Terrain->GetTerrainType(vx + i, vz + j) == TT_HOUSE)
			{
				outX = vx + i;
				outZ = vz + j;
				return true;
			}
		}
	}
	outX = vx;
	outZ = vz;
	return true;
}

bool GameSim::VillageFootprintReady(const Unit& village) const
{
	if (!g_Terrain)
		return false;
	const int vx = CellX(village);
	const int vz = CellZ(village);
	for (int i = vx - 1; i <= vx + 1; ++i)
	{
		for (int j = vz - 1; j <= vz + 1; ++j)
		{
			if (!g_Terrain->IsValidVillageTerrain(i, j))
				return false;
		}
	}
	return true;
}

bool GameSim::FindUnevenInFootprint(const Unit& village, int& outX, int& outZ) const
{
	if (!g_Terrain)
		return false;
	const int vx = CellX(village);
	const int vz = CellZ(village);
	// Center, then orthogonal, then diagonal — same priority as expansion.
	static const int kCells[9][2] = {
		{ 0, 0 },
		{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
		{ 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 },
	};
	for (const auto& d : kCells)
	{
		const int x = vx + d[0];
		const int z = vz + d[1];
		if (x < 1 || z < 1 || x >= g_Terrain->m_CellWidth - 1 || z >= g_Terrain->m_CellHeight - 1)
			continue;
		if (!g_Terrain->IsValidVillageTerrain(x, z))
		{
			outX = x;
			outZ = z;
			return true;
		}
	}
	return false;
}

bool GameSim::FindUnevenInExpansionSites(const Unit& village, int& outX, int& outZ) const
{
	if (!g_Terrain)
		return false;
	const int vx = CellX(village);
	const int vz = CellZ(village);

	// Prefer orthogonal expansion pads; diagonals only if those are already flat/blocked.
	static const int kOrtho[4][2] = {
		{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
	};
	static const int kDiag[4][2] = {
		{ 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 },
	};
	static const int kCells[9][2] = {
		{ 0, 0 },
		{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
		{ 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 },
	};

	auto scanSite = [&](int di, int dj) -> bool
	{
		const int cx = vx + di * 3;
		const int cz = vz + dj * 3;
		for (const auto& c : kCells)
		{
			const int x = cx + c[0];
			const int z = cz + c[1];
			if (x < 1 || z < 1 || x >= g_Terrain->m_CellWidth - 1 || z >= g_Terrain->m_CellHeight - 1)
				continue;
			if (!g_Terrain->IsValidVillageTerrain(x, z))
			{
				outX = x;
				outZ = z;
				return true;
			}
		}
		return false;
	};

	for (const auto& d : kOrtho)
	{
		if (scanSite(d[0], d[1]))
			return true;
	}
	for (const auto& d : kDiag)
	{
		if (scanSite(d[0], d[1]))
			return true;
	}
	return false;
}

void GameSim::MoveToward(Unit& unit, float tickDt)
{
	if (!unit.m_HasTarget)
		return;

	ClampToMap(unit.m_Target.x, unit.m_Target.z);
	unit.m_Target.y = GroundY(unit.m_Target.x, unit.m_Target.z);

	Vector3 delta = Vector3Subtract(unit.m_Target, unit.m_Pos);
	delta.y = 0.0f;
	const float dist = Vector3Length(delta);
	if (dist < kArriveDist)
	{
		unit.m_Pos.x = unit.m_Target.x;
		unit.m_Pos.z = unit.m_Target.z;
		unit.m_HasTarget = false;
		unit.m_Pos.y = GroundY(unit.m_Pos.x, unit.m_Pos.z);
		return;
	}

	const float step = unit.m_Speed * tickDt;
	if (step >= dist)
	{
		unit.m_Pos.x = unit.m_Target.x;
		unit.m_Pos.z = unit.m_Target.z;
		unit.m_HasTarget = false;
	}
	else
	{
		delta = Vector3Scale(Vector3Normalize(delta), step);
		unit.m_Pos.x += delta.x;
		unit.m_Pos.z += delta.z;
	}
	const int facing = FacingFromDelta(delta.x, delta.z);
	if (facing >= 0)
		unit.m_Facing = facing;
	ClampToMap(unit.m_Pos.x, unit.m_Pos.z);
	unit.m_Pos.y = GroundY(unit.m_Pos.x, unit.m_Pos.z);
}

void GameSim::UpdateWalker(Unit& unit, float tickDt)
{
	Unit* village = (unit.m_VillageId >= 0) ? GetUnit(unit.m_VillageId) : nullptr;
	if (!village || !village->IsAlive() || !village->IsVillage())
	{
		// Orphaned walker dies with their village.
		DestroyUnit(unit.m_Id);
		return;
	}

	// Eating draws from the village food bucket (classic Walker::Update).
	if (unit.m_EatTimer <= 0)
	{
		village->m_FoodBucket -= 1;
		unit.m_EatTimer = kEatIntervalTicks;
	}
	else
	{
		--unit.m_EatTimer;
	}

	auto setMoveToCell = [&](int cellX, int cellZ, UnitState arriveAs)
	{
		float x = static_cast<float>(cellX) + 0.5f;
		float z = static_cast<float>(cellZ) + 0.5f;
		ClampToMap(x, z);
		unit.m_Target = { x, GroundY(x, z), z };
		unit.m_HasTarget = true;
		unit.m_State = UnitState::Move;
		unit.m_NextState = arriveAs;
		unit.m_CarryFood = (arriveAs == UnitState::SupplyFood);
	};

	switch (unit.m_State)
	{
	case UnitState::Idle:
	{
		int fx = 0;
		int fz = 0;
		// Walkers only farm. Raising/lowering/flattening land is a god-power job
		// so expansion pads stay for the player to prepare.
		if (VillageHasFarm(*village) && FindVillageFarmCell(*village, fx, fz, unit.m_Id))
			setMoveToCell(fx, fz, UnitState::GatherFood);
		break;
	}

	case UnitState::Move:
	{
		MoveToward(unit, tickDt);
		if (!unit.m_HasTarget)
		{
			const UnitState next = unit.m_NextState;
			unit.m_NextState = UnitState::Idle;
			if (next == UnitState::GatherFood)
			{
				unit.m_State = UnitState::GatherFood;
				unit.m_JobTimer = kFarmWorkTicks;
			}
			else if (next == UnitState::SupplyFood)
			{
				village->m_FoodBucket += unit.m_FoodCarried;
				unit.m_FoodCarried = 0;
				unit.m_CarryFood = false;
				unit.m_State = UnitState::Idle;
				unit.m_JobTimer = 0;
			}
			else
			{
				unit.m_State = UnitState::Idle;
				unit.m_JobTimer = 0;
			}
		}
		break;
	}

	case UnitState::GatherFood:
	{
		if (unit.m_JobTimer > 0)
		{
			--unit.m_JobTimer;
			break;
		}
		// Yield depends on the tile underfoot when the job finishes.
		int amount = 0;
		if (g_Terrain)
		{
			const int t = g_Terrain->GetTerrainType(
				static_cast<int>(unit.m_Pos.x), static_cast<int>(unit.m_Pos.z));
			if (t == TT_BLESSEDLAND)
				amount = kFoodPerTrip * 2;
			else if (t == TT_FARMLAND)
				amount = kFoodPerTrip;
			// TT_RUINEDLAND and anything else: no food
		}
		unit.m_FoodCarried = amount;
		int hx = 0;
		int hz = 0;
		FindVillageHouseCell(*village, hx, hz);
		setMoveToCell(hx, hz, UnitState::SupplyFood);
		unit.m_CarryFood = amount > 0; // setMoveToCell would force true for SupplyFood
		break;
	}

	case UnitState::FlattenLand:
		// Legacy state — walkers no longer reshape terrain.
		unit.m_State = UnitState::Idle;
		unit.m_JobTimer = 0;
		break;

	case UnitState::SupplyFood:
	{
		// Handled on Move arrival; shouldn't linger here.
		unit.m_State = UnitState::Idle;
		break;
	}

	default:
		unit.m_State = UnitState::Idle;
		break;
	}
}

bool GameSim::HasOpenDaughterSite(const Unit& village) const
{
	if (!g_Terrain)
		return false;

	const int vx = CellX(village);
	const int vz = CellZ(village);
	static const int kOrtho[4][2] = {
		{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
	};
	static const int kDiag[4][2] = {
		{ 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 },
	};

	auto siteFree = [&](int di, int dj) -> bool
	{
		const int x = vx + (di * 3);
		const int z = vz + (dj * 3);
		if (x < 1 || z < 1 || x >= g_Terrain->m_CellWidth - 1 || z >= g_Terrain->m_CellHeight - 1)
			return false;
		for (int k = x - 1; k <= x + 1; ++k)
		{
			for (int l = z - 1; l <= z + 1; ++l)
			{
				if (!g_Terrain->IsValidVillageTerrain(k, l))
					return false;
			}
		}
		for (const auto& [id, other] : m_Units)
		{
			(void)id;
			if (!other.IsAlive() || !other.IsVillage())
				continue;
			const int ox = CellX(other);
			const int oz = CellZ(other);
			if (std::abs(ox - x) <= 2 && std::abs(oz - z) <= 2)
				return false;
		}
		return true;
	};

	for (const auto& d : kOrtho)
	{
		if (siteFree(d[0], d[1]))
			return true;
	}
	for (const auto& d : kDiag)
	{
		if (siteFree(d[0], d[1]))
			return true;
	}
	return false;
}

bool GameSim::TrySpawnDaughterVillage(Unit& village)
{
	if (!g_Terrain || village.m_Team < 0 || village.m_Team >= kMaxPlayers)
		return false;
	Player& player = m_Players[village.m_Team];
	if (player.m_DidExpandThisTick)
		return false;

	// Founding costs 5 walkers from the parent (regen dips until pop recovers).
	constexpr int kFoundingWalkerCost = 5;
	if (village.m_VillagerCount < kFoundingWalkerCost)
		return false;

	// Manor at capacity (10) founds a daughter and drops back toward 5 via founding cost.
	if (village.m_VillageSize != 1 || village.m_VillagerCount < kMaxWalkersManor)
		return false;

	const int vx = CellX(village);
	const int vz = CellZ(village);

	// Prefer orthogonal neighbors (N/E/S/W); only use diagonals if none fit.
	// Offsets are in 3-cell steps so 3x3 farm pads don't overlap.
	static const int kOrtho[4][2] = {
		{ 0, -1 }, // N
		{ 1, 0 },  // E
		{ 0, 1 },  // S
		{ -1, 0 }, // W
	};
	static const int kDiag[4][2] = {
		{ 1, -1 },  // NE
		{ 1, 1 },   // SE
		{ -1, 1 },  // SW
		{ -1, -1 }, // NW
	};

	auto tryOffset = [&](int di, int dj) -> bool
	{
		const int x = vx + (di * 3);
		const int z = vz + (dj * 3);
		if (x < 1 || z < 1 || x >= g_Terrain->m_CellWidth - 1 || z >= g_Terrain->m_CellHeight - 1)
			return false;

		bool good = true;
		for (int k = x - 1; k <= x + 1 && good; ++k)
		{
			for (int l = z - 1; l <= z + 1 && good; ++l)
			{
				if (!g_Terrain->IsValidVillageTerrain(k, l))
					good = false;
			}
		}
		if (!good)
			return false;

		// Don't overlap existing villages' 3x3 footprints.
		for (const auto& [id, other] : m_Units)
		{
			(void)id;
			if (!other.IsAlive() || !other.IsVillage())
				continue;
			const int ox = CellX(other);
			const int oz = CellZ(other);
			if (std::abs(ox - x) <= 2 && std::abs(oz - z) <= 2)
				return false;
		}

		if (player.m_Population >= kMaxPopulation)
			return false;

		// Spend 5 parent walkers before the daughter exists.
		{
			std::vector<int> founderIds;
			for (const auto& [id, u] : m_Units)
			{
				if (u.IsAlive() && u.IsWalker() && u.m_VillageId == village.m_Id)
					founderIds.push_back(id);
			}
			std::sort(founderIds.begin(), founderIds.end());
			if (static_cast<int>(founderIds.size()) < kFoundingWalkerCost)
				return false;
			for (int i = 0; i < kFoundingWalkerCost; ++i)
				DestroyUnit(founderIds[i]);
			village.m_VillagerCount = std::max(0, village.m_VillagerCount - kFoundingWalkerCost);
		}

		LayoutVillageSmall(x, z);
		const float wx = static_cast<float>(x) + 0.5f;
		const float wz = static_cast<float>(z) + 0.5f;
		const int newId = SpawnUnit(UnitType::Village, village.m_Team, wx, wz);
		if (Unit* neu = GetUnit(newId))
		{
			neu->m_FoodBucket = 15;
			neu->m_VillageSize = 0;
			neu->m_VillagerCount = 0;
		}
		// Seed one walker so the new village isn't immediately eliminated.
		const int wid = SpawnUnit(UnitType::Walker, village.m_Team, wx + 0.5f, wz + 0.5f);
		if (Unit* w = GetUnit(wid))
		{
			w->m_VillageId = newId;
			if (Unit* neu = GetUnit(newId))
				neu->m_VillagerCount = 1;
		}

			player.m_DidExpandThisTick = true;
		return true;
	};

	for (const auto& d : kOrtho)
	{
		if (tryOffset(d[0], d[1]))
			return true;
	}
	for (const auto& d : kDiag)
	{
		if (tryOffset(d[0], d[1]))
			return true;
	}
	return false;
}

void GameSim::UpdateVillage(Unit& village)
{
	village.m_Pos.y = GroundY(village.m_Pos.x, village.m_Pos.z);

	if (village.m_Team < 0 || village.m_Team >= kMaxPlayers || !g_Terrain)
		return;

	Player& player = m_Players[village.m_Team];
	const int pop = player.m_Population;

	// Starvation: food < 0 kills a walker but never the last one (classic).
	if (village.m_FoodBucket < 0 && village.m_VillagerCount > 1)
	{
		int starveId = -1;
		for (const auto& [id, walker] : m_Units)
		{
			if (walker.IsAlive() && walker.IsWalker() && walker.m_VillageId == village.m_Id)
			{
				if (starveId < 0 || id < starveId)
					starveId = id;
			}
		}
		if (starveId >= 0)
		{
			DestroyUnit(starveId);
			if (village.m_VillageSize == 0)
				village.m_FoodBucket = 5;
			else if (village.m_VillageSize == 1)
				village.m_FoodBucket = 10;
			else
				village.m_FoodBucket = 20;
		}
	}
	else if (village.m_VillageSize == 0
		&& village.m_VillagerCount < kMaxWalkersHouse
		&& village.m_FoodBucket >= 30
		&& pop < kMaxPopulation)
	{
		const float wx = village.m_Pos.x;
		const float wz = village.m_Pos.z;
		const int wid = SpawnUnit(UnitType::Walker, village.m_Team, wx, wz);
		if (Unit* w = GetUnit(wid))
			w->m_VillageId = village.m_Id;
		++village.m_VillagerCount;
		village.m_FoodBucket = 15;
	}
	else if (village.m_VillageSize == 1
		&& village.m_VillagerCount < kMaxWalkersManor
		&& village.m_FoodBucket >= 60
		&& pop < kMaxPopulation)
	{
		const int wid = SpawnUnit(UnitType::Walker, village.m_Team, village.m_Pos.x, village.m_Pos.z);
		if (Unit* w = GetUnit(wid))
			w->m_VillageId = village.m_Id;
		++village.m_VillagerCount;
		village.m_FoodBucket = 30;
	}

	const int vx = CellX(village);
	const int vz = CellZ(village);

	// House -> manor at 5 villagers (footprint must be flat).
	if (village.m_VillageSize == 0 && village.m_VillagerCount >= kMaxWalkersHouse)
	{
		bool ready = true;
		for (int i = vx - 1; i <= vx + 1 && ready; ++i)
			for (int j = vz - 1; j <= vz + 1 && ready; ++j)
				if (!g_Terrain->IsValidVillageTerrain(i, j))
					ready = false;
		if (ready)
		{
			village.m_VillageSize = 1;
			LayoutVillageMedium(vx, vz);
		}
	}

	// Empty village dies — use a live count, not stale m_VillagerCount (which is only
	// refreshed in RecountPopulation at end-of-tick). A stale 0 would demolish the
	// house while peasants still lived; next tick they'd orphan-die and false-eliminate.
	{
		int living = 0;
		for (const auto& [wid, walker] : m_Units)
		{
			(void)wid;
			if (walker.IsAlive() && walker.IsWalker() && walker.m_VillageId == village.m_Id)
				++living;
		}
		village.m_VillagerCount = living;
		if (living <= 0)
			DestroyUnit(village.m_Id);
	}
}

void GameSim::UpdateFxUnit(Unit& unit, float tickDt)
{
	(void)tickDt;
	if (unit.m_LifetimeTicks > 0)
		--unit.m_LifetimeTicks;
	if (unit.m_LifetimeTicks <= 0)
	{
		DestroyUnit(unit.m_Id);
		return;
	}

	unit.m_Pos.y = GroundY(unit.m_Pos.x, unit.m_Pos.z);

	if (unit.m_Type == UnitType::Lightning)
	{
		// One-shot bolt: heavy damage on first ticks.
		if (unit.m_LifetimeTicks >= 6)
			DamageEnemiesInRadius(unit.m_Team, unit.m_Pos.x, unit.m_Pos.z, 1.5f, 12.0f);
	}
	else if (unit.m_Type == UnitType::Flamestrike)
	{
		// Wander and burn.
		if (!unit.m_HasTarget || unit.m_State != UnitState::Move)
		{
			float tx = unit.m_Pos.x + (g_vitalRNG ? g_vitalRNG->RandomRangeFloat(-2.0f, 2.0f) : 0.5f);
			float tz = unit.m_Pos.z + (g_vitalRNG ? g_vitalRNG->RandomRangeFloat(-2.0f, 2.0f) : 0.5f);
			ClampToMap(tx, tz);
			unit.m_Target = { tx, GroundY(tx, tz), tz };
			unit.m_HasTarget = true;
			unit.m_State = UnitState::Move;
		}
		MoveToward(unit, tickDt);
		if ((unit.m_LifetimeTicks % 5) == 0)
			DamageEnemiesInRadius(unit.m_Team, unit.m_Pos.x, unit.m_Pos.z, 1.8f, unit.m_AttackPower);
	}
	else if (unit.m_Type == UnitType::Earthquake)
	{
		// Unlevel terrain, chip enemies, clear blessed back to normal land.
		constexpr float kQuakeRadius = 5.0f;
		if (unit.m_LifetimeTicks == 44)
			DamageEnemiesInRadius(unit.m_Team, unit.m_Pos.x, unit.m_Pos.z, kQuakeRadius, unit.m_AttackPower);
		if ((unit.m_LifetimeTicks % 2) == 0)
		{
			JiggleTerrainInRadius(unit.m_Pos.x, unit.m_Pos.z, kQuakeRadius);
			ClearBlessedInRadius(unit.m_Pos.x, unit.m_Pos.z, kQuakeRadius);
			TossWalkersInRadius(unit.m_Pos.x, unit.m_Pos.z, kQuakeRadius);
		}
	}
}

void GameSim::JiggleTerrainInRadius(float x, float z, float radius)
{
	if (!g_Terrain)
		return;

	bool changed = false;
	const int r = static_cast<int>(std::ceil(radius));
	const int cx = static_cast<int>(x);
	const int cz = static_cast<int>(z);
	const float r2 = radius * radius;

	// Jiggle vertex heights (classic: +/-0.05 random). Keep at/above water so we don't flood the map.
	for (int i = cx - r; i <= cx + r + 1; ++i)
	{
		for (int j = cz - r; j <= cz + r + 1; ++j)
		{
			const float dx = static_cast<float>(i) - x;
			const float dz = static_cast<float>(j) - z;
			if (dx * dx + dz * dz > r2)
				continue;
			if (i < 0 || j < 0 || i >= g_Terrain->m_VertexWidth || j >= g_Terrain->m_VertexHeight)
				continue;

			const float offset = g_vitalRNG
				? (static_cast<float>(g_vitalRNG->Random(100)) / 1000.0f) - 0.05f
				: 0.0f;
			float h = g_Terrain->GetValue(i, j) + offset;
			h = std::clamp(h, g_Terrain->m_WaterHeight, 8.0f);
			if (i == 0 || j == 0 || i == g_Terrain->m_CellWidth || j == g_Terrain->m_CellHeight)
				h = 0.0f; // map rim stays sea-level
			g_Terrain->SetValue(i, j, h);
			changed = true;
		}
	}

	if (changed)
	{
		for (int i = cx - r; i <= cx + r; ++i)
			for (int j = cz - r; j <= cz + r; ++j)
				g_Terrain->ScrubTerrainCell(i, j);
		g_Terrain->MarkMeshDirty();
	}
}

void GameSim::ClearBlessedInRadius(float x, float z, float radius)
{
	if (!g_Terrain)
		return;
	const int minX = std::max(0, static_cast<int>(x - radius));
	const int maxX = std::min(g_Terrain->m_CellWidth - 1, static_cast<int>(x + radius));
	const int minZ = std::max(0, static_cast<int>(z - radius));
	const int maxZ = std::min(g_Terrain->m_CellHeight - 1, static_cast<int>(z + radius));
	const float r2 = radius * radius;
	bool dirty = false;
	for (int cx = minX; cx <= maxX; ++cx)
	{
		for (int cz = minZ; cz <= maxZ; ++cz)
		{
			const float dx = (static_cast<float>(cx) + 0.5f) - x;
			const float dz = (static_cast<float>(cz) + 0.5f) - z;
			if (dx * dx + dz * dz > r2)
				continue;
			if (g_Terrain->GetTerrainType(cx, cz) != TT_BLESSEDLAND)
				continue;
			g_Terrain->SetTerrainType(cx, cz, TT_GRASS);
			dirty = true;
			// If the pad is still flat, restore farmland immediately; otherwise
			// RestoreVillageFarmlandAt will run again when the player flattens.
			RestoreVillageFarmlandAt(cx, cz);
		}
	}
	if (dirty)
		g_Terrain->MarkMeshDirty();
}

void GameSim::RestoreVillageFarmlandAt(int cellX, int cellZ)
{
	if (!g_Terrain)
		return;
	if (!g_Terrain->IsValidVillageTerrain(cellX, cellZ))
		return;

	const int existing = g_Terrain->GetTerrainType(cellX, cellZ);
	// Only heal plain ground back into a farm slot — leave house/bless/ruin/swamp alone.
	if (existing != TT_GRASS && existing != TT_FLATLAND)
		return;

	for (const auto& [id, unit] : m_Units)
	{
		(void)id;
		if (!unit.IsAlive() || !unit.IsVillage())
			continue;
		const int vx = CellX(unit);
		const int vz = CellZ(unit);
		const int dx = cellX - vx;
		const int dz = cellZ - vz;
		if (dx < -1 || dx > 1 || dz < -1 || dz > 1)
			continue;
		if (dx == 0 && dz == 0)
			continue; // house tile
		if (unit.m_VillageSize == 0)
		{
			// House: only the four cardinal farm slots.
			if (std::abs(dx) + std::abs(dz) != 1)
				continue;
		}
		// Manor: any of the eight neighbors.
		g_Terrain->SetTerrainType(cellX, cellZ, TT_FARMLAND);
		g_Terrain->MarkMeshDirty();
		return;
	}
}

void GameSim::TossWalkersInRadius(float x, float z, float radius)
{
	const float r2 = radius * radius;
	std::vector<int> ids;
	for (const auto& [id, unit] : m_Units)
	{
		(void)unit;
		ids.push_back(id);
	}
	std::sort(ids.begin(), ids.end());
	for (int id : ids)
	{
		Unit* unit = GetUnit(id);
		if (!unit || !unit->IsAlive() || !unit->IsWalker())
			continue;
		if (unit->IsAirborne())
			continue;
		const float dx = unit->m_Pos.x - x;
		const float dz = unit->m_Pos.z - z;
		if (dx * dx + dz * dz > r2)
			continue;
		unit->m_VelY = g_vitalRNG
			? static_cast<float>(g_vitalRNG->Random(750)) / 1000.0f
			: 0.4f;
		unit->m_HasTarget = false;
		unit->m_State = UnitState::Idle;
	}
}

void GameSim::ApplyAirPhysics(Unit& unit, float tickDt)
{
	const float ground = GroundY(unit.m_Pos.x, unit.m_Pos.z);
	if (!unit.IsAirborne() && unit.m_Pos.y <= ground + 0.001f)
	{
		unit.m_Pos.y = ground;
		return;
	}

	constexpr float kGravity = 4.5f;
	unit.m_VelY -= kGravity * tickDt;
	unit.m_Pos.y += unit.m_VelY * tickDt * 30.0f; // scale impulse to feel like classic toss

	if (unit.m_Pos.y <= ground)
	{
		unit.m_Pos.y = ground;
		// Small bounce then settle (classic halved rebound).
		if (unit.m_VelY < -0.15f)
			unit.m_VelY = -unit.m_VelY * 0.35f;
		else
			unit.m_VelY = 0.0f;
	}
}

void GameSim::UpdateUnit(Unit& unit, float tickDt)
{
	if (unit.m_AttackCooldown > 0.0f)
		unit.m_AttackCooldown = std::max(0.0f, unit.m_AttackCooldown - tickDt);

	// Airborne units (earthquake toss) - don't snap to ground until they land.
	if (unit.IsAirborne() || unit.m_Pos.y > GroundY(unit.m_Pos.x, unit.m_Pos.z) + 0.05f)
	{
		ApplyAirPhysics(unit, tickDt);
		return;
	}

	switch (unit.m_Type)
	{
	case UnitType::Walker:
		UpdateWalker(unit, tickDt);
		break;
	case UnitType::Village:
		UpdateVillage(unit);
		break;
	case UnitType::Lightning:
	case UnitType::Flamestrike:
	case UnitType::Earthquake:
		UpdateFxUnit(unit, tickDt);
		break;
	default:
		if (unit.m_State == UnitState::Move)
			MoveToward(unit, tickDt);
		else
			unit.m_Pos.y = GroundY(unit.m_Pos.x, unit.m_Pos.z);
		break;
	}
}

void GameSim::DrawUnits(const Camera3D& camera) const
{
	WalkerSprites::EnsureLoaded();

	for (const auto& [id, unit] : m_Units)
	{
		(void)id;
		if (!unit.IsAlive())
			continue;

		Color color = (unit.m_Team >= 0 && unit.m_Team < kMaxPlayers)
			? GetPlayer(unit.m_Team).TeamColor()
			: PlayerTeamColor(unit.m_Team);

		if (unit.IsWalker() && WalkerSprites::IsReady())
		{
			WalkerSprites::DrawWalker(unit, camera, color);
			continue;
		}

		float h = 0.85f;
		float w = 0.4f;
		if (unit.IsVillage())
		{
			// House = small cube; manor = large cube.
			if (unit.m_VillageSize == 0)
			{
				h = 0.9f;
				w = 0.7f;
			}
			else
			{
				h = 1.6f;
				w = 1.2f;
			}
		}
		else if (unit.m_Type == UnitType::Lightning)
		{
			color = Color{ 255, 255, 120, 255 };
			h = 3.0f;
			w = 0.2f;
		}
		else if (unit.m_Type == UnitType::Flamestrike)
		{
			color = Color{ 255, 120, 40, 220 };
			h = 1.2f;
			w = 0.6f;
		}
		else if (unit.m_Type == UnitType::Earthquake)
		{
			// Terrain-only effect — no brown placeholder block.
			continue;
		}
		else if (unit.m_Type == UnitType::General)
		{
			// Distinct from walkers: taller + brighter.
			h = 1.6f;
			w = 0.7f;
			color = Color{
				static_cast<unsigned char>(std::min(255, color.r + 80)),
				static_cast<unsigned char>(std::min(255, color.g + 80)),
				static_cast<unsigned char>(std::min(255, color.b + 40)),
				255
			};
		}
		else if (unit.m_Type == UnitType::Archer
			|| unit.m_Type == UnitType::Warrior
			|| unit.m_Type == UnitType::Barbarian)
		{
			h = 1.15f;
			w = 0.5f;
		}

		Vector3 p = unit.m_Pos;
		p.y += h * 0.5f;
		DrawCube(p, w, h, w, color);
		DrawCubeWires(p, w, h, w, BLACK);

		DrawCube(Vector3{ unit.m_Pos.x, unit.m_Pos.y + 0.02f, unit.m_Pos.z }, w * 1.2f, 0.02f, w * 0.8f,
			Color{ 0, 0, 0, 80 });
	}
}

bool GameSim::TryFlatten(int playerSlot, int cellX, int cellZ)
{
	if (!g_Terrain || playerSlot < 0 || playerSlot >= kMaxPlayers)
		return false;
	Player& player = m_Players[playerSlot];
	if (!player.m_Active || player.m_Eliminated)
		return false;
	const float cost = ManaCostFor(PlayerAction::Flatten);
	if (player.m_Mana < cost)
		return false;
	if (g_Terrain->FlattenToward(cellX, cellZ, kFlattenTarget, 0.1f))
	{
		player.m_Mana -= cost;
		// Quake-cleared bless (now grass) in a village ring becomes farmland again when flat.
		for (int i = cellX - 1; i <= cellX + 1; ++i)
			for (int j = cellZ - 1; j <= cellZ + 1; ++j)
				RestoreVillageFarmlandAt(i, j);
		return true;
	}
	return false;
}

bool GameSim::TryRaise(int playerSlot, int cellX, int cellZ)
{
	if (!g_Terrain || playerSlot < 0 || playerSlot >= kMaxPlayers)
		return false;
	Player& player = m_Players[playerSlot];
	const float cost = ManaCostFor(PlayerAction::Raise);
	if (!player.m_Active || player.m_Mana < cost)
		return false;
	if (g_Terrain->RaiseArea(cellX, cellZ, 0.1f))
	{
		player.m_Mana -= cost;
		return true;
	}
	return false;
}

bool GameSim::TryLower(int playerSlot, int cellX, int cellZ)
{
	if (!g_Terrain || playerSlot < 0 || playerSlot >= kMaxPlayers)
		return false;
	Player& player = m_Players[playerSlot];
	const float cost = ManaCostFor(PlayerAction::Lower);
	if (!player.m_Active || player.m_Mana < cost)
		return false;
	if (g_Terrain->LowerArea(cellX, cellZ, 0.1f))
	{
		player.m_Mana -= cost;
		return true;
	}
	return false;
}

bool GameSim::CanAfford(const Player& player, PlayerAction action) const
{
	return player.m_Active && !player.m_Eliminated
		&& player.m_SpellCooldown <= 0.0f
		&& player.m_Mana >= ManaCostFor(action);
}

bool GameSim::ChargeMana(Player& player, PlayerAction action)
{
	const float cost = ManaCostFor(action);
	if (player.m_Mana < cost)
		return false;
	player.m_Mana -= cost;
	if (action != PlayerAction::Flatten && action != PlayerAction::Raise && action != PlayerAction::Lower)
		player.m_SpellCooldown = 1.5f;
	return true;
}

void GameSim::DamageEnemiesInRadius(int casterTeam, float x, float z, float radius, float damage)
{
	const float r2 = radius * radius;
	for (auto& [id, unit] : m_Units)
	{
		(void)id;
		if (!unit.IsAlive() || unit.IsFx() || unit.IsVillage())
			continue;
		if (unit.m_Team == casterTeam)
			continue;
		const float dx = unit.m_Pos.x - x;
		const float dz = unit.m_Pos.z - z;
		if (dx * dx + dz * dz > r2)
			continue;
		unit.m_HitPoints -= damage;
		if (unit.m_HitPoints <= 0.0f)
			DestroyUnit(unit.m_Id);
	}
}

void GameSim::HealFriendliesInRadius(int team, float x, float z, float radius, float amount)
{
	const float r2 = radius * radius;
	for (auto& [id, unit] : m_Units)
	{
		(void)id;
		if (!unit.IsAlive() || unit.IsFx() || unit.IsVillage())
			continue;
		if (unit.m_Team != team)
			continue;
		const float dx = unit.m_Pos.x - x;
		const float dz = unit.m_Pos.z - z;
		if (dx * dx + dz * dz > r2)
			continue;
		unit.m_HitPoints = std::min(unit.m_MaxHitPoints, unit.m_HitPoints + amount);
	}
}

void GameSim::ScatterUnitsInRadius(float x, float z, float radius, float strength)
{
	const float r2 = radius * radius;
	for (auto& [id, unit] : m_Units)
	{
		(void)id;
		if (!unit.IsAlive() || unit.IsFx() || unit.IsVillage())
			continue;
		const float dx = unit.m_Pos.x - x;
		const float dz = unit.m_Pos.z - z;
		const float d2 = dx * dx + dz * dz;
		if (d2 > r2 || d2 < 0.0001f)
			continue;
		const float inv = strength / std::sqrt(d2);
		float nx = unit.m_Pos.x + dx * inv;
		float nz = unit.m_Pos.z + dz * inv;
		ClampToMap(nx, nz);
		unit.m_Pos.x = nx;
		unit.m_Pos.z = nz;
		unit.m_Pos.y = GroundY(nx, nz);
	}
}

int GameSim::SpawnFx(UnitType type, int team, float x, float z, int lifeTicks)
{
	const int id = SpawnUnit(type, team, x, z);
	if (Unit* fx = GetUnit(id))
		fx->m_LifetimeTicks = lifeTicks;
	return id;
}

bool GameSim::TryCast(PlayerAction action, int playerSlot, int cellX, int cellZ)
{
	if (!g_Terrain || playerSlot < 0 || playerSlot >= kMaxPlayers)
		return false;
	Player& player = m_Players[playerSlot];
	if (!CanAfford(player, action))
		return false;

	const float wx = static_cast<float>(cellX) + 0.5f;
	const float wz = static_cast<float>(cellZ) + 0.5f;

	switch (action)
	{
	case PlayerAction::Flatten:
		return TryFlatten(playerSlot, cellX, cellZ);

	case PlayerAction::Raise:
		return TryRaise(playerSlot, cellX, cellZ);

	case PlayerAction::Lower:
		return TryLower(playerSlot, cellX, cellZ);

	case PlayerAction::Bless:
	{
		bool changed = false;
		for (int i = cellX - 1; i <= cellX + 1; ++i)
		{
			for (int j = cellZ - 1; j <= cellZ + 1; ++j)
			{
				if (g_vitalRNG && g_vitalRNG->Random(100) < 50)
					continue;
				if (g_Terrain->IsValidVillageTerrain(i, j) && g_Terrain->GetTerrainType(i, j) != TT_HOUSE)
				{
					g_Terrain->SetTerrainType(i, j, TT_BLESSEDLAND);
					changed = true;
				}
			}
		}
		if (!changed)
			return false;
		return ChargeMana(player, action);
	}

	case PlayerAction::StoneRain:
	{
		bool changed = false;
		for (int i = cellX - 1; i <= cellX + 1; ++i)
		{
			for (int j = cellZ - 1; j <= cellZ + 1; ++j)
			{
				if (g_vitalRNG && g_vitalRNG->Random(100) < 50)
					continue;
				if (g_Terrain->IsValidVillageTerrain(i, j) && g_Terrain->GetTerrainType(i, j) != TT_HOUSE)
				{
					g_Terrain->SetTerrainType(i, j, TT_RUINEDLAND);
					changed = true;
				}
			}
		}
		if (!changed)
			return false;
		return ChargeMana(player, action);
	}

	case PlayerAction::Swamp:
	{
		bool changed = false;
		for (int i = cellX - 2; i <= cellX + 2; ++i)
		{
			for (int j = cellZ - 2; j <= cellZ + 2; ++j)
			{
				if (g_vitalRNG && g_vitalRNG->Random(100) < 50)
					continue;
				if (g_Terrain->IsValidVillageTerrain(i, j) && g_Terrain->GetTerrainType(i, j) != TT_HOUSE)
				{
					g_Terrain->SetTerrainType(i, j, TT_SWAMP);
					changed = true;
				}
			}
		}
		if (!changed)
			return false;
		return ChargeMana(player, action);
	}

	case PlayerAction::Lightning:
		if (!ChargeMana(player, action))
			return false;
		SpawnFx(UnitType::Lightning, playerSlot, wx, wz, 8);
		return true;

	case PlayerAction::Flamestrike:
		if (!ChargeMana(player, action))
			return false;
		SpawnFx(UnitType::Flamestrike, playerSlot, wx, wz, 90);
		return true;

	case PlayerAction::Earthquake:
		if (!ChargeMana(player, action))
			return false;
		SpawnFx(UnitType::Earthquake, playerSlot, wx, wz, 45);
		return true;

	case PlayerAction::HealingLight:
		if (!ChargeMana(player, action))
			return false;
		HealFriendliesInRadius(playerSlot, wx, wz, 3.5f, 8.0f);
		return true;

	case PlayerAction::MoveGeneral:
		return TryMoveGeneral(playerSlot, cellX, cellZ);

	default:
		return false;
	}
}

bool GameSim::ConvertWalkerToMilitary(UnitType type, int playerSlot)
{
	Player& player = m_Players[playerSlot];

	// Must have a village with a spare walker, and general near a village (or no general yet).
	bool generalClose = (player.m_GeneralId < 0);
	Unit* general = (player.m_GeneralId >= 0) ? GetUnit(player.m_GeneralId) : nullptr;
	if (general && !general->IsAlive())
	{
		player.m_GeneralId = -1;
		general = nullptr;
		generalClose = true;
	}

	if (general)
	{
		for (int villageId : player.m_VillageIds)
		{
			Unit* village = GetUnit(villageId);
			if (!village || !village->IsAlive())
				continue;
			const float dx = village->m_Pos.x - general->m_Pos.x;
			const float dz = village->m_Pos.z - general->m_Pos.z;
			if (dx * dx + dz * dz <= 9.0f)
			{
				generalClose = true;
				break;
			}
		}
	}
	if (!generalClose)
		return false;

	// Count living walkers per village (don't trust stale m_VillagerCount).
	int donorVillageId = -1;
	int walkerId = -1;
	float villageX = 0.0f;
	float villageZ = 0.0f;
	for (int villageId : player.m_VillageIds)
	{
		Unit* village = GetUnit(villageId);
		if (!village || !village->IsAlive())
			continue;
		int living = 0;
		int candidateWalker = -1;
		for (const auto& [id, unit] : m_Units)
		{
			if (unit.IsAlive() && unit.IsWalker() && unit.m_VillageId == village->m_Id)
			{
				++living;
				if (candidateWalker < 0 || id < candidateWalker)
					candidateWalker = id;
			}
		}
		if (living > 1 && candidateWalker >= 0)
		{
			donorVillageId = villageId;
			walkerId = candidateWalker;
			villageX = village->m_Pos.x;
			villageZ = village->m_Pos.z;
			break;
		}
	}
	if (donorVillageId < 0 || walkerId < 0)
		return false;

	DestroyUnit(walkerId);

	// Spawn beside the village on the flat pad (not stacked inside the house cube).
	float generalX = villageX + 1.5f;
	float generalZ = villageZ;
	ClampToMap(generalX, generalZ);

	if (!general)
	{
		const int gid = SpawnUnit(UnitType::General, playerSlot, generalX, generalZ);
		player.m_GeneralId = gid;
		general = GetUnit(gid);
	}

	float troopX = villageX + 1.5f;
	float troopZ = villageZ + 1.5f;
	if (general)
	{
		troopX = general->m_Pos.x + 0.75f;
		troopZ = general->m_Pos.z + 0.75f;
	}
	ClampToMap(troopX, troopZ);

	const int mid = SpawnUnit(type, playerSlot, troopX, troopZ);
	if (Unit* mil = GetUnit(mid))
	{
		mil->m_MasterId = player.m_GeneralId;
		mil->m_VillageId = -1;
		mil->m_Pos.y = GroundY(troopX, troopZ);
	}
	if (Unit* g = GetUnit(player.m_GeneralId))
		g->m_Pos.y = GroundY(g->m_Pos.x, g->m_Pos.z);

	RefreshGeneralSpeed(playerSlot);

	Log(std::string("Created ") + UnitTypeName(type)
		+ " at " + std::to_string(troopX) + "," + std::to_string(troopZ)
		+ " team=" + std::to_string(playerSlot));
	return true;
}

void GameSim::RefreshGeneralSpeed(int playerSlot)
{
	if (playerSlot < 0 || playerSlot >= kMaxPlayers)
		return;
	Player& player = m_Players[playerSlot];
	Unit* general = (player.m_GeneralId >= 0) ? GetUnit(player.m_GeneralId) : nullptr;
	if (!general || !general->IsAlive())
		return;

	// Solo general keeps base pace; with troops, match the slowest living army unit.
	constexpr float kGeneralBaseSpeed = 1.7f;
	float slowest = kGeneralBaseSpeed;
	bool hasTroops = false;
	for (int armyId : player.m_ArmyIds)
	{
		Unit* u = GetUnit(armyId);
		if (!u || !u->IsAlive() || u->m_Type == UnitType::General)
			continue;
		hasTroops = true;
		slowest = std::min(slowest, u->m_Speed);
	}
	general->m_Speed = hasTroops ? slowest : kGeneralBaseSpeed;
}

bool GameSim::TryCreateMilitary(UnitType type, int playerSlot)
{
	if (playerSlot < 0 || playerSlot >= kMaxPlayers)
		return false;
	Player& player = m_Players[playerSlot];
	if (!player.m_Active || player.m_Eliminated)
		return false;
	if (type != UnitType::Archer && type != UnitType::Warrior && type != UnitType::Barbarian)
		return false;
	return ConvertWalkerToMilitary(type, playerSlot);
}

bool GameSim::TryMoveGeneral(int playerSlot, int cellX, int cellZ)
{
	if (playerSlot < 0 || playerSlot >= kMaxPlayers)
		return false;
	Player& player = m_Players[playerSlot];
	if (!player.m_Active || player.m_Eliminated || player.m_GeneralId < 0)
		return false;
	Unit* general = GetUnit(player.m_GeneralId);
	if (!general || !general->IsAlive())
		return false;

	RefreshGeneralSpeed(playerSlot);

	float x = static_cast<float>(cellX) + 0.5f;
	float z = static_cast<float>(cellZ) + 0.5f;
	ClampToMap(x, z);
	general->m_Target = { x, GroundY(x, z), z };
	general->m_HasTarget = true;
	general->m_State = UnitState::Move;

	// Army follows the general (sorted for deterministic RNG offsets).
	std::vector<int> armyIds = player.m_ArmyIds;
	std::sort(armyIds.begin(), armyIds.end());
	for (int armyId : armyIds)
	{
		Unit* u = GetUnit(armyId);
		if (!u || !u->IsAlive() || u->m_Type == UnitType::General)
			continue;
		float ox = x + (g_vitalRNG ? g_vitalRNG->RandomRangeFloat(-1.0f, 1.0f) : 0.0f);
		float oz = z + (g_vitalRNG ? g_vitalRNG->RandomRangeFloat(-1.0f, 1.0f) : 0.0f);
		ClampToMap(ox, oz);
		u->m_Target = { ox, GroundY(ox, oz), oz };
		u->m_HasTarget = true;
		u->m_State = UnitState::Move;
	}
	return true;
}

void GameSim::ApplyPlayerCommand(PlayerAction action, int playerSlot, int cellX, int cellZ, uint16_t extra)
{
	(void)extra;
	switch (action)
	{
	case PlayerAction::Flatten:
		TryFlatten(playerSlot, cellX, cellZ);
		break;
	case PlayerAction::Raise:
		TryRaise(playerSlot, cellX, cellZ);
		break;
	case PlayerAction::Lower:
		TryLower(playerSlot, cellX, cellZ);
		break;
	case PlayerAction::CreateArcher:
		TryCreateMilitary(UnitType::Archer, playerSlot);
		break;
	case PlayerAction::CreateWarrior:
		TryCreateMilitary(UnitType::Warrior, playerSlot);
		break;
	case PlayerAction::CreateBarbarian:
		TryCreateMilitary(UnitType::Barbarian, playerSlot);
		break;
	case PlayerAction::MoveGeneral:
		TryMoveGeneral(playerSlot, cellX, cellZ);
		break;
	default:
		TryCast(action, playerSlot, cellX, cellZ);
		break;
	}
}
