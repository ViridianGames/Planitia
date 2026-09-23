#include <algorithm>
#include <iosfwd>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

#include "Geist/Globals.h"

#include "GameGlobals.h"

#include "Engine.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

using namespace std;

namespace
{
	// Local GetScreenToWorldRayEx - Planitia's bundled raylib only has GetMouseRay
	// (which uses window size, wrong for the virtual render target).
	Ray ScreenToWorldRayEx(Vector2 position, Camera camera, int width, int height)
	{
		Ray ray{};

		const float x = (2.0f * position.x) / static_cast<float>(width) - 1.0f;
		const float y = 1.0f - (2.0f * position.y) / static_cast<float>(height);
		const Vector3 deviceCoords{ x, y, 1.0f };

		const Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);
		Matrix matProj = MatrixIdentity();

		if (camera.projection == CAMERA_PERSPECTIVE)
		{
			matProj = MatrixPerspective(
				camera.fovy * DEG2RAD,
				static_cast<double>(width) / static_cast<double>(height),
				RL_CULL_DISTANCE_NEAR,
				RL_CULL_DISTANCE_FAR);
		}
		else if (camera.projection == CAMERA_ORTHOGRAPHIC)
		{
			const double aspect = static_cast<double>(width) / static_cast<double>(height);
			const double top = camera.fovy / 2.0;
			const double right = top * aspect;
			matProj = MatrixOrtho(-right, right, -top, top, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
		}

		const Vector3 nearPoint = Vector3Unproject(
			Vector3{ deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
		const Vector3 farPoint = Vector3Unproject(
			Vector3{ deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);
		const Vector3 cameraPlanePointerPos = Vector3Unproject(
			Vector3{ deviceCoords.x, deviceCoords.y, -1.0f }, matProj, matView);

		const Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

		if (camera.projection == CAMERA_PERSPECTIVE)
			ray.position = camera.position;
		else if (camera.projection == CAMERA_ORTHOGRAPHIC)
			ray.position = cameraPlanePointerPos;

		ray.direction = direction;
		return ray;
	}
}

void DrawOutlinedText(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float fontSize, int spacing, Color color)
{
	DrawTextEx(*font, text.c_str(), Vector2{ position.x + 1, position.y + 1 }, fontSize, spacing, Color{ 0, 0, 0, color.a });
	DrawTextEx(*font, text.c_str(), Vector2{ position.x, position.y + 1 }, fontSize, spacing, Color{ 0, 0, 0, color.a });
	DrawTextEx(*font, text.c_str(), Vector2{ position.x - 1, position.y - 1 }, fontSize, spacing, Color{ 0, 0, 0, color.a });
	DrawTextEx(*font, text.c_str(), Vector2{ position.x, position.y - 1 }, fontSize, spacing, Color{ 0, 0, 0, color.a });
	DrawTextEx(*font, text.c_str(), Vector2{ position.x + 1, position.y - 1 }, fontSize, spacing, Color{ 0, 0, 0, color.a });
	DrawTextEx(*font, text.c_str(), Vector2{ position.x + 1, position.y }, fontSize, spacing, Color{ 0, 0, 0, color.a });
	DrawTextEx(*font, text.c_str(), Vector2{ position.x - 1, position.y + 1 }, fontSize, spacing, Color{ 0, 0, 0, color.a });
	DrawTextEx(*font, text.c_str(), Vector2{ position.x - 1, position.y }, fontSize, spacing, Color{ 0, 0, 0, color.a });
	DrawTextEx(*font, text.c_str(), position, fontSize, spacing, color);
}

void DrawParagraph(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float maxwidth, float fontSize, int spacing, Color color, bool outlined)
{
	std::istringstream iss(text);
	std::string word;
	std::vector<std::string> lines;
	float lineWidth = 0;

	string rawline;
	string line;
	while (getline(iss, rawline))
	{
		std::stringstream lineStream(rawline);
		while (lineStream >> word)
		{
			int currentLineWidth = MeasureTextEx(*font, (line + word).c_str(), fontSize, spacing).x;
			if (currentLineWidth > maxwidth)
			{
				lines.push_back(line);
				line.clear();
				line += word + " ";
			}
			else
			{
				line += word + " ";
			}
		}

		lines.push_back(line);

		line.clear();
	}

	auto it = lines.begin();
	float y = position.y;
	while (it != lines.end())
	{
		if (outlined)
		{
			DrawOutlinedText(font, (*it).c_str(), Vector2{ position.x, y }, fontSize, spacing, color);
		}
		else
		{
			DrawTextEx(*font, (*it).c_str(), Vector2{ position.x, y }, fontSize, spacing, color);
		}
		y += fontSize * 1.2f;
		++it;
	}
}

std::vector<ConsoleString> g_ConsoleStrings;

void AddConsoleString(const std::string& text, Color color)
{
	ConsoleString entry;
	entry.m_String = text;
	entry.m_Color = color;
	entry.m_StartTime = static_cast<unsigned int>(GetTime());
	g_ConsoleStrings.push_back(entry);
	if (g_ConsoleStrings.size() > 12)
		g_ConsoleStrings.erase(g_ConsoleStrings.begin());
}

void DrawConsole()
{
	if (!g_Engine || !g_smallFont)
		return;

	// Bottom-left, stacked upward above the F3 perf panel (~20% of screen height).
	const float fs = static_cast<float>(g_smallFont->baseSize);
	const float lineH = fs + 2.0f;
	const float perfH = g_showPerfCounter ? (g_Engine->m_RenderHeight * 0.20f) : 0.0f;
	const float bottom = static_cast<float>(g_Engine->m_RenderHeight) - perfH - 4.0f;

	int visible = 0;
	for (auto& node : g_ConsoleStrings)
	{
		(void)node;
		++visible;
	}
	// Cap how many lines we draw so we never climb into the help text.
	const int maxLines = std::max(1, static_cast<int>((bottom - 40.0f) / lineH));
	const int start = std::max(0, visible - maxLines);

	int drawn = 0;
	for (size_t i = static_cast<size_t>(start); i < g_ConsoleStrings.size(); ++i)
	{
		auto& node = g_ConsoleStrings[i];
		float elapsed = GetTime() - node.m_StartTime;
		Color c = node.m_Color;
		if (elapsed > 9.0f)
		{
			float alpha = float(10.0 - elapsed);
			if (alpha < 0.0f) alpha = 0.0f;
			c.a = static_cast<unsigned char>(alpha * 255.0f);
		}
		if (elapsed < 10.0f)
		{
			const float y = bottom - lineH * static_cast<float>(drawn + 1);
			DrawOutlinedText(g_smallFont, node.m_String.c_str(), Vector2{ 4.0f, y }, fs, 1, c);
			++drawn;
		}
	}

	for (auto it = g_ConsoleStrings.begin(); it != g_ConsoleStrings.end();)
	{
		if (GetTime() - it->m_StartTime > 10)
			it = g_ConsoleStrings.erase(it);
		else
			++it;
	}
}

void DrawPerfCounter(Font* font, int loc)
{
	if (!font || !g_Engine)
		return;

	const int width = static_cast<int>(g_Engine->m_RenderWidth * 0.20f);
	const int height = static_cast<int>(g_Engine->m_RenderHeight * 0.20f);
	int hpos = 0;
	int vpos = 0;
	switch (loc)
	{
	case 0: // Bottom-left
		hpos = 0;
		vpos = g_Engine->m_RenderHeight - height;
		break;
	case 1: // Top-left
		hpos = 0;
		vpos = 0;
		break;
	case 2: // Bottom-right
		hpos = g_Engine->m_RenderWidth - width;
		vpos = g_Engine->m_RenderHeight - height;
		break;
	case 3: // Top-right
		hpos = g_Engine->m_RenderWidth - width;
		vpos = 0;
		break;
	default:
		break;
	}

	DrawRectangle(hpos, vpos, width, height, Color{ 0, 0, 0, 200 });
	DrawRectangleLines(hpos, vpos, width, height, BLUE);

	const float frameSec = g_Engine->LastFrameInSeconds();
	const int fps = (frameSec > 0.0001f) ? static_cast<int>(1.0f / frameSec) : 0;
	const int mspf = static_cast<int>(frameSec * 1000.0f);
	const int netNow = static_cast<int>(g_Engine->m_NetworkFrames[49]);
	std::string perfTemp = std::to_string(fps) + " fps (" + std::to_string(mspf) + " mspf)";
	if (netNow > 0)
		perfTemp += "  net " + std::to_string(netNow) + "ms";
	const float fontSize = static_cast<float>(font->baseSize);
	DrawTextEx(*font, perfTemp.c_str(),
		{ hpos + width * 0.05f, static_cast<float>(vpos + height) - fontSize * 1.1f },
		fontSize, 1, WHITE);

	// Legend (panel-relative — U7 incorrectly pinned this to full-screen Y).
	const float legendY = static_cast<float>(vpos + height) - fontSize * 2.2f;
	DrawTextEx(*font, "Update", { hpos + width * 0.05f, legendY }, fontSize, 1, YELLOW);
	DrawTextEx(*font, "Draw", { hpos + width * 0.35f, legendY }, fontSize, 1, GREEN);
	DrawTextEx(*font, "Network", { hpos + width * 0.60f, legendY }, fontSize, 1, SKYBLUE);

	// Fixed 33ms budget so bar heights are comparable across frames (autoscaling
	// made a 4ms frame look as tall as a 40ms stall).
	constexpr float kBudgetMs = 33.0f;
	const int baseline = vpos + height - static_cast<int>(fontSize * 2.4f);
	const int maxBar = std::max(8, baseline - vpos - 4);
	const float pxPerMs = static_cast<float>(maxBar) / kBudgetMs;

	// 16.7ms (60fps) and 33ms (sim tick) guides.
	const int y16 = baseline - static_cast<int>(16.7f * pxPerMs);
	const int y33 = baseline - maxBar;
	DrawLine(hpos + 2, y16, hpos + width - 2, y16, Color{ 80, 80, 80, 180 });
	DrawLine(hpos + 2, y33, hpos + width - 2, y33, Color{ 100, 60, 60, 180 });

	const int barSlots = std::min(50, (width - 8) / 2);
	for (int i = 0; i < barSlots; ++i)
	{
		// Oldest on the left: map slot i -> history index.
		const int hist = 50 - barSlots + i;
		const int rawUpdate = std::max(0, static_cast<int>(g_Engine->m_UpdateFrames[hist]));
		const int rawNet = std::max(0, static_cast<int>(g_Engine->m_NetworkFrames[hist]));
		const int rawDraw = std::max(0, static_cast<int>(g_Engine->m_DrawFrames[hist]));

		int updatePx = static_cast<int>(rawUpdate * pxPerMs);
		int netPx = static_cast<int>(rawNet * pxPerMs);
		int drawPx = static_cast<int>(rawDraw * pxPerMs);
		const int total = updatePx + netPx + drawPx;
		if (total > maxBar && total > 0)
		{
			const float scale = static_cast<float>(maxBar) / static_cast<float>(total);
			updatePx = static_cast<int>(updatePx * scale);
			netPx = static_cast<int>(netPx * scale);
			drawPx = static_cast<int>(drawPx * scale);
		}

		const int x = hpos + 4 + (i * 2);
		int y = baseline;
		if (updatePx > 0)
		{
			DrawRectangle(x, y - updatePx, 2, updatePx, YELLOW);
			y -= updatePx;
		}
		if (netPx > 0)
		{
			DrawRectangle(x, y - netPx, 2, netPx, SKYBLUE);
			y -= netPx;
		}
		if (drawPx > 0)
			DrawRectangle(x, y - drawPx, 2, drawPx, GREEN);
	}
}

Vector2 GetScaledMousePosition()
{
	Vector2 mouse = GetMousePosition();
	// Map window pixels -> virtual render pixels using the same stretch as DrawTexturePro.
	const Vector2 scale = g_Engine->GetInputScaleXY();
	if (scale.x != 0.0f)
	{
		mouse.x /= scale.x;
	}
	if (scale.y != 0.0f)
	{
		mouse.y /= scale.y;
	}
	return mouse;
}

Ray GetTerrainMouseRay(const Camera3D& camera)
{
	// Unproject in full virtual-render pixel space so the ray matches BeginMode3D's
	// projection (built from the render-target framebuffer aspect).
	const Vector2 mouse = GetScaledMousePosition();
	const int width = g_Engine ? static_cast<int>(g_Engine->m_RenderWidth) : GetScreenWidth();
	const int height = g_Engine ? static_cast<int>(g_Engine->m_RenderHeight) : GetScreenHeight();
	return ScreenToWorldRayEx(mouse, camera, width, height);
}