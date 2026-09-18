#include <string>
#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/StateMachine.h"

#include "GameGlobals.h"

using namespace std;

void TitleState::Init(const std::string& /*configfile*/)
{
	m_DrawCursor = true;
}

void TitleState::Shutdown()
{
}

void TitleState::OnEnter()
{
}

void TitleState::OnExit()
{
}

void TitleState::Update()
{
	if (IsKeyPressed(KEY_ESCAPE))
	{
		g_Engine->m_Done = true;
		return;
	}

	// Local skirmish
	if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_N))
	{
		g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
		return;
	}

	// Multiplayer lobby
	if (IsKeyPressed(KEY_M))
	{
		g_StateMachine->MakeStateTransition(STATE_MULTIPLAYERMENUSTATE);
		return;
	}
}

void TitleState::Draw()
{
	DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, Color{ 20, 40, 80, 255 });

	float y = 8.0f;
	if (g_font)
	{
		DrawOutlinedText(g_font, "Planitia", { 8.0f, y }, static_cast<float>(g_font->baseSize), 1, WHITE);
		y += static_cast<float>(g_font->baseSize) + 6.0f;
	}
	if (g_smallFont)
	{
		const float fs = static_cast<float>(g_smallFont->baseSize);
		DrawOutlinedText(g_smallFont, "Space/Enter/N - local skirmish", { 8.0f, y }, fs, 1, Color{ 200, 210, 230, 255 });
		y += fs + 3.0f;
		DrawOutlinedText(g_smallFont, "M - multiplayer lobby", { 8.0f, y }, fs, 1, Color{ 200, 210, 230, 255 });
		y += fs + 3.0f;
		DrawOutlinedText(g_smallFont, "Esc - quit", { 8.0f, y }, fs, 1, Color{ 200, 210, 230, 255 });
	}
}
