///////////////////////////////////////////////////////////////////////////
//
// Name:     TERRAIN.CPP
// Purpose:  Adapted Planitia heightfield - hemisphere hills, 4-tri flat-shaded
//           tiles (early-90s software look), raylib mesh draw.
//
///////////////////////////////////////////////////////////////////////////

#include "Terrain.h"

#include "Geist/Config.h"
#include "Geist/Globals.h"
#include "Geist/Logging.h"
#include "Geist/Primitives.h"
#include "Geist/ResourceManager.h"
#include "GameGlobals.h"

#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
	constexpr float kAmbientLight = 128.0f;
	constexpr float kDirectLightScale = 127.0f;
	const Vector3 kSunDirection = Vector3Normalize(Vector3{ 1.0f, 1.0f, 1.0f });

	int ComputeLightFromNormal(Vector3 normal)
	{
		normal = Vector3Normalize(normal);
		float cosine = Vector3DotProduct(normal, kSunDirection);
		if (cosine < 0.0f)
			cosine = 0.0f;
		const int light = static_cast<int>(kAmbientLight + cosine * kDirectLightScale);
		return std::min(light, 255);
	}

	Mesh BuildMeshFromTriangleSoup(const std::vector<Vertex>& vertices)
	{
		Mesh mesh{};
		const int vertexCount = static_cast<int>(vertices.size());
		mesh.vertexCount = vertexCount;
		mesh.triangleCount = vertexCount / 3;
		if (vertexCount <= 0)
			return mesh;

		mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned>(vertexCount * 3 * sizeof(float))));
		mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned>(vertexCount * 2 * sizeof(float))));
		mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned>(vertexCount * 4)));

		for (int i = 0; i < vertexCount; ++i)
		{
			const Vertex& v = vertices[static_cast<size_t>(i)];
			mesh.vertices[i * 3 + 0] = v.x;
			mesh.vertices[i * 3 + 1] = v.y;
			mesh.vertices[i * 3 + 2] = v.z;
			mesh.texcoords[i * 2 + 0] = v.u;
			mesh.texcoords[i * 2 + 1] = v.v;
			mesh.colors[i * 4 + 0] = static_cast<unsigned char>(std::clamp(v.r, 0.0f, 1.0f) * 255.0f);
			mesh.colors[i * 4 + 1] = static_cast<unsigned char>(std::clamp(v.g, 0.0f, 1.0f) * 255.0f);
			mesh.colors[i * 4 + 2] = static_cast<unsigned char>(std::clamp(v.b, 0.0f, 1.0f) * 255.0f);
			mesh.colors[i * 4 + 3] = static_cast<unsigned char>(std::clamp(v.a, 0.0f, 1.0f) * 255.0f);
		}

		UploadMesh(&mesh, false);
		return mesh;
	}

	Vertex MakeShadedVertex(float x, float y, float z, Color base, int light, float u, float v, float a = 1.0f)
	{
		const float shade = static_cast<float>(light) / 255.0f;
		return CreateVertex(
			x, y, z,
			(base.r / 255.0f) * shade,
			(base.g / 255.0f) * shade,
			(base.b / 255.0f) * shade,
			a,
			u, v);
	}

	// Early-90s software look: each triangle gets one flat shade from its face normal.
	void EmitFlatTriangle(
		Vertex* out,
		Vector3 v0, Vector3 v1, Vector3 v2,
		Color base,
		float u0, float v0t,
		float u1, float v1t,
		float u2, float v2t,
		float alpha = 1.0f)
	{
		const Vector3 e1 = Vector3Subtract(v1, v0);
		const Vector3 e2 = Vector3Subtract(v2, v0);
		const Vector3 normal = Vector3CrossProduct(e2, e1);
		const int light = ComputeLightFromNormal(normal);

		out[0] = MakeShadedVertex(v0.x, v0.y, v0.z, base, light, u0, v0t, alpha);
		out[1] = MakeShadedVertex(v1.x, v1.y, v1.z, base, light, u1, v1t, alpha);
		out[2] = MakeShadedVertex(v2.x, v2.y, v2.z, base, light, u2, v2t, alpha);
	}

	constexpr int kAtlasTile = 256;
	constexpr int kAtlasCols = 4;

	bool BarycentricXZ(
		float px, float pz,
		float ax, float az, float bx, float bz, float cx, float cz,
		float& u, float& v, float& w)
	{
		const float v0x = bx - ax;
		const float v0z = bz - az;
		const float v1x = cx - ax;
		const float v1z = cz - az;
		const float v2x = px - ax;
		const float v2z = pz - az;
		const float den = v0x * v1z - v1x * v0z;
		if (std::fabs(den) < 1e-8f)
			return false;
		v = (v2x * v1z - v1x * v2z) / den;
		w = (v0x * v2z - v2x * v0z) / den;
		u = 1.0f - v - w;
		constexpr float kEps = -1e-4f;
		return u >= kEps && v >= kEps && w >= kEps;
	}
}

