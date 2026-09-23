#ifndef _PLANITIA_GODPOWERCURSOR_H_
#define _PLANITIA_GODPOWERCURSOR_H_

#include "GameTypes.h"
#include "raylib.h"

// 3D terrain cursor when a god power is selected:
//  - point spells: small draped TerrainHighlightRing at the hit
//  - AOE spells: particle.png icons orbiting the radius (Populous-style)
namespace GodPowerCursor
{
	void Init();
	void Shutdown();

	// Draw inside BeginMode3D at the continuous ray-hit point (smooth follow).
	// Casting should use the same hit, truncated to cells.
	bool Draw(const Camera3D& camera, PlayerAction power, Vector3 terrainHit, bool validHit);
}

#endif
