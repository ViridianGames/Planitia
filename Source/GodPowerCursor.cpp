#include "GodPowerCursor.h"

#include "GameGlobals.h"
#include "Geist/Config.h"
#include "Geist/Globals.h"
#include "Geist/ResourceManager.h"
#include "Terrain.h"

#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace GodPowerCursor
{
namespace
{
	Texture2D* g_RingTex = nullptr;     // point-spell marker (Flatten / bolt)
	Texture2D* g_ParticleTex = nullptr; // orbiting AOE icons

	enum class Style
	{
		None,
		PointRing, // small draped ring at the hit (no area)
		OrbitIcons // circle of particles around AOE radius
	};

	struct CursorInfo
	{
		Style style = Style::None;
		Color tint{ 255, 255, 255, 255 };
		float radius = 1.0f;
	};

	// Colors match the original Planitia MainState switch on input mode.
	CursorInfo InfoFor(PlayerAction power)
	{
		CursorInfo info;
		switch (power)
		{
		case PlayerAction::Flatten:
		case PlayerAction::Raise:
		case PlayerAction::Lower:
			info.style = Style::PointRing;
			info.tint = Color{ 255, 255, 0, 230 };
			info.radius = 0.65f;
			break;

		case PlayerAction::Lightning:
			info.style = Style::PointRing;
			info.tint = Color{ 64, 128, 255, 230 };
			info.radius = 0.65f;
			break;

		case PlayerAction::Flamestrike:
			info.style = Style::PointRing;
			info.tint = Color{ 255, 64, 64, 230 };
			info.radius = 0.65f;
			break;

		case PlayerAction::Earthquake:
			info.style = Style::OrbitIcons;
			info.tint = Color{ 255, 255, 0, 230 };
			info.radius = 5.0f;
			break;

		case PlayerAction::StoneRain:
			info.style = Style::OrbitIcons;
			info.tint = Color{ 255, 64, 64, 230 };
			info.radius = 1.5f; // 3x3
			break;

		case PlayerAction::Bless:
			info.style = Style::OrbitIcons;
			info.tint = Color{ 0, 255, 0, 230 };
			info.radius = 1.5f; // 3x3
			break;

		case PlayerAction::Swamp:
			info.style = Style::OrbitIcons;
			info.tint = Color{ 0, 255, 0, 230 };
			info.radius = 2.5f; // 5x5
			break;

		case PlayerAction::HealingLight:
			info.style = Style::OrbitIcons;
			info.tint = Color{ 0, 255, 0, 230 };
			info.radius = 2.0f;
			break;

		default:
			info.style = Style::None;
			break;
		}
		return info;
	}

	float GroundAt(float x, float z)
	{
		if (!g_Terrain)
			return 0.05f;
		return g_Terrain->GetHeight(x, z) + 0.04f;
	}

	void DrawPointRing(float cx, float cz, float radius, Color tint)
	{
		if (!g_RingTex || g_RingTex->id == 0)
			return;

		const float ang = GetTime();
		const float c = std::cos(ang);
		const float s = std::sin(ang);

		Vector2 uv[4];
		const Vector2 uvOff[4] = {
			{ -0.5f, -0.5f },
			{ -0.5f, 0.5f },
			{ 0.5f, 0.5f },
			{ 0.5f, -0.5f },
		};
		for (int i = 0; i < 4; ++i)
		{
			uv[i].x = 0.5f + uvOff[i].x * c - uvOff[i].y * s;
			uv[i].y = 0.5f + uvOff[i].x * s + uvOff[i].y * c;
		}

		const float x0 = cx - radius;
		const float x1 = cx + radius;
		const float z0 = cz - radius;
		const float z1 = cz + radius;
		const Vector3 corner[4] = {
			{ x0, GroundAt(x0, z0), z0 },
			{ x0, GroundAt(x0, z1), z1 },
			{ x1, GroundAt(x1, z1), z1 },
			{ x1, GroundAt(x1, z0), z0 },
		};
		const Vector3 center{ cx, GroundAt(cx, cz), cz };

		Mesh mesh{};
		mesh.triangleCount = 4;
		mesh.vertexCount = 12;
		mesh.vertices = static_cast<float*>(MemAlloc(12 * 3 * sizeof(float)));
		mesh.texcoords = static_cast<float*>(MemAlloc(12 * 2 * sizeof(float)));
		mesh.normals = static_cast<float*>(MemAlloc(12 * 3 * sizeof(float)));

		auto writeVert = [&](int index, Vector3 pos, Vector2 tex)
		{
			mesh.vertices[index * 3 + 0] = pos.x;
			mesh.vertices[index * 3 + 1] = pos.y;
			mesh.vertices[index * 3 + 2] = pos.z;
			mesh.texcoords[index * 2 + 0] = tex.x;
			mesh.texcoords[index * 2 + 1] = tex.y;
			mesh.normals[index * 3 + 0] = 0.0f;
			mesh.normals[index * 3 + 1] = 1.0f;
			mesh.normals[index * 3 + 2] = 0.0f;
		};

		int vi = 0;
		for (int i = 0; i < 4; ++i)
		{
			const int j = (i + 1) % 4;
			writeVert(vi++, corner[i], uv[i]);
			writeVert(vi++, corner[j], uv[j]);
			writeVert(vi++, center, Vector2{ 0.5f, 0.5f });
		}

		UploadMesh(&mesh, false);

		static Material s_ringMat{};
		static bool s_ringMatReady = false;
		if (!s_ringMatReady)
		{
			s_ringMat = LoadMaterialDefault();
			s_ringMatReady = true;
		}
		SetMaterialTexture(&s_ringMat, MATERIAL_MAP_DIFFUSE, *g_RingTex);
		s_ringMat.maps[MATERIAL_MAP_DIFFUSE].color = tint;

		rlDisableDepthMask();
		BeginBlendMode(BLEND_ALPHA);
		DrawMesh(mesh, s_ringMat, MatrixIdentity());
		EndBlendMode();
		rlEnableDepthMask();

		UnloadMesh(mesh);
	}

	// Populous-style: discrete icons orbiting the AOE center, floating on the terrain.
	void DrawOrbitingIcons(const Camera3D& camera, float cx, float cz, float radius, Color tint)
	{
		if (!g_ParticleTex || g_ParticleTex->id == 0)
			return;

		// More icons for larger radii; keep a readable density.
		const int count = std::clamp(static_cast<int>(std::round(radius * 4.0f)), 8, 24);
		const float spin = GetTime() * 0.85f; // rad/sec around the center
		const float iconSize = 0.45f;
		const Rectangle src{
			0.0f, 0.0f,
			static_cast<float>(g_ParticleTex->width),
			static_cast<float>(g_ParticleTex->height)
		};

		rlDisableDepthMask();
		BeginBlendMode(BLEND_ALPHA);
		for (int i = 0; i < count; ++i)
		{
			const float a = spin + (static_cast<float>(i) / static_cast<float>(count)) * (2.0f * PI);
			const float x = cx + std::cos(a) * radius;
			const float z = cz + std::sin(a) * radius;
			const Vector3 pos{ x, GroundAt(x, z) + 0.18f, z };

			DrawBillboardPro(
				camera,
				*g_ParticleTex,
				src,
				pos,
				Vector3{ 0, 1, 0 },
				Vector2{ iconSize, iconSize },
				Vector2{ iconSize * 0.5f, iconSize * 0.5f },
				0.0f,
				tint);
		}
		EndBlendMode();
		rlEnableDepthMask();
	}
} // namespace

void Init()
{
	if (!g_ResourceManager)
		return;
	g_RingTex = g_ResourceManager->GetTexture("Images/TerrainHighlightRing.png", false);
	g_ParticleTex = g_ResourceManager->GetTexture("Images/particle.png", false);
	if (g_RingTex && g_RingTex->id != 0)
	{
		SetTextureFilter(*g_RingTex, TEXTURE_FILTER_BILINEAR);
		SetTextureWrap(*g_RingTex, TEXTURE_WRAP_CLAMP);
	}
	if (g_ParticleTex && g_ParticleTex->id != 0)
	{
		SetTextureFilter(*g_ParticleTex, TEXTURE_FILTER_BILINEAR);
		SetTextureWrap(*g_ParticleTex, TEXTURE_WRAP_CLAMP);
	}
}

void Shutdown()
{
	g_RingTex = nullptr;
	g_ParticleTex = nullptr;
}

bool Draw(const Camera3D& camera, PlayerAction power, Vector3 terrainHit, bool validHit)
{
	const CursorInfo info = InfoFor(power);
	if (info.style == Style::None || !validHit || !g_Terrain)
		return false;

	const float cx = terrainHit.x;
	const float cz = terrainHit.z;

	if (info.style == Style::OrbitIcons)
		DrawOrbitingIcons(camera, cx, cz, info.radius, info.tint);
	else
		DrawPointRing(cx, cz, info.radius, info.tint);

	return true;
}

} // namespace GodPowerCursor