Terrain::~Terrain()
{
	Shutdown();
}

void Terrain::Init(const std::string& configfile)
{
	Shutdown();
	LoadConfig(configfile);

	m_VertexWidth = m_CellWidth + 1;
	m_VertexHeight = m_CellHeight + 1;
	m_Values.assign(static_cast<size_t>(m_VertexWidth * m_VertexHeight), 0.0f);
	m_Cells.assign(static_cast<size_t>(m_CellWidth * m_CellHeight), TerrainCell{});

	m_CellDirtyFlag.assign(static_cast<size_t>(m_CellWidth * m_CellHeight), 0);
	m_DirtyCells.clear();

	LoadTextures();
	InitializeMap(m_Seed);
	RebuildMesh();
}

void Terrain::Shutdown()
{
	UnloadDrawMeshes();
	if (m_WhiteTexture.id > 0)
	{
		UnloadTexture(m_WhiteTexture);
		m_WhiteTexture = {};
	}
	if (m_AtlasOwned && m_AtlasTexture.id > 0)
	{
		UnloadTexture(m_AtlasTexture);
		m_AtlasTexture = {};
		m_AtlasOwned = false;
	}
	if (m_TexturesOwned)
	{
		for (int i = 0; i < TT_LASTTERRAINTYPE; ++i)
		{
			if (m_TerrainTextures[i].id == 0)
				continue;
			bool already = false;
			for (int j = 0; j < i; ++j)
			{
				if (m_TerrainTextures[j].id == m_TerrainTextures[i].id)
				{
					already = true;
					break;
				}
			}
			if (!already)
				UnloadTexture(m_TerrainTextures[i]);
			m_TerrainTextures[i] = {};
		}
		m_TexturesOwned = false;
	}
	else
	{
		for (int i = 0; i < TT_LASTTERRAINTYPE; ++i)
			m_TerrainTextures[i] = {};
	}
	m_Values.clear();
	m_Cells.clear();
	m_CellDirtyFlag.clear();
	m_DirtyCells.clear();
}

void Terrain::CycleDrawMode()
{
	const int next = (static_cast<int>(m_DrawMode) + 1) % static_cast<int>(TerrainDrawMode::Count);
	m_DrawMode = static_cast<TerrainDrawMode>(next);
	// Textured vs Colors changes vertex albedo - full rebuild.
	MarkMeshDirty();
}

const char* Terrain::GetDrawModeName() const
{
	switch (m_DrawMode)
	{
	case TerrainDrawMode::Textured:  return "Textured";
	case TerrainDrawMode::Colors:    return "Colors";
	case TerrainDrawMode::Wireframe: return "Wireframe";
	default:                         return "Unknown";
	}
}

void Terrain::LoadConfig(const std::string& configfile)
{
	m_CellWidth = 64;
	m_CellHeight = 64;
	m_Seed = 7777;

	if (configfile.empty())
		return;

	Config cfg;
	if (cfg.Load(configfile))
	{
		const int w = static_cast<int>(cfg.GetNumber("width"));
		const int h = static_cast<int>(cfg.GetNumber("height"));
		if (w > 0) m_CellWidth = w;
		if (h > 0) m_CellHeight = h;
		const int seed = static_cast<int>(cfg.GetNumber("seed"));
		if (seed != 0) m_Seed = static_cast<unsigned int>(seed);
	}
}

void Terrain::LoadTextures()
{
	auto loadTex = [this](const char* path) -> Texture2D
	{
		if (g_ResourceManager)
		{
			Texture* t = g_ResourceManager->GetTexture(path);
			if (t && t->id > 0)
				return *t;
		}
		Texture2D loaded = LoadTexture(path);
		if (loaded.id > 0)
			m_TexturesOwned = true;
		return loaded;
	};

	for (int i = 0; i < TT_LASTTERRAINTYPE; ++i)
		m_TerrainTextures[i] = {};

	m_TerrainTextures[TT_GRASS] = loadTex("Images/Terrain/grass.png");
	m_TerrainTextures[TT_SAND] = loadTex("Images/Terrain/sand.png");
	m_TerrainTextures[TT_BEACH] = m_TerrainTextures[TT_SAND];
	m_TerrainTextures[TT_WATER] = loadTex("Images/Terrain/water_shallow.png");
	m_TerrainTextures[TT_BLESSEDLAND] = loadTex("Images/Terrain/bless.png");
	m_TerrainTextures[TT_RUINEDLAND] = loadTex("Images/Terrain/ruin.png");
	m_TerrainTextures[TT_LAVA] = loadTex("Images/Terrain/lava.png");
	m_TerrainTextures[TT_FARMLAND] = loadTex("Images/Terrain/farm.png");
	m_TerrainTextures[TT_HOUSE] = loadTex("Images/Terrain/villagesmall.png");
	m_TerrainTextures[TT_FLATLAND] = m_TerrainTextures[TT_GRASS];
	m_TerrainTextures[TT_SWAMP] = loadTex("Images/Terrain/swampterrain.png");

	for (int i = 0; i < TT_LASTTERRAINTYPE; ++i)
	{
		if (m_TerrainTextures[i].id > 0)
			SetTextureFilter(m_TerrainTextures[i], TEXTURE_FILTER_POINT);
	}

	Image white = GenImageColor(1, 1, WHITE);
	m_WhiteTexture = LoadTextureFromImage(white);
	UnloadImage(white);

	BuildAtlas();
}

