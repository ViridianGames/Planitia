#include "MatchChecksum.h"

#include "GameGlobals.h"
#include "GameSim.h"
#include "Unit.h"

#include <algorithm>
#include <vector>

uint32_t PlanitiaMatchChecksum(const GameSim& sim)
{
	uint32_t h = 2166136261u;
	auto mix = [&](uint32_t v)
	{
		h ^= v;
		h *= 16777619u;
	};

	mix(sim.Tick());
	mix(static_cast<uint32_t>(sim.Units().size()));

	std::vector<int> ids;
	ids.reserve(sim.Units().size());
	for (const auto& [id, unit] : sim.Units())
	{
		(void)unit;
		ids.push_back(id);
	}
	std::sort(ids.begin(), ids.end());
	for (int id : ids)
	{
		const Unit* u = sim.GetUnit(id);
		if (!u)
			continue;
		mix(static_cast<uint32_t>(id));
		mix(static_cast<uint32_t>(u->m_Type));
		mix(static_cast<uint32_t>(u->m_Team));
		union { float f; uint32_t u32; } x{ u->m_Pos.x }, z{ u->m_Pos.z }, hp{ u->m_HitPoints };
		mix(x.u32);
		mix(z.u32);
		mix(hp.u32);
	}

	if (g_Terrain)
	{
		// Sparse height samples for a cheap terrain fingerprint.
		for (int z = 0; z < g_Terrain->m_CellHeight; z += 8)
		{
			for (int x = 0; x < g_Terrain->m_CellWidth; x += 8)
			{
				union { float f; uint32_t u32; } v{ g_Terrain->GetValue(x, z) };
				mix(v.u32);
			}
		}
	}

	if (g_vitalRNG)
	{
		unsigned int seed = 0, index = 0;
		g_vitalRNG->GetRNGState(seed, index);
		mix(seed);
		mix(index);
		mix(g_vitalRNG->GetOriginalSeed());
	}
	return h;
}
