///////////////////////////////////////////////////////////////////////////
//
// Name:     TERRAIN.H
// Purpose:  Heightfield terrain adapted from classic Planitia (Populous-style
//           hemisphere hills). 4-tri flat-shaded tiles; dirty-cell mesh patches.
//
///////////////////////////////////////////////////////////////////////////

#ifndef _PLANITIA_TERRAIN_H_
#define _PLANITIA_TERRAIN_H_

#include "Geist/RNG.h"
#include "raylib.h"

#include <string>
#include <vector>

enum TerrainType
{
	TT_SAND = 0,
	TT_BEACH,
	TT_GRASS,
	TT_BLESSEDLAND,
	TT_RUINEDLAND,
	TT_LAVA,
	TT_WATER,
	TT_FARMLAND,
	TT_HOUSE,
	TT_FLATLAND,
	TT_SWAMP,

	TT_LASTTERRAINTYPE
};

struct TerrainCell
{
	int m_TerrainType = TT_GRASS;
	unsigned int m_AdditionalData = 0;
};

enum class TerrainDrawMode
{
	Textured = 0,
	Colors,     // vertex-colored terrain types (no diffuse texture)
	Wireframe,  // grid lines
	Count
};

class Terrain
{
public:
	static constexpr int kVertsPerCell = 12; // 4 tris x 3 unshared verts

	Terrain() = default;
	~Terrain();

	// configfile: optional path relative to Redist (e.g. "Data/Maps/MainMenuTerrain.txt")
	void Init(const std::string& configfile = "");
	void Shutdown();
	void Update();
	void Draw();

	void CycleDrawMode();
	TerrainDrawMode GetDrawMode() const { return m_DrawMode; }
	const char* GetDrawModeName() const;

	void InitializeMap(unsigned int seed);
	void RebuildMesh(); // full rebuild (map gen / draw-mode change)

	void AddHill();
	void AddHill(float x, float z, float radius);

	float GetValue(int x, int z) const;
	void SetValue(int x, int z, float value);
	void OffsetValue(int x, int z, float value);
	float GetMiddle(int x, int z) const;
	float GetHeight(float x, float z) const; // sample across 4 center-fan triangles
	float GetMaxHeight() const;

	// Step along a ray and find the heightfield crossing (LittleWars combat pick).
	bool Raycast(const Ray& ray, Vector3& outHit) const;

	int GetTerrainType(int x, int z) const;
	void SetTerrainType(int x, int z, int type);
	void AssignTerrainTypesFromHeights();
	int FindTerrainType(int cellX, int cellZ) const;
	void ScrubTerrainCell(int cellX, int cellZ);

	// God-power style terrain edits (step toward target; returns true if changed).
	bool FlattenToward(int cellX, int cellZ, float targetHeight, float step = 0.1f);
	bool RaiseArea(int cellX, int cellZ, float step = 0.1f);
	bool LowerArea(int cellX, int cellZ, float step = 0.1f);
	void MarkMeshDirty(); // full rebuild next Update

	bool IsWaterCell(int x, int z) const;
	bool ShouldFlipDiagonal(int cellX, int cellZ) const;
	// Flat pad above water - required for houses / village expansion (classic Planitia).
	bool IsValidVillageTerrain(int x, int z) const;

	int m_CellWidth = 64;
	int m_CellHeight = 64;
	int m_VertexWidth = 65;
	int m_VertexHeight = 65;

	unsigned int m_Seed = 7777;
	float m_WaterHeight = 0.05f;
	float m_MaxHeight = 0.0f;
	TerrainDrawMode m_DrawMode = TerrainDrawMode::Textured;

private:
	static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

	void LoadConfig(const std::string& configfile);
	void LoadTextures();
	void BuildAtlas();
	void UnloadDrawMeshes();
	Color BaseColorForType(int terrainType) const;
	Texture2D TextureForType(int terrainType) const;
	void AtlasUvRect(int terrainType, float& u0, float& v0, float& u1, float& v1) const;

	void MarkCellDirty(int cellX, int cellZ);
	void MarkCellsTouchingVertex(int vx, int vz);
	void FlushDirtyCells();
	void FillCellVertices(int cellX, int cellZ, struct Vertex* out12) const;
	void WriteCellToMesh(int cellX, int cellZ);
	void RecalculateMaxHeight();

	std::vector<float> m_Values;
	std::vector<TerrainCell> m_Cells;

	// Diffuse maps per terrain type (ResourceManager-backed; not owned).
	Texture2D m_TerrainTextures[TT_LASTTERRAINTYPE]{};
	Texture2D m_WhiteTexture{};
	Texture2D m_AtlasTexture{}; // packed albedos for single-draw incremental mesh
	bool m_AtlasOwned = false;
	bool m_TexturesOwned = false;

	// Fixed layout: cell (x,z) owns verts [index*12, index*12+12).
	Model m_TerrainModel{};
	bool m_TerrainModelLoaded = false;
	bool m_FullRebuild = false;
	std::vector<uint8_t> m_CellDirtyFlag;
	std::vector<int> m_DirtyCells;
};

#endif