void Terrain::BuildAtlas()
{
	if (m_AtlasOwned && m_AtlasTexture.id > 0)
	{
		UnloadTexture(m_AtlasTexture);
		m_AtlasTexture = {};
		m_AtlasOwned = false;
	}

	const int atlasSize = kAtlasTile * kAtlasCols;
	Image atlas = GenImageColor(atlasSize, atlasSize, BLACK);

	static const char* kPaths[TT_LASTTERRAINTYPE] = {
		"Images/Terrain/sand.png",           // TT_SAND
		"Images/Terrain/sand.png",           // TT_BEACH
		"Images/Terrain/grass.png",          // TT_GRASS
		"Images/Terrain/bless.png",          // TT_BLESSEDLAND
		"Images/Terrain/ruin.png",           // TT_RUINEDLAND
		"Images/Terrain/lava.png",           // TT_LAVA
		"Images/Terrain/water_shallow.png",  // TT_WATER
		"Images/Terrain/farm.png",           // TT_FARMLAND
		"Images/Terrain/villagesmall.png",   // TT_HOUSE
		"Images/Terrain/grass.png",          // TT_FLATLAND
		"Images/Terrain/swampterrain.png",   // TT_SWAMP
	};

	for (int t = 0; t < TT_LASTTERRAINTYPE; ++t)
	{
		const int col = t % kAtlasCols;
		const int row = t / kAtlasCols;
		const Rectangle dst = {
			static_cast<float>(col * kAtlasTile),
			static_cast<float>(row * kAtlasTile),
			static_cast<float>(kAtlasTile),
			static_cast<float>(kAtlasTile)
		};

		Image src = LoadImage(kPaths[t]);
		if (src.data != nullptr)
		{
			ImageResize(&src, kAtlasTile, kAtlasTile);
			ImageDraw(&atlas, src,
				Rectangle{ 0, 0, static_cast<float>(src.width), static_cast<float>(src.height) },
				dst, WHITE);
			UnloadImage(src);
		}
		else
		{
			ImageDrawRectangle(&atlas,
				static_cast<int>(dst.x), static_cast<int>(dst.y),
				kAtlasTile, kAtlasTile, BaseColorForType(t));
		}
	}

	m_AtlasTexture = LoadTextureFromImage(atlas);
	m_AtlasOwned = true;
	SetTextureFilter(m_AtlasTexture, TEXTURE_FILTER_POINT);
	UnloadImage(atlas);
}

void Terrain::AtlasUvRect(int terrainType, float& u0, float& v0, float& u1, float& v1) const
{
	int t = terrainType;
	if (t < 0 || t >= TT_LASTTERRAINTYPE)
		t = TT_GRASS;
	const int col = t % kAtlasCols;
	const int row = t / kAtlasCols;
	const float s = 1.0f / static_cast<float>(kAtlasCols);
	u0 = static_cast<float>(col) * s;
	v0 = static_cast<float>(row) * s;
	u1 = u0 + s;
	v1 = v0 + s;
}

void Terrain::InitializeMap(unsigned int seed)
{
	m_Seed = seed;
	if (!g_vitalRNG)
		g_vitalRNG = std::make_unique<RNG>();
	g_vitalRNG->SeedRNG(seed);

	std::fill(m_Values.begin(), m_Values.end(), 0.0f);

	// Classic Planitia: (vertexWidth + vertexHeight) random hemisphere hills.
	const int hillCount = m_VertexWidth + m_VertexHeight;
	for (int i = 0; i < hillCount; ++i)
		AddHill();

	float highest = 0.0f;
	for (float v : m_Values)
		highest = std::max(highest, v);

	if (highest > 0.0f)
	{
		const float scalar = 8.0f / highest;
		for (float& v : m_Values)
			v *= scalar;
	}

	// Sink so water appears (original Random(3) -> 0..2).
	const int sink = static_cast<int>(g_vitalRNG->Random(3));
	for (float& v : m_Values)
	{
		v -= static_cast<float>(sink);
		if (v < 0.0f)
			v = 0.0f;
	}

	// Seam map edges to sea level so water doesn't show under the rim.
	for (int x = 0; x < m_VertexWidth; ++x)
	{
		for (int z = 0; z < m_VertexHeight; ++z)
		{
			if (x == 0 || x == m_CellWidth || z == 0 || z == m_CellHeight)
				SetValue(x, z, 0.0f);
			else if (x == 1 || x == m_CellWidth - 1 || z == 1 || z == m_CellHeight - 1)
			{
				if (GetValue(x, z) > 1.0f)
					SetValue(x, z, 1.0f);
			}
		}
	}

	AssignTerrainTypesFromHeights();

	m_MaxHeight = 0.0f;
	for (float v : m_Values)
		m_MaxHeight = std::max(m_MaxHeight, v);
}

