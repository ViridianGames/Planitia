#include "Harness/SettleAgent.h"

#include "GameGlobals.h"
#include "GameSim.h"
#include "Terrain.h"
#include "Unit.h"
#include "json.hpp"

#include <cmath>
#include <cstdlib>

namespace
{
	constexpr float kPadHeight = 2.0f;

	constexpr int kOrtho[4][2] = {
		{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
	};
	constexpr int kDiag[4][2] = {
		{ 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 },
	};

	bool InMap(int x, int z)
	{
		return g_Terrain
			&& x >= 0 && z >= 0
			&& x < g_Terrain->m_CellWidth
			&& z < g_Terrain->m_CellHeight;
	}

	int UnitCellX(const Unit& unit)
	{
		return static_cast<int>(unit.m_Pos.x);
	}

	int UnitCellZ(const Unit& unit)
	{
		return static_cast<int>(unit.m_Pos.z);
	}

	bool PadInMap(int x, int z)
	{
		return InMap(x - 1, z - 1) && InMap(x + 1, z + 1);
	}

	float Deviation(int x, int z)
	{
		float sum = 0.0f;
		sum += std::fabs(g_Terrain->GetValue(x, z) - kPadHeight);
		sum += std::fabs(g_Terrain->GetValue(x + 1, z) - kPadHeight);
		sum += std::fabs(g_Terrain->GetValue(x, z + 1) - kPadHeight);
		sum += std::fabs(g_Terrain->GetValue(x + 1, z + 1) - kPadHeight);
		return sum;
	}

	bool VillageOccupies(const GameSim& sim, int x, int z)
	{
		for (const auto& [id, unit] : sim.Units())
		{
			(void)id;
			if (!unit.IsAlive() || !unit.IsVillage())
				continue;
			if (std::abs(UnitCellX(unit) - x) <= 2 && std::abs(UnitCellZ(unit) - z) <= 2)
				return true;
		}
		return false;
	}

	bool WorstInvalid(int cx, int cz, int& outX, int& outZ)
	{
		bool any = false;
		float worst = -1.0f;
		for (int z = cz - 1; z <= cz + 1; ++z)
		{
			for (int x = cx - 1; x <= cx + 1; ++x)
			{
				if (!InMap(x, z) || g_Terrain->IsValidVillageTerrain(x, z))
					continue;
				const float dev = Deviation(x, z);
				if (!any || dev > worst || (dev == worst && (z < outZ || (z == outZ && x < outX))))
				{
					any = true;
					worst = dev;
					outX = x;
					outZ = z;
				}
			}
		}
		return any;
	}

	void CollectInvalid(int cx, int cz, std::vector<std::pair<int, int>>& out)
	{
		for (int z = cz - 1; z <= cz + 1; ++z)
		{
			for (int x = cx - 1; x <= cx + 1; ++x)
			{
				if (InMap(x, z) && !g_Terrain->IsValidVillageTerrain(x, z))
					out.emplace_back(x, z);
			}
		}
	}

	int CountValid(int cx, int cz)
	{
		int n = 0;
		for (int z = cz - 1; z <= cz + 1; ++z)
		{
			for (int x = cx - 1; x <= cx + 1; ++x)
			{
				if (InMap(x, z) && g_Terrain->IsValidVillageTerrain(x, z))
					++n;
			}
		}
		return n;
	}

	// Home footprint first, then the next unready daughter pad of the oldest village.
	bool FindWorkPad(const GameSim& sim, int slot, int& cx, int& cz)
	{
		const Player& player = sim.GetPlayer(slot);
		const Unit* home = nullptr;
		for (int id : player.m_VillageIds)
		{
			const Unit* village = sim.GetUnit(id);
			if (!village || !village->IsAlive() || !village->IsVillage())
				continue;
			if (!home)
				home = village;
			const int vx = UnitCellX(*village);
			const int vz = UnitCellZ(*village);
			int ix = 0, iz = 0;
			if (WorstInvalid(vx, vz, ix, iz))
			{
				cx = vx;
				cz = vz;
				return true;
			}
		}
		if (!home)
			return false;

		const int vx = UnitCellX(*home);
		const int vz = UnitCellZ(*home);
		auto consider = [&](int di, int dj) -> bool
		{
			const int x = vx + di * 3;
			const int z = vz + dj * 3;
			if (!PadInMap(x, z) || VillageOccupies(sim, x, z))
				return false;
			int ix = 0, iz = 0;
			if (!WorstInvalid(x, z, ix, iz))
				return false;
			cx = x;
			cz = z;
			return true;
		};
		for (const auto& d : kOrtho)
		{
			if (consider(d[0], d[1]))
				return true;
		}
		for (const auto& d : kDiag)
		{
			if (consider(d[0], d[1]))
				return true;
		}
		return false;
	}

	void CollectFarms(const GameSim& sim, int slot, std::vector<std::pair<int, int>>& farms, int& blessed)
	{
		blessed = 0;
		const Player& player = sim.GetPlayer(slot);
		for (int id : player.m_VillageIds)
		{
			const Unit* village = sim.GetUnit(id);
			if (!village || !village->IsAlive() || !village->IsVillage())
				continue;
			const int vx = UnitCellX(*village);
			const int vz = UnitCellZ(*village);
			const int reach = (village->m_VillageSize == 0) ? 1 : 1;
			(void)reach;
			for (int dz = -1; dz <= 1; ++dz)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					if (dx == 0 && dz == 0)
						continue;
					if (village->m_VillageSize == 0 && std::abs(dx) + std::abs(dz) != 1)
						continue;
					const int x = vx + dx;
					const int z = vz + dz;
					if (!InMap(x, z))
						continue;
					const int type = g_Terrain->GetTerrainType(x, z);
					if (type == TT_BLESSEDLAND)
						++blessed;
					else if (type == TT_FARMLAND)
						farms.emplace_back(x, z);
				}
			}
		}
	}
}

