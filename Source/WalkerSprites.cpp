#include "WalkerSprites.h"

#include "GameGlobals.h"
#include "Unit.h"
#include "Geist/Config.h"
#include "Geist/Globals.h"
#include "Geist/Logging.h"
#include "Geist/ResourceManager.h"

#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace WalkerSprites
{
namespace
{
	constexpr const char* kFixedPath = "Images/VillagerWalkFixed.png";
	constexpr const char* kMaskPath = "Images/VillagerWalkMask.png";
	constexpr const char* kShadowPath = "Images/dropshadow.png";

	// World size for upright villager art (Planitia tiles ~1 unit; cuboids were ~0.85 tall).
	constexpr float kWorldHeight = 1.15f;
	constexpr float kWidthScale = 1.35f;
	constexpr float kShadowScale = 0.45f; // relative to sprite width (keep small to limit ground swimming)
	constexpr int kWalkFrameMs = 120;
	constexpr int kTrimAlphaMin = 8;

	// Sheet rows top→bottom: S, SE, E, NE, N, NW, W, SW.
	// Runtime slots:           0=SW, 1=W, 2=NW, 3=N, 4=NE, 5=E, 6=SE, 7=S.
	constexpr int kSheetRowToDir[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };

	std::vector<std::vector<Texture*>> g_colorFrames; // [dir][frame]
	std::vector<std::vector<Texture*>> g_maskFrames;
	Model g_shadowPlane{};
	bool g_shadowPlaneReady = false;
	bool g_loaded = false;
	bool g_loadAttempted = false;

	bool EnsureShadowPlane()
	{
		if (g_shadowPlaneReady)
			return true;
		if (!g_ResourceManager || !g_ResourceManager->DoesFileExist(kShadowPath))
		{
			Log(std::string("WalkerSprites: missing ") + kShadowPath);
			return false;
		}
		// Unit XZ plane; scaled per-draw to match the walker.
		Mesh mesh = GenMeshPlane(1.0f, 1.0f, 1, 1);
		g_shadowPlane = LoadModelFromMesh(mesh);
		g_shadowPlaneReady = (g_shadowPlane.meshCount > 0);
		return g_shadowPlaneReady;
	}

	bool FindOpaqueBounds(const Image& img, int alphaMin, int& minX, int& minY, int& maxX, int& maxY)
	{
		minX = img.width;
		minY = img.height;
		maxX = -1;
		maxY = -1;
		if (img.data == nullptr || img.width <= 0 || img.height <= 0)
			return false;

		for (int y = 0; y < img.height; ++y)
		{
			for (int x = 0; x < img.width; ++x)
			{
				if (GetImageColor(img, x, y).a > alphaMin)
				{
					if (x < minX) minX = x;
					if (y < minY) minY = y;
					if (x > maxX) maxX = x;
					if (y > maxY) maxY = y;
				}
			}
		}
		return maxX >= minX;
	}

	Image MakeUniformCell(const Image& content, int canvasW, int canvasH)
	{
		Image canvas = GenImageColor(canvasW, canvasH, BLANK);
		ImageFormat(&canvas, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
		if (content.data == nullptr || content.width <= 0 || content.height <= 0)
			return canvas;

		const int dstX = (canvasW - content.width) / 2;
		const int dstY = canvasH - content.height; // bottom-align so feet stay planted
		ImageDraw(&canvas, content,
			Rectangle{ 0, 0, float(content.width), float(content.height) },
			Rectangle{ float(dstX), float(dstY), float(content.width), float(content.height) },
			WHITE);
		return canvas;
	}

	// Shirt overlay: white RGB + alpha from mask (A, or inverted R where A is empty).
	void ConvertMaskCellToShirtMatte(Image& img)
	{
		ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
		for (int y = 0; y < img.height; ++y)
		{
			for (int x = 0; x < img.width; ++x)
			{
				Color c = GetImageColor(img, x, y);
				unsigned char strength = c.a;
				if (strength == 0 && c.r < 255)
					strength = static_cast<unsigned char>(255 - c.r);
				ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, strength });
			}
		}
	}

	bool LoadWalkerSheets()
	{
		if (!g_ResourceManager || !g_ResourceManager->DoesFileExist(kFixedPath))
		{
			Log(std::string("WalkerSprites: missing ") + kFixedPath);
			return false;
		}

		Image colorSheet = LoadImage(kFixedPath);
		if (colorSheet.data == nullptr || colorSheet.width <= 0 || colorSheet.height <= 0)
		{
			Log(std::string("WalkerSprites: failed to load ") + kFixedPath);
			if (colorSheet.data != nullptr)
				UnloadImage(colorSheet);
			return false;
		}

		if ((colorSheet.height % 8) != 0)
		{
			Log("WalkerSprites: color sheet height not divisible by 8");
			UnloadImage(colorSheet);
			return false;
		}

		const int cellH = colorSheet.height / 8;
		const int cellW = cellH;
		if (cellW <= 0 || (colorSheet.width % cellW) != 0)
		{
			Log("WalkerSprites: color sheet width not divisible by cell size");
			UnloadImage(colorSheet);
			return false;
		}

		Image maskSheet{};
		bool haveMask = false;
		if (g_ResourceManager->DoesFileExist(kMaskPath))
		{
			maskSheet = LoadImage(kMaskPath);
			if (maskSheet.data != nullptr
				&& maskSheet.width == colorSheet.width
				&& maskSheet.height == colorSheet.height)
			{
				haveMask = true;
			}
			else
			{
				Log("WalkerSprites: mask sheet missing or size mismatch; no team shirt tint");
				if (maskSheet.data != nullptr)
				{
					UnloadImage(maskSheet);
					maskSheet = {};
				}
			}
		}

		const int srcFrames = colorSheet.width / cellW;
		const int frameStep = (srcFrames >= 8) ? 2 : 1;
		const int frameCount = srcFrames / frameStep;

		g_colorFrames.resize(8);
		g_maskFrames.resize(haveMask ? 8 : 0);
		for (int d = 0; d < 8; ++d)
		{
			g_colorFrames[d].assign(frameCount, nullptr);
			if (haveMask)
				g_maskFrames[d].assign(frameCount, nullptr);
		}

		auto colorName = [&](int row, int srcFrame) {
			return std::string("walksheet:") + kFixedPath + ":r" + std::to_string(row) + ":" +
				std::to_string(srcFrame) + ":uni";
		};
		auto maskName = [&](int row, int srcFrame) {
			return std::string("walkmask:") + kMaskPath + ":r" + std::to_string(row) + ":" +
				std::to_string(srcFrame) + ":uni";
		};

		// Fast path: all cells already in ResourceManager.
		bool allCached = true;
		for (int row = 0; row < 8 && allCached; ++row)
		{
			for (int f = 0; f < frameCount; ++f)
			{
				const int srcFrame = f * frameStep;
				if (!g_ResourceManager->DoesTextureExist(colorName(row, srcFrame)))
				{
					allCached = false;
					break;
				}
				if (haveMask && !g_ResourceManager->DoesTextureExist(maskName(row, srcFrame)))
				{
					allCached = false;
					break;
				}
			}
		}
		if (allCached)
		{
			for (int row = 0; row < 8; ++row)
			{
				const int dir = kSheetRowToDir[row];
				for (int f = 0; f < frameCount; ++f)
				{
					const int srcFrame = f * frameStep;
					g_colorFrames[dir][f] = g_ResourceManager->GetTexture(colorName(row, srcFrame), false);
					if (haveMask)
						g_maskFrames[dir][f] = g_ResourceManager->GetTexture(maskName(row, srcFrame), false);
				}
			}
			UnloadImage(colorSheet);
			if (haveMask)
				UnloadImage(maskSheet);
			return true;
		}

		struct CellTrim
		{
			int minX = 0, minY = 0, maxX = 0, maxY = 0;
			bool valid = false;
		};
		std::vector<CellTrim> trims(static_cast<size_t>(8 * frameCount));
		std::vector<Image> colorCells(static_cast<size_t>(8 * frameCount));
		std::vector<Image> maskCells(haveMask ? static_cast<size_t>(8 * frameCount) : 0);

		int maxW = 1;
		int maxH = 1;

		// Pass 1: slice cells; trim bounds come from the COLOR sheet so mask stays aligned.
		for (int row = 0; row < 8; ++row)
		{
			for (int f = 0; f < frameCount; ++f)
			{
				const int srcFrame = f * frameStep;
				const int idx = row * frameCount + f;
				const Rectangle srcRect{
					float(srcFrame * cellW), float(row * cellH),
					float(cellW), float(cellH)
				};

				colorCells[idx] = ImageFromImage(colorSheet, srcRect);
				ImageFormat(&colorCells[idx], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

				CellTrim& t = trims[idx];
				t.valid = FindOpaqueBounds(colorCells[idx], kTrimAlphaMin, t.minX, t.minY, t.maxX, t.maxY);
				if (t.valid)
				{
					Image cropped = ImageFromImage(colorCells[idx], Rectangle{
						float(t.minX), float(t.minY),
						float(t.maxX - t.minX + 1), float(t.maxY - t.minY + 1)
					});
					UnloadImage(colorCells[idx]);
					colorCells[idx] = cropped;
					maxW = std::max(maxW, colorCells[idx].width);
					maxH = std::max(maxH, colorCells[idx].height);
				}

				if (haveMask)
				{
					maskCells[idx] = ImageFromImage(maskSheet, srcRect);
					ConvertMaskCellToShirtMatte(maskCells[idx]);
					if (t.valid)
					{
						Image cropped = ImageFromImage(maskCells[idx], Rectangle{
							float(t.minX), float(t.minY),
							float(t.maxX - t.minX + 1), float(t.maxY - t.minY + 1)
						});
						UnloadImage(maskCells[idx]);
						maskCells[idx] = cropped;
					}
				}
			}
		}

		// Pass 2: pad to shared canvas and register textures.
		for (int row = 0; row < 8; ++row)
		{
			const int dir = kSheetRowToDir[row];
			for (int f = 0; f < frameCount; ++f)
			{
				const int srcFrame = f * frameStep;
				const int idx = row * frameCount + f;

				const std::string cname = colorName(row, srcFrame);
				if (!g_ResourceManager->DoesTextureExist(cname))
				{
					Image uniform = MakeUniformCell(colorCells[idx], maxW, maxH);
					g_ResourceManager->AddTexture(uniform, cname, false);
					UnloadImage(uniform);
				}
				UnloadImage(colorCells[idx]);
				colorCells[idx] = {};
				g_colorFrames[dir][f] = g_ResourceManager->GetTexture(cname, false);

				if (haveMask)
				{
					const std::string mname = maskName(row, srcFrame);
					if (!g_ResourceManager->DoesTextureExist(mname))
					{
						Image uniform = MakeUniformCell(maskCells[idx], maxW, maxH);
						g_ResourceManager->AddTexture(uniform, mname, false);
						UnloadImage(uniform);
					}
					UnloadImage(maskCells[idx]);
					maskCells[idx] = {};
					g_maskFrames[dir][f] = g_ResourceManager->GetTexture(mname, false);
				}
			}
		}

		UnloadImage(colorSheet);
		if (haveMask)
			UnloadImage(maskSheet);

		Log("WalkerSprites: loaded " + std::string(kFixedPath) +
			(haveMask ? std::string(" + ") + kMaskPath : "") +
			" (" + std::to_string(frameCount) + " of " + std::to_string(srcFrames) +
			" frames, " + std::to_string(cellW) + "x" + std::to_string(cellH) +
			" cells, uniform " + std::to_string(maxW) + "x" + std::to_string(maxH) + ")");
		return true;
	}

	int CameraRelativeDir(const Unit& unit, const Camera3D& camera)
	{
		Vector3 cameraAngle = Vector3Subtract(camera.position, camera.target);
		Vector3 cameraVector = Vector3{ cameraAngle.x, 0.0f, cameraAngle.z };
		const float camLen = Vector3Length(cameraVector);
		if (camLen > 0.0001f)
			cameraVector = Vector3Scale(cameraVector, 1.0f / camLen);
		else
			cameraVector = Vector3{ 0.0f, 0.0f, 1.0f };

		const float cameraAtan2 = atan2f(cameraVector.x, cameraVector.z);

		// m_Facing: 0=SW .. 7=S (same as U7 walk slots).
		static const Vector3 kFacingDir[8] = {
			{ -1, 0, 1 },  // SW
			{ -1, 0, 0 },  // W
			{ -1, 0, -1 }, // NW
			{ 0, 0, -1 },  // N
			{ 1, 0, -1 },  // NE
			{ 1, 0, 0 },   // E
			{ 1, 0, 1 },   // SE
			{ 0, 0, 1 },   // S
		};
		const int facing = (unit.m_Facing >= 0 && unit.m_Facing < 8) ? unit.m_Facing : 7;
		const Vector3 dir = kFacingDir[facing];
		const float unitAtan2 = atan2f(dir.x, dir.z);

		float angle = cameraAtan2 - unitAtan2;
		angle += (1.0f / 16.0f) * (2.0f * PI);
		while (angle < 0.0f) angle += (2.0f * PI);
		while (angle > (2.0f * PI)) angle -= (2.0f * PI);
		angle /= ((1.0f / 8.0f) * (2.0f * PI));
		return (static_cast<int>(angle) + 7) % 8;
	}
} // namespace

bool EnsureLoaded()
{
	if (g_loaded)
		return true;
	if (g_loadAttempted)
		return false;
	g_loadAttempted = true;

	if (!LoadWalkerSheets())
		return false;

	g_loaded = true;
	return true;
}

void Unload()
{
	g_colorFrames.clear();
	g_maskFrames.clear();
	if (g_shadowPlaneReady)
	{
		UnloadModel(g_shadowPlane);
		g_shadowPlane = {};
		g_shadowPlaneReady = false;
	}
	g_loaded = false;
	g_loadAttempted = false;
}

bool IsReady()
{
	return g_loaded
		&& g_colorFrames.size() >= 8
		&& !g_colorFrames[0].empty()
		&& g_colorFrames[0][0] != nullptr;
}

void DrawWalker(const Unit& unit, const Camera3D& camera, Color teamColor)
{
	if (!IsReady() && !EnsureLoaded())
		return;
	if (!IsReady())
		return;

	const int dir = CameraRelativeDir(unit, camera);
	const int frameCount = static_cast<int>(g_colorFrames[dir].size());
	if (frameCount <= 0 || g_colorFrames[dir][0] == nullptr)
		return;

	const bool moving = unit.m_HasTarget && unit.m_State == UnitState::Move;
	int frame = 0;
	if (moving)
		frame = (static_cast<int>(GetTime() * 1000.0) / kWalkFrameMs) % frameCount;

	Texture* colorTex = g_colorFrames[dir][frame];
	if (!colorTex)
		return;

	const float aspect = float(colorTex->width) / float(colorTex->height);
	const Vector2 size{ kWorldHeight * aspect * kWidthScale, kWorldHeight };
	Vector3 pos = unit.m_Pos;
	pos.y += size.y * 0.5f;

	const Rectangle src{ 0, 0, float(colorTex->width), float(colorTex->height) };

	// Drop shadow on the ground (U7-style flat quad + dropshadow.png).
	if (EnsureShadowPlane())
	{
		Texture* shadowTex = g_ResourceManager->GetTexture(kShadowPath, false);
		if (shadowTex)
		{
			SetMaterialTexture(&g_shadowPlane.materials[0], MATERIAL_MAP_DIFFUSE, *shadowTex);
			const float shadowW = size.x * kShadowScale;
			const float shadowD = shadowW * 0.7f;
			const Vector3 shadowPos{ unit.m_Pos.x, unit.m_Pos.y + 0.02f, unit.m_Pos.z };
			rlDisableDepthMask();
			DrawModelEx(g_shadowPlane, shadowPos, Vector3{ 0, 1, 0 }, 0.0f,
				Vector3{ shadowW, 1.0f, shadowD }, BLACK);
			rlEnableDepthMask();
		}
	}

	// Alpha discard so transparent texels write no depth and don't blend through
	// overlapping walkers / village cuboids (same fix as U7Revisited).
	const bool useCutout = (g_alphaDiscard.id != 0);
	if (useCutout)
		BeginShaderMode(g_alphaDiscard);

	DrawBillboardPro(camera, *colorTex, src, pos, Vector3{ 0, 1, 0 },
		size, Vector2{ 0, 0 }, 0.0f, WHITE);

	if (g_maskFrames.size() >= 8
		&& static_cast<int>(g_maskFrames[dir].size()) > frame
		&& g_maskFrames[dir][frame] != nullptr)
	{
		Texture* maskTex = g_maskFrames[dir][frame];
		const Rectangle maskSrc{ 0, 0, float(maskTex->width), float(maskTex->height) };
		DrawBillboardPro(camera, *maskTex, maskSrc, pos, Vector3{ 0, 1, 0 },
			size, Vector2{ 0, 0 }, 0.0f, teamColor);
	}

	if (useCutout)
		EndShaderMode();
}

} // namespace WalkerSprites