void Terrain::AddHill()
{
	if (!g_vitalRNG)
		return;
	const float radius = static_cast<float>(g_vitalRNG->RandomRange(1, 8));
	const float cx = static_cast<float>(g_vitalRNG->Random(m_VertexWidth));
	const float cz = static_cast<float>(g_vitalRNG->Random(m_VertexHeight));
	AddHill(cx, cz, radius);
}

void Terrain::AddHill(float cx, float cz, float radius)
{
	// Hemisphere: height contribution = sqrt(r2 - dx2 - dz2) inside the circle.
	for (float i = -radius; i <= radius; ++i)
	{
		const float rowExtent = std::sqrt(std::max(0.0f, radius * radius - i * i));
		for (float j = -rowExtent; j <= rowExtent; ++j)
		{
			float value = radius * radius - i * i - j * j;
			if (value < 0.0f)
				value = 0.0f;
			const int x = static_cast<int>(cx + i);
			const int z = static_cast<int>(cz + j);
			if (x >= 0 && x < m_VertexWidth && z >= 0 && z < m_VertexHeight)
				OffsetValue(x, z, std::sqrt(value));
		}
	}
}

float Terrain::GetValue(int x, int z) const
{
	if (x < 0 || x >= m_VertexWidth || z < 0 || z >= m_VertexHeight)
		return 0.0f;
	return m_Values[static_cast<size_t>(x + z * m_VertexWidth)];
}

void Terrain::SetValue(int x, int z, float value)
{
	if (x < 0 || x >= m_VertexWidth || z < 0 || z >= m_VertexHeight)
		return;
	m_Values[static_cast<size_t>(x + z * m_VertexWidth)] = value;
	if (value > m_MaxHeight)
		m_MaxHeight = value;
	MarkCellsTouchingVertex(x, z);
}

void Terrain::OffsetValue(int x, int z, float value)
{
	SetValue(x, z, GetValue(x, z) + value);
}

float Terrain::GetMiddle(int x, int z) const
{
	return 0.25f * (GetValue(x, z) + GetValue(x + 1, z) + GetValue(x, z + 1) + GetValue(x + 1, z + 1));
}

float Terrain::GetMaxHeight() const
{
	return m_MaxHeight;
}

bool Terrain::ShouldFlipDiagonal(int cellX, int cellZ) const
{
	// Prefer the shorter diagonal (LittleWars / Planitia software path).
	const float heightUL = GetValue(cellX, cellZ);
	const float heightUR = GetValue(cellX + 1, cellZ);
	const float heightLR = GetValue(cellX + 1, cellZ + 1);
	const float heightLL = GetValue(cellX, cellZ + 1);
	const float diffAD = std::fabs(heightUL - heightLR);
	const float diffBC = std::fabs(heightUR - heightLL);
	return diffAD > diffBC;
}

bool Terrain::Raycast(const Ray& ray, Vector3& outHit) const
{
	// Same stepping approach as LittleWars RaycastCombatTerrain.
	constexpr float kMaxDistance = 300.0f;
	constexpr float kStepDistance = 0.35f;
	constexpr float kFallbackSurfaceDistance = 2.5f;

	const Vector3 direction = Vector3Normalize(ray.direction);
	bool hasPreviousSample = false;
	float previousT = 0.0f;
	float previousHeightDelta = 0.0f;

	float closestSurfaceDistance = kFallbackSurfaceDistance;
	Vector3 closestSurfacePoint{};

	const float maxX = static_cast<float>(m_CellWidth);
	const float maxZ = static_cast<float>(m_CellHeight);

	for (float t = 0.0f; t <= kMaxDistance; t += kStepDistance)
	{
		const Vector3 point = Vector3Add(ray.position, Vector3Scale(direction, t));
		if (point.x < 0.0f || point.z < 0.0f || point.x > maxX || point.z > maxZ)
		{
			continue;
		}

		const float terrainY = GetHeight(point.x, point.z);
		const float heightDelta = point.y - terrainY;
		const float surfaceDistance = std::fabs(heightDelta);
		if (surfaceDistance < closestSurfaceDistance)
		{
			closestSurfaceDistance = surfaceDistance;
			closestSurfacePoint = Vector3{ point.x, terrainY, point.z };
		}

		if (hasPreviousSample && previousHeightDelta > 0.0f && heightDelta <= 0.0f)
		{
			const float blend = previousHeightDelta / (previousHeightDelta - heightDelta + 0.0001f);
			const float hitT = previousT + (t - previousT) * blend;
			const Vector3 hitPoint = Vector3Add(ray.position, Vector3Scale(direction, hitT));
			outHit = Vector3{
				hitPoint.x,
				GetHeight(hitPoint.x, hitPoint.z),
				hitPoint.z
			};
			return true;
		}

		hasPreviousSample = true;
		previousT = t;
		previousHeightDelta = heightDelta;
	}

	if (closestSurfaceDistance < kFallbackSurfaceDistance)
	{
		outHit = closestSurfacePoint;
		return true;
	}

	return false;
}