const char* SeatActionWireName(PlayerAction action)
{
	switch (action)
	{
	case PlayerAction::Flatten: return "flatten";
	case PlayerAction::Raise: return "raise";
	case PlayerAction::Lower: return "lower";
	case PlayerAction::Bless: return "bless";
	case PlayerAction::Swamp: return "swamp";
	case PlayerAction::StoneRain: return "stone-rain";
	case PlayerAction::Lightning: return "lightning";
	case PlayerAction::Flamestrike: return "flamestrike";
	case PlayerAction::Earthquake: return "earthquake";
	case PlayerAction::HealingLight: return "healing";
	case PlayerAction::CreateArcher: return "create-archer";
	case PlayerAction::CreateBarbarian: return "create-barbarian";
	case PlayerAction::CreateWarrior: return "create-warrior";
	case PlayerAction::MoveGeneral: return "move-general";
	default: return "none";
	}
}

bool SeatActionFromWireName(const std::string& name, PlayerAction& out)
{
	for (int i = 0; i < static_cast<int>(PlayerAction::Count); ++i)
	{
		const PlayerAction action = static_cast<PlayerAction>(i);
		if (name == SeatActionWireName(action))
		{
			out = action;
			return true;
		}
	}
	return false;
}

bool ParseSeatCommandLine(const std::string& line, SeatCommand& out, std::string& error)
{
	out = {};
	error.clear();
	if (line.empty())
	{
		error = "empty reply";
		return false;
	}

	nlohmann::json doc;
	try
	{
		doc = nlohmann::json::parse(line);
	}
	catch (const nlohmann::json::exception& ex)
	{
		error = ex.what();
		return false;
	}

	if (!doc.is_object() || !doc.contains("action") || !doc["action"].is_string())
	{
		error = "reply needs an action string";
		return false;
	}

	const std::string name = doc["action"].get<std::string>();
	if (name == "none")
		return true;

	PlayerAction action = PlayerAction::Flatten;
	if (!SeatActionFromWireName(name, action))
	{
		error = "unknown action " + name;
		return false;
	}

	const bool needsCell = action != PlayerAction::CreateArcher
		&& action != PlayerAction::CreateBarbarian
		&& action != PlayerAction::CreateWarrior;
	int x = 0;
	int z = 0;
	if (needsCell)
	{
		if (!doc.contains("x") || !doc.contains("z") || !doc["x"].is_number() || !doc["z"].is_number())
		{
			error = "reply needs numeric x and z";
			return false;
		}
		x = doc["x"].get<int>();
		z = doc["z"].get<int>();
		if (!InMap(x, z))
		{
			error = "cell is off the map";
			return false;
		}
	}

	out.pass = false;
	out.action = action;
	out.x = x;
	out.z = z;
	return true;
}

SeatInspect InspectSeat(const GameSim& sim, int slot)
{
	SeatInspect view;
	if (slot < 0 || slot >= kMaxPlayers || !g_Terrain)
		return view;

	const Player& player = sim.GetPlayer(slot);
	view.population = player.m_Population;
	view.mana = player.m_Mana;
	view.cooldown = player.m_SpellCooldown;
	view.eliminated = player.m_Eliminated;

	for (int id : player.m_VillageIds)
	{
		const Unit* village = sim.GetUnit(id);
		if (!village || !village->IsAlive() || !village->IsVillage())
			continue;
		SeatVillageInfo info;
		info.x = UnitCellX(*village);
		info.z = UnitCellZ(*village);
		info.size = village->m_VillageSize;
		info.food = village->m_FoodBucket;
		info.villagers = village->m_VillagerCount;
		view.food += info.food;
		view.villages.push_back(info);
	}
	view.villageCount = static_cast<int>(view.villages.size());

	int padX = 0;
	int padZ = 0;
	const bool havePad = FindWorkPad(sim, slot, padX, padZ);
	if (havePad)
	{
		CollectInvalid(padX, padZ, view.flatten);
		view.prepared = CountValid(padX, padZ);
	}
	else
	{
		view.prepared = 9;
	}

	CollectFarms(sim, slot, view.bless, view.blessed);

	int flatX = 0;
	int flatZ = 0;
	if (havePad && WorstInvalid(padX, padZ, flatX, flatZ)
		&& player.m_Mana >= ManaCostFor(PlayerAction::Flatten))
	{
		view.suggestion.pass = false;
		view.suggestion.action = PlayerAction::Flatten;
		view.suggestion.x = flatX;
		view.suggestion.z = flatZ;
		return view;
	}

	if (!havePad && !view.bless.empty() && sim.CanAfford(player, PlayerAction::Bless))
	{
		int bestX = view.bless[0].first;
		int bestZ = view.bless[0].second;
		for (const auto& cell : view.bless)
		{
			if (cell.second < bestZ || (cell.second == bestZ && cell.first < bestX))
			{
				bestX = cell.first;
				bestZ = cell.second;
			}
		}
		view.suggestion.pass = false;
		view.suggestion.action = PlayerAction::Bless;
		view.suggestion.x = bestX;
		view.suggestion.z = bestZ;
	}
	return view;
}