float Terrain::GetHeight(float x, float z) const
{
	if (x < 0.0f || z < 0.0f || x > m_VertexWidth - 1 || z > m_VertexHeight - 1)
		return -9999.0f;

	const int xWhole = static_cast<int>(x);
	const int zWhole = static_cast<int>(z);
	const float xFrac = x - static_cast<float>(xWhole);
	const float zFrac = z - static_cast<float>(zWhole);

	// Corners + center (same topology as RebuildMesh's 4-tri fan).
	const float hUL = GetValue(xWhole, zWhole);
	const float hUR = GetValue(xWhole + 1, zWhole);
	const float hLR = GetValue(xWhole + 1, zWhole + 1);
	const float hLL = GetValue(xWhole, zWhole + 1);
	const float hM = 0.25f * (hUL + hUR + hLR + hLL);

	const float px = xFrac;
	const float pz = zFrac;
	float u = 0.0f, v = 0.0f, w = 0.0f;

	// North: UL(0,0)-UR(1,0)-M(0.5,0.5)
	if (BarycentricXZ(px, pz, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, u, v, w))
		return u * hUL + v * hUR + w * hM;
	// East: UR(1,0)-LR(1,1)-M
	if (BarycentricXZ(px, pz, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, u, v, w))
		return u * hUR + v * hLR + w * hM;
	// South: LR(1,1)-LL(0,1)-M
	if (BarycentricXZ(px, pz, 1.0f, 1.0f, 0.0f, 1.0f, 0.5f, 0.5f, u, v, w))
		return u * hLR + v * hLL + w * hM;
	// West: LL(0,1)-UL(0,0)-M
	if (BarycentricXZ(px, pz, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f, u, v, w))
		return u * hLL + v * hUL + w * hM;

	return hM;
}

int Terrain::GetTerrainType(int x, int z) const
{
	if (x < 0 || x >= m_CellWidth || z < 0 || z >= m_CellHeight)
		return -1;
	return m_Cells[static_cast<size_t>(x + z * m_CellWidth)].m_TerrainType;
}

void Terrain::SetTerrainType(int x, int z, int type)
{
	if (x < 0 || x >= m_CellWidth || z < 0 || z >= m_CellHeight)
		return;
	auto& cell = m_Cells[static_cast<size_t>(x + z * m_CellWidth)];
	if (cell.m_TerrainType == type)
		return;
	cell.m_TerrainType = type;
	MarkCellDirty(x, z);
}

bool Terrain::IsValidVillageTerrain(int x, int z) const
{
	if (x < 0 || x >= m_CellWidth || z < 0 || z >= m_CellHeight)
		return false;

	const float target = GetValue(x, z);
	return GetValue(x + 1, z + 1) == target
		&& GetValue(x, z + 1) == target
		&& GetValue(x + 1, z) == target
		&& GetMiddle(x, z) == target
		&& GetMiddle(x, z) > 1.0f;
}

bool Terrain::IsWaterCell(int x, int z) const
{
	return GetValue(x, z) == 0.0f
		&& GetValue(x + 1, z) == 0.0f
		&& GetValue(x + 1, z + 1) == 0.0f
		&& GetValue(x, z + 1) == 0.0f;
}

int Terrain::FindTerrainType(int cellX, int cellZ) const
{
	if (cellX < 0 || cellX >= m_CellWidth || cellZ < 0 || cellZ >= m_CellHeight)
		return TT_SAND;

	const float a = GetValue(cellX, cellZ);
	const float b = GetValue(cellX + 1, cellZ);
	const float c = GetValue(cellX + 1, cellZ + 1);
	const float d = GetValue(cellX, cellZ + 1);

	if (a == 0.0f && b == 0.0f && c == 0.0f && d == 0.0f)
		return TT_WATER;
	if (a < 0.4f || b < 0.4f || c < 0.4f || d < 0.4f)
		return TT_BEACH;
	if (GetMiddle(cellX, cellZ) < 0.75f)
		return TT_SAND;
	return TT_GRASS;
}

void Terrain::AssignTerrainTypesFromHeights()
{
	for (int x = 0; x < m_CellWidth; ++x)
	{
		for (int z = 0; z < m_CellHeight; ++z)
		{
			const int existing = GetTerrainType(x, z);
			if (existing == TT_FARMLAND || existing == TT_HOUSE
				|| existing == TT_BLESSEDLAND || existing == TT_RUINEDLAND)
				continue;
			SetTerrainType(x, z, FindTerrainType(x, z));
		}
	}
}

void Terrain::ScrubTerrainCell(int cellX, int cellZ)
{
	for (int x = cellX - 1; x <= cellX + 1; ++x)
	{
		for (int z = cellZ - 1; z <= cellZ + 1; ++z)
		{
			if (x < 0 || x >= m_CellWidth || z < 0 || z >= m_CellHeight)
				continue;
			const int existing = GetTerrainType(x, z);
			if (existing == TT_FARMLAND || existing == TT_HOUSE
				|| existing == TT_BLESSEDLAND || existing == TT_RUINEDLAND)
				continue;
			SetTerrainType(x, z, FindTerrainType(x, z)); // dirties only if type changes
		}
	}
	// Heights around this cell may have changed via SetValue -> already dirty.
}

void Terrain::MarkMeshDirty()
{
	m_FullRebuild = true;
	RecalculateMaxHeight();
}

void Terrain::RecalculateMaxHeight()
{
	m_MaxHeight = 0.0f;
	for (float v : m_Values)
		m_MaxHeight = std::max(m_MaxHeight, v);
}

void Terrain::MarkCellDirty(int cellX, int cellZ)
{
	if (cellX < 0 || cellX >= m_CellWidth || cellZ < 0 || cellZ >= m_CellHeight)
		return;
	if (m_CellDirtyFlag.empty())
		m_CellDirtyFlag.assign(static_cast<size_t>(m_CellWidth * m_CellHeight), 0);
	const int idx = cellX + cellZ * m_CellWidth;
	if (m_CellDirtyFlag[static_cast<size_t>(idx)])
		return;
	m_CellDirtyFlag[static_cast<size_t>(idx)] = 1;
	m_DirtyCells.push_back(idx);
}

void Terrain::MarkCellsTouchingVertex(int vx, int vz)
{
	// A heightfield vertex is shared by up to 4 cells.
	MarkCellDirty(vx - 1, vz - 1);
	MarkCellDirty(vx - 1, vz);
	MarkCellDirty(vx, vz - 1);
	MarkCellDirty(vx, vz);
}

bool Terrain::FlattenToward(int cellX, int cellZ, float targetHeight, float step)
{
	bool changed = false;
	for (int i = cellX; i <= cellX + 1; ++i)
	{
		for (int j = cellZ; j <= cellZ + 1; ++j)
		{
			float target = targetHeight;
			if (i == 0 || j == 0 || i == m_CellWidth || j == m_CellHeight)
				target = 0.0f;
			else if (i == 1 || j == 1 || i == m_CellWidth - 1 || j == m_CellHeight - 1)
				target = std::min(target, 0.5f);

			float v = GetValue(i, j);
			if (v < target)
			{
				v = std::min(v + step, target);
				SetValue(i, j, v);
				changed = true;
			}
			else if (v > target)
			{
				v = std::max(v - step, target);
				SetValue(i, j, v);
				changed = true;
			}
		}
	}
	if (changed)
		ScrubTerrainCell(cellX, cellZ);
	return changed;
}

bool Terrain::RaiseArea(int cellX, int cellZ, float step)
{
	bool changed = false;
	for (int i = cellX - 1; i <= cellX + 2; ++i)
	{
		for (int j = cellZ - 1; j <= cellZ + 2; ++j)
		{
			const float v = GetValue(i, j);
			if (v + step < 8.0f)
			{
				SetValue(i, j, v + step);
				changed = true;
			}
		}
	}
	if (changed)
		ScrubTerrainCell(cellX, cellZ);
	return changed;
}

bool Terrain::LowerArea(int cellX, int cellZ, float step)
{
	bool changed = false;
	for (int i = cellX - 1; i <= cellX + 2; ++i)
	{
		for (int j = cellZ - 1; j <= cellZ + 2; ++j)
		{
			const float v = GetValue(i, j);
			if (v - step > 0.0f)
			{
				SetValue(i, j, v - step);
				changed = true;
			}
			else if (v > 0.0f)
			{
				SetValue(i, j, 0.0f);
				changed = true;
			}
		}
	}
	if (changed)
		ScrubTerrainCell(cellX, cellZ);
	return changed;
}

Color Terrain::BaseColorForType(int terrainType) const
{
	switch (terrainType)
	{
	case TT_WATER: return Color{ 40, 90, 160, 255 };
	case TT_SAND:  return Color{ 194, 178, 128, 255 };
	case TT_BEACH: return Color{ 210, 190, 140, 255 };
	case TT_GRASS: return Color{ 70, 140, 60, 255 };
	case TT_BLESSEDLAND: return Color{ 160, 220, 90, 255 };
	case TT_RUINEDLAND: return Color{ 90, 80, 70, 255 };
	case TT_LAVA: return Color{ 200, 60, 20, 255 };
	case TT_FARMLAND: return Color{ 160, 140, 60, 255 };
	case TT_HOUSE: return Color{ 120, 90, 60, 255 };
	case TT_SWAMP: return Color{ 50, 80, 45, 255 };
	default:       return Color{ 100, 100, 100, 255 };
	}
}

Texture2D Terrain::TextureForType(int terrainType) const
{
	if (terrainType < 0 || terrainType >= TT_LASTTERRAINTYPE)
		return m_WhiteTexture;
	if (m_TerrainTextures[terrainType].id > 0)
		return m_TerrainTextures[terrainType];
	if (m_TerrainTextures[TT_GRASS].id > 0)
		return m_TerrainTextures[TT_GRASS];
	return m_WhiteTexture;
}

void Terrain::UnloadDrawMeshes()
{
	if (m_TerrainModelLoaded)
	{
		UnloadModel(m_TerrainModel);
		m_TerrainModel = {};
		m_TerrainModelLoaded = false;
	}
}

void Terrain::FillCellVertices(int cellX, int cellZ, Vertex* out12) const
{
	const bool textured = (m_DrawMode == TerrainDrawMode::Textured);
	int type = GetTerrainType(cellX, cellZ);
	if (type < 0 || type >= TT_LASTTERRAINTYPE)
		type = TT_GRASS;
	if (IsWaterCell(cellX, cellZ))
		type = TT_WATER;

	float au0 = 0, av0 = 0, au1 = 1, av1 = 1;
	AtlasUvRect(type, au0, av0, au1, av1);
	auto mapU = [&](float u) { return au0 + u * (au1 - au0); };
	auto mapV = [&](float v) { return av0 + v * (av1 - av0); };

	const Color albedo = textured ? WHITE : BaseColorForType(type);
	const float alpha = (type == TT_WATER) ? (200.0f / 255.0f) : 1.0f;

	Vector3 ul, ur, lr, ll, mid;
	if (type == TT_WATER)
	{
		const float y = m_WaterHeight;
		ul = { static_cast<float>(cellX), y, static_cast<float>(cellZ) };
		ur = { static_cast<float>(cellX + 1), y, static_cast<float>(cellZ) };
		lr = { static_cast<float>(cellX + 1), y, static_cast<float>(cellZ + 1) };
		ll = { static_cast<float>(cellX), y, static_cast<float>(cellZ + 1) };
		mid = { static_cast<float>(cellX) + 0.5f, y, static_cast<float>(cellZ) + 0.5f };
	}
	else
	{
		const float hUL = GetValue(cellX, cellZ);
		const float hUR = GetValue(cellX + 1, cellZ);
		const float hLR = GetValue(cellX + 1, cellZ + 1);
		const float hLL = GetValue(cellX, cellZ + 1);
		const float hM = 0.25f * (hUL + hUR + hLR + hLL);
		ul = { static_cast<float>(cellX), hUL, static_cast<float>(cellZ) };
		ur = { static_cast<float>(cellX + 1), hUR, static_cast<float>(cellZ) };
		lr = { static_cast<float>(cellX + 1), hLR, static_cast<float>(cellZ + 1) };
		ll = { static_cast<float>(cellX), hLL, static_cast<float>(cellZ + 1) };
		mid = { static_cast<float>(cellX) + 0.5f, hM, static_cast<float>(cellZ) + 0.5f };
	}

	EmitFlatTriangle(out12 + 0, ul, ur, mid, albedo,
		mapU(0), mapV(0), mapU(1), mapV(0), mapU(0.5f), mapV(0.5f), alpha);
	EmitFlatTriangle(out12 + 3, ur, lr, mid, albedo,
		mapU(1), mapV(0), mapU(1), mapV(1), mapU(0.5f), mapV(0.5f), alpha);
	EmitFlatTriangle(out12 + 6, lr, ll, mid, albedo,
		mapU(1), mapV(1), mapU(0), mapV(1), mapU(0.5f), mapV(0.5f), alpha);
	EmitFlatTriangle(out12 + 9, ll, ul, mid, albedo,
		mapU(0), mapV(1), mapU(0), mapV(0), mapU(0.5f), mapV(0.5f), alpha);
}

void Terrain::WriteCellToMesh(int cellX, int cellZ)
{
	if (!m_TerrainModelLoaded || m_TerrainModel.meshCount <= 0)
		return;

	Mesh& mesh = m_TerrainModel.meshes[0];
	const int cellIndex = cellX + cellZ * m_CellWidth;
	const int vertStart = cellIndex * kVertsPerCell;

	Vertex verts[kVertsPerCell];
	FillCellVertices(cellX, cellZ, verts);

	float positions[kVertsPerCell * 3];
	float texcoords[kVertsPerCell * 2];
	unsigned char colors[kVertsPerCell * 4];
	for (int i = 0; i < kVertsPerCell; ++i)
	{
		positions[i * 3 + 0] = verts[i].x;
		positions[i * 3 + 1] = verts[i].y;
		positions[i * 3 + 2] = verts[i].z;
		texcoords[i * 2 + 0] = verts[i].u;
		texcoords[i * 2 + 1] = verts[i].v;
		colors[i * 4 + 0] = static_cast<unsigned char>(std::clamp(verts[i].r, 0.0f, 1.0f) * 255.0f);
		colors[i * 4 + 1] = static_cast<unsigned char>(std::clamp(verts[i].g, 0.0f, 1.0f) * 255.0f);
		colors[i * 4 + 2] = static_cast<unsigned char>(std::clamp(verts[i].b, 0.0f, 1.0f) * 255.0f);
		colors[i * 4 + 3] = static_cast<unsigned char>(std::clamp(verts[i].a, 0.0f, 1.0f) * 255.0f);

		// Keep CPU copy in sync for tools / future reads.
		if (mesh.vertices)
		{
			mesh.vertices[vertStart * 3 + i * 3 + 0] = positions[i * 3 + 0];
			mesh.vertices[vertStart * 3 + i * 3 + 1] = positions[i * 3 + 1];
			mesh.vertices[vertStart * 3 + i * 3 + 2] = positions[i * 3 + 2];
		}
		if (mesh.texcoords)
		{
			mesh.texcoords[vertStart * 2 + i * 2 + 0] = texcoords[i * 2 + 0];
			mesh.texcoords[vertStart * 2 + i * 2 + 1] = texcoords[i * 2 + 1];
		}
		if (mesh.colors)
		{
			mesh.colors[vertStart * 4 + i * 4 + 0] = colors[i * 4 + 0];
			mesh.colors[vertStart * 4 + i * 4 + 1] = colors[i * 4 + 1];
			mesh.colors[vertStart * 4 + i * 4 + 2] = colors[i * 4 + 2];
			mesh.colors[vertStart * 4 + i * 4 + 3] = colors[i * 4 + 3];
		}
	}

	UpdateMeshBuffer(mesh, 0, positions, static_cast<int>(sizeof(positions)),
		vertStart * 3 * static_cast<int>(sizeof(float)));
	UpdateMeshBuffer(mesh, 1, texcoords, static_cast<int>(sizeof(texcoords)),
		vertStart * 2 * static_cast<int>(sizeof(float)));
	UpdateMeshBuffer(mesh, 3, colors, static_cast<int>(sizeof(colors)),
		vertStart * 4 * static_cast<int>(sizeof(unsigned char)));
}

void Terrain::FlushDirtyCells()
{
	if (m_DirtyCells.empty())
		return;
	for (int idx : m_DirtyCells)
	{
		const int x = idx % m_CellWidth;
		const int z = idx / m_CellWidth;
		WriteCellToMesh(x, z);
		m_CellDirtyFlag[static_cast<size_t>(idx)] = 0;
	}
	m_DirtyCells.clear();
}

void Terrain::RebuildMesh()
{
	UnloadDrawMeshes();

	const int cellCount = m_CellWidth * m_CellHeight;
	std::vector<Vertex> allVerts(static_cast<size_t>(cellCount * kVertsPerCell));
	for (int z = 0; z < m_CellHeight; ++z)
	{
		for (int x = 0; x < m_CellWidth; ++x)
		{
			const int idx = x + z * m_CellWidth;
			FillCellVertices(x, z, allVerts.data() + idx * kVertsPerCell);
		}
	}

	Mesh mesh = BuildMeshFromTriangleSoup(allVerts);
	m_TerrainModel = LoadModelFromMesh(mesh);
	if (m_TerrainModel.materialCount > 0)
	{
		const bool textured = (m_DrawMode == TerrainDrawMode::Textured);
		Texture2D tex = textured && m_AtlasTexture.id > 0 ? m_AtlasTexture : m_WhiteTexture;
		if (tex.id > 0)
			m_TerrainModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
	}
	m_TerrainModelLoaded = true;
	m_FullRebuild = false;
	m_DirtyCells.clear();
	std::fill(m_CellDirtyFlag.begin(), m_CellDirtyFlag.end(), 0);

	Log("Terrain rebuilt: " + std::to_string(m_CellWidth) + "x" + std::to_string(m_CellHeight)
		+ " cells, seed " + std::to_string(m_Seed)
		+ ", tris " + std::to_string(cellCount * 4)
		+ ", mode " + GetDrawModeName());
}

void Terrain::Update()
{
	if (m_FullRebuild || !m_TerrainModelLoaded)
	{
		RebuildMesh();
		return;
	}
	FlushDirtyCells();
}

void Terrain::Draw()
{
	if (!m_TerrainModelLoaded)
		return;

	const bool wire = (m_DrawMode == TerrainDrawMode::Wireframe);
	const bool colorsOnly = (m_DrawMode == TerrainDrawMode::Colors);

	if (m_TerrainModel.materialCount > 0)
	{
		if (colorsOnly && m_WhiteTexture.id > 0)
			m_TerrainModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = m_WhiteTexture;
		else if (m_AtlasTexture.id > 0)
			m_TerrainModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = m_AtlasTexture;
	}

	if (wire)
	{
		DrawModelWires(m_TerrainModel, Vector3{ 0, 0, 0 }, 1.0f, Color{ 220, 220, 100, 255 });
		return;
	}

	// Keep depth writes on so units sort against hills. Water uses vertex alpha + blending.
	DrawModel(m_TerrainModel, Vector3{ 0, 0, 0 }, 1.0f, WHITE);
}
