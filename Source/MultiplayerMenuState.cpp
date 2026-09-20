#include "MultiplayerMenuState.h"

#include "GameGlobals.h"
#include "GameSim.h"
#include "LanAddress.h"
#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/Logging.h"
#include "Geist/StateMachine.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define planitia_getpid _getpid
#else
#include <unistd.h>
#define planitia_getpid getpid
#endif

namespace
{
	uint16_t CfgPort()
	{
		return static_cast<uint16_t>(
			g_Engine ? std::max(1, static_cast<int>(g_Engine->m_EngineConfig.GetNumber("port"))) : 43000);
	}

	uint16_t CfgTurnLength()
	{
		return static_cast<uint16_t>(
			g_Engine ? std::max(1, static_cast<int>(g_Engine->m_EngineConfig.GetNumber("turn_length"))) : 1);
	}

	Rectangle Btn(float x, float y, float w, float h)
	{
		return Rectangle{ x, y, w, h };
	}

	void DrawBtn(const Rectangle& r, const char* label, bool enabled = true)
	{
		DrawRectangleRec(r, enabled ? Color{ 40, 55, 80, 255 } : Color{ 30, 35, 45, 255 });
		DrawRectangleLinesEx(r, 1.0f, enabled ? Color{ 120, 150, 200, 255 } : Color{ 70, 75, 85, 255 });
		if (g_smallFont)
		{
			DrawOutlinedText(
				g_smallFont,
				label,
				{ r.x + 6.0f, r.y + 5.0f },
				static_cast<float>(g_smallFont->baseSize),
				1,
				enabled ? WHITE : Color{ 120, 120, 130, 255 });
		}
	}

	std::string Trim(std::string s)
	{
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
			s.erase(s.begin());
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
			s.pop_back();
		return s;
	}

	// Six lobby actions stacked bottom-right (narrower so they clear the status text).
	struct LobbyButtons
	{
		Rectangle host{};
		Rectangle copy{};
		Rectangle paste{};
		Rectangle join{};
		Rectangle start{};
		Rectangle back{};
		float btnW = 0.0f;
		float btnH = 0.0f;
	};

	LobbyButtons LayoutLobbyButtons()
	{
		LobbyButtons out{};
		const float fs = g_smallFont ? static_cast<float>(g_smallFont->baseSize) : 8.0f;
		const float renderW = g_Engine ? static_cast<float>(g_Engine->m_RenderWidth) : 320.0f;
		const float renderH = g_Engine ? static_cast<float>(g_Engine->m_RenderHeight) : 200.0f;
		constexpr float kMargin = 8.0f;
		constexpr float kGap = 4.0f;
		constexpr int kCount = 6;

		// ~2/3 of the old centered panel width (was min(240, 60% screen)).
		out.btnW = std::min(160.0f, renderW * 0.40f);
		out.btnH = fs + 10.0f;
		const float stackH = static_cast<float>(kCount) * out.btnH + static_cast<float>(kCount - 1) * kGap;
		const float x = renderW - kMargin - out.btnW;
		float y = renderH - kMargin - stackH;

		out.host = Btn(x, y, out.btnW, out.btnH); y += out.btnH + kGap;
		out.copy = Btn(x, y, out.btnW, out.btnH); y += out.btnH + kGap;
		out.paste = Btn(x, y, out.btnW, out.btnH); y += out.btnH + kGap;
		out.join = Btn(x, y, out.btnW, out.btnH); y += out.btnH + kGap;
		out.start = Btn(x, y, out.btnW, out.btnH); y += out.btnH + kGap;
		out.back = Btn(x, y, out.btnW, out.btnH);
		return out;
	}
}

void MultiplayerMenuState::Init(const std::string& /*configfile*/)
{
	m_DrawCursor = true;
	m_JoinPort = CfgPort();
}

void MultiplayerMenuState::Shutdown()
{
}

void MultiplayerMenuState::RefreshHostInvite()
{
	m_LocalIps = EnumerateLocalIPv4();
	const uint16_t port = g_Net.IsOnline() ? g_Net.Port() : CfgPort();
	if (!m_LocalIps.empty())
		m_HostInvite = m_LocalIps.front() + ":" + std::to_string(port);
	else
		m_HostInvite = "127.0.0.1:" + std::to_string(port);

	m_PublicInvite.clear();
	if (g_Net.GetMode() == NetSession::Mode::Host)
		m_PublicInvite = g_Net.PublicInvite();
}

std::string MultiplayerMenuState::CurrentInvite() const
{
	// Prefer public (internet) invite when UPnP worked; else LAN.
	if (g_Net.GetMode() == NetSession::Mode::Host)
	{
		if (!m_PublicInvite.empty())
			return m_PublicInvite;
		if (!m_HostInvite.empty())
			return m_HostInvite;
	}
	return m_JoinIp + ":" + std::to_string(m_JoinPort);
}

bool MultiplayerMenuState::ParseInvite(const std::string& text, std::string& outIp, uint16_t& outPort) const
{
	std::string s = Trim(text);
	if (s.empty())
		return false;

	// Accept optional prefixes: planitia:// or planitia:
	const std::string prefix1 = "planitia://";
	const std::string prefix2 = "planitia:";
	if (s.rfind(prefix1, 0) == 0)
		s = s.substr(prefix1.size());
	else if (s.rfind(prefix2, 0) == 0)
		s = s.substr(prefix2.size());
	s = Trim(s);

	// Strip path/query if someone pasted a URL-ish string.
	const auto slash = s.find('/');
	if (slash != std::string::npos)
		s = s.substr(0, slash);

	std::string ip = s;
	uint16_t port = CfgPort();
	const auto colon = s.rfind(':');
	if (colon != std::string::npos)
	{
		ip = s.substr(0, colon);
		const std::string portStr = s.substr(colon + 1);
		int p = 0;
		try { p = std::stoi(portStr); }
		catch (...) { return false; }
		if (p < 1 || p > 65535)
			return false;
		port = static_cast<uint16_t>(p);
	}

	ip = Trim(ip);
	if (ip.empty())
		return false;

	// Very light validation: digits/dots or hostname chars.
	for (unsigned char c : ip)
	{
		if (std::isalnum(c) || c == '.' || c == '-')
			continue;
		return false;
	}

	outIp = ip;
	outPort = port;
	return true;
}

void MultiplayerMenuState::OnEnter()
{
	m_TransitionToMain = false;
	m_JoinPort = CfgPort();
	g_Net.Disconnect();
	g_Lockstep.ResetOffline(0);
	g_Lockstep.PauseMatch("Multiplayer lobby");
	RefreshHostInvite();
	AddConsoleString("Multiplayer lobby - Host or Join (" + Net::VersionLabel() + ")", Color{ 200, 210, 230, 255 });
	AddConsoleString("Share invite ip:port (Copy). Friend pastes then Join.", Color{ 180, 195, 220, 255 });
	AddConsoleString("All players must be on " + Net::VersionLabel(), Color{ 180, 195, 220, 255 });
}

void MultiplayerMenuState::OnExit()
{
}

void MultiplayerMenuState::Update()
{
	if (IsKeyPressed(KEY_ESCAPE))
	{
		g_Net.Disconnect();
		g_Lockstep.ResetOffline(0);
		g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
		return;
	}

	// Ctrl+C / Ctrl+V shortcuts while in lobby.
	const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
	if (ctrl && IsKeyPressed(KEY_C) && g_Net.GetMode() == NetSession::Mode::Host)
	{
		RefreshHostInvite();
		SetClipboardText(m_HostInvite.c_str());
		AddConsoleString("Copied invite: " + m_HostInvite, GREEN);
	}
	if (ctrl && IsKeyPressed(KEY_V))
	{
		const char* clip = GetClipboardText();
		if (clip)
		{
			std::string ip;
			uint16_t port = CfgPort();
			if (ParseInvite(clip, ip, port))
			{
				m_JoinIp = ip;
				m_JoinPort = port;
				AddConsoleString("Pasted invite: " + m_JoinIp + ":" + std::to_string(m_JoinPort), GREEN);
			}
			else
				AddConsoleString("Clipboard is not a valid ip:port invite", RED);
		}
	}

	g_Net.SetPeerHandlers(
		[](int /*peerIndex*/)
		{
			// Name comes with Hello; Lockstep logs the real join.
			AddConsoleString("Incoming connection...", Color{ 200, 210, 230, 255 });
		},
		[](int peerIndex)
		{
			const int slot = static_cast<int>(g_Lockstep.AssignedSlotForPeer(peerIndex));
			std::string name = g_Lockstep.PeerNameForSlot(slot);
			g_Lockstep.OnPeerDisconnectedAsHost(peerIndex);
			if (name.empty())
				name = "Player";
			AddConsoleString(name + " left the lobby (" +
				std::to_string(g_Net.ConnectedPeerCount()) + "/" +
				std::to_string(kMaxPlayers - 1) + " connected)", YELLOW);
		});

	g_Net.Service([&](Net::PacketType type, const uint8_t* data, size_t size, int peerIndex)
	{
		const int occupiedBefore = g_Lockstep.OccupiedRemoteSlots();
		g_Lockstep.OnNetworkPacket(type, data, size, peerIndex, g_Net);
		if (type == Net::PacketType::Hello && g_Net.GetMode() == NetSession::Mode::Host)
		{
			const int occupiedAfter = g_Lockstep.OccupiedRemoteSlots();
			if (occupiedAfter > occupiedBefore)
			{
				const int slot = static_cast<int>(g_Lockstep.AssignedSlotForPeer(peerIndex));
				const std::string name = g_Lockstep.PeerNameForSlot(slot);
				const PlayerColorId color = g_Lockstep.ColorForSlot(slot);
				AddConsoleString(
					(name.empty() ? "Player" : name) + " joined as " + PlayerColorName(color)
					+ " (slot " + std::to_string(slot) + ", "
					+ std::to_string(occupiedAfter) + "/" + std::to_string(kMaxPlayers - 1) + " connected)",
					PlayerColorRgb(color));
			}
			else if (occupiedAfter == occupiedBefore && occupiedAfter >= kMaxPlayers - 1)
			{
				AddConsoleString("Lobby full - could not seat player", RED);
			}
		}
	});
	g_Lockstep.Update(g_Net, g_Sim);

	std::string myColor;
	if (g_Lockstep.TakePendingColorNotify(myColor))
		AddConsoleString("You are playing as " + myColor, PlayerColorRgb(g_Lockstep.ColorForSlot(g_Lockstep.LocalSlot())));

	std::string netErr;
	if (g_Lockstep.TakePendingNetError(netErr))
		AddConsoleString(netErr, RED);

	if (g_Lockstep.MatchRunning() && g_Net.GetMode() == NetSession::Mode::Client)
	{
		AddConsoleString("Host started the match - entering game", GREEN);
		g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
		return;
	}

	if (m_TransitionToMain)
	{
		m_TransitionToMain = false;
		g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
		return;
	}

	if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !g_Engine)
		return;

	const Vector2 mouse = GetScaledMousePosition();
	const LobbyButtons btns = LayoutLobbyButtons();

	const uint16_t port = CfgPort();
	const uint16_t turnLen = CfgTurnLength();

	if (CheckCollisionPointRec(mouse, btns.host))
	{
		if (g_Net.Host(port))
		{
			const int pid = static_cast<int>(planitia_getpid());
			const std::string hostLog = "runlog_host_" + std::to_string(pid) + ".txt";
			Log("Switching log file -> " + hostLog);
			SetLogFileName(hostLog);
			Log("Host log started (pid " + std::to_string(pid) + ", port " + std::to_string(port) + ")");
			g_Lockstep.PauseMatch("Hosting - waiting for client, then Start Match");
			g_Lockstep.ConfigureMatch(1, 0, turnLen);
			RefreshHostInvite();
			const std::string invite = CurrentInvite();
			SetClipboardText(invite.c_str());
			AddConsoleString("Hosting on port " + std::to_string(port), GREEN);
			if (g_Net.UpnpActive() && !m_PublicInvite.empty())
			{
				AddConsoleString("UPnP OK - public invite copied: " + m_PublicInvite, GREEN);
				AddConsoleString("LAN invite: " + m_HostInvite, Color{ 180, 200, 220, 255 });
			}
			else if (g_Net.UpnpActive())
			{
				AddConsoleString("UPnP mapped but no public IP - copied LAN invite", YELLOW);
				AddConsoleString("Invite: " + m_HostInvite, GREEN);
			}
			else
			{
				const std::string& upnpErr = g_Net.UpnpError();
				AddConsoleString("UPnP failed - copied LAN invite only", YELLOW);
				if (!upnpErr.empty())
					AddConsoleString("UPnP reason: " + upnpErr, Color{ 220, 180, 100, 255 });
				AddConsoleString("Internet friends need manual UDP port forward to this PC", YELLOW);
				AddConsoleString("Invite: " + m_HostInvite, GREEN);
			}
			AddConsoleString("Log: " + hostLog, Color{ 180, 200, 220, 255 });
		}
		else
			AddConsoleString("Host failed: " + g_Net.LastError(), RED);
		return;
	}

	if (CheckCollisionPointRec(mouse, btns.copy))
	{
		if (g_Net.GetMode() != NetSession::Mode::Host)
		{
			AddConsoleString("Host a game first to copy an invite", YELLOW);
			return;
		}
		RefreshHostInvite();
		const std::string invite = CurrentInvite();
		SetClipboardText(invite.c_str());
		AddConsoleString("Copied invite: " + invite, GREEN);
		return;
	}

	if (CheckCollisionPointRec(mouse, btns.paste))
	{
		const char* clip = GetClipboardText();
		if (!clip || !*clip)
		{
			AddConsoleString("Clipboard empty", YELLOW);
			return;
		}
		std::string ip;
		uint16_t p = CfgPort();
		if (!ParseInvite(clip, ip, p))
		{
			AddConsoleString("Clipboard is not a valid ip:port invite", RED);
			return;
		}
		m_JoinIp = ip;
		m_JoinPort = p;
		AddConsoleString("Pasted invite: " + m_JoinIp + ":" + std::to_string(m_JoinPort), GREEN);
		return;
	}

	if (CheckCollisionPointRec(mouse, btns.join))
	{
		if (g_Net.Join(m_JoinIp, m_JoinPort))
		{
			const int pid = static_cast<int>(planitia_getpid());
			const std::string clientLog = "runlog_client_" + std::to_string(pid) + ".txt";
			Log("Switching log file -> " + clientLog);
			SetLogFileName(clientLog);
			Log("Client log started (pid " + std::to_string(pid)
				+ ", joining " + m_JoinIp + ":" + std::to_string(m_JoinPort) + ")");
			g_Lockstep.PauseMatch("Client - waiting for host to Start Match");
			AddConsoleString("Joining " + m_JoinIp + ":" + std::to_string(m_JoinPort), YELLOW);
			AddConsoleString("Log: " + clientLog, Color{ 180, 200, 220, 255 });
		}
		else
			AddConsoleString("Join failed: " + g_Net.LastError(), RED);
		return;
	}

	if (CheckCollisionPointRec(mouse, btns.start))
	{
		if (g_Net.GetMode() != NetSession::Mode::Host)
		{
			AddConsoleString("Only the host can Start Match", YELLOW);
			return;
		}
		const int peers = g_Net.ConnectedPeerCount();
		if (peers < 1)
		{
			AddConsoleString("Wait for at least one player to join", YELLOW);
			return;
		}
		if (peers > kMaxPlayers - 1)
		{
			AddConsoleString("Too many peers", RED);
			return;
		}
		const uint32_t seed = static_cast<uint32_t>(GetTime() * 1000.0);
		Log("UI: Start Match clicked peers=" + std::to_string(peers) + " seed=" + std::to_string(seed));
		g_Lockstep.ConfigureMatch(peers + 1, 0, turnLen);
		g_Lockstep.HostBroadcastStart(g_Net, seed);
		g_Lockstep.BeginMatch(g_Sim, seed, peers + 1, 0, turnLen, peers + 1);
		AddConsoleString("Match started (" + std::to_string(peers + 1) + " players)", GREEN);
		m_TransitionToMain = true;
		return;
	}

	if (CheckCollisionPointRec(mouse, btns.back))
	{
		g_Net.Disconnect();
		g_Lockstep.ResetOffline(0);
		g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
	}
}

void MultiplayerMenuState::Draw()
{
	ClearBackground(Color{ 16, 28, 48, 255 });

	const float fs = g_smallFont ? static_cast<float>(g_smallFont->baseSize) : 8.0f;
	float y = 8.0f;

	if (g_font)
	{
		DrawOutlinedText(g_font, "Multiplayer", { 8.0f, y }, static_cast<float>(g_font->baseSize), 1, WHITE);
		y += static_cast<float>(g_font->baseSize) + 6.0f;
	}

	if (g_smallFont)
	{
		DrawOutlinedText(g_smallFont, TextFormat("Build %s - all players must match", Net::VersionLabel().c_str()),
			{ 8.0f, y }, fs, 1, Color{ 160, 200, 160, 255 });
		y += fs + 4.0f;
		DrawOutlinedText(g_smallFont, "No Steam/GOG needed. Share invite, friend pastes + Join.",
			{ 8.0f, y }, fs, 1, Color{ 180, 195, 220, 255 });
		y += fs + 8.0f;

		const char* modeStr = "Offline";
		if (g_Net.GetMode() == NetSession::Mode::Host) modeStr = "HOST";
		else if (g_Net.GetMode() == NetSession::Mode::Client) modeStr = "CLIENT";

		DrawOutlinedText(g_smallFont, TextFormat("Mode: %s", modeStr), { 8.0f, y }, fs, 1, Color{ 200, 210, 230, 255 });
		y += fs + 2.0f;
		DrawOutlinedText(g_smallFont, g_Net.Status().c_str(), { 8.0f, y }, fs, 1, Color{ 200, 210, 230, 255 });
		y += fs + 2.0f;
		DrawOutlinedText(g_smallFont, g_Lockstep.Status().c_str(), { 8.0f, y }, fs, 1,
			g_Lockstep.IsDesynced() ? RED : Color{ 200, 210, 230, 255 });
		y += fs + 2.0f;

		if (g_Net.GetMode() == NetSession::Mode::Host)
		{
			if (!m_PublicInvite.empty())
			{
				DrawOutlinedText(g_smallFont, TextFormat("Public invite: %s", m_PublicInvite.c_str()),
					{ 8.0f, y }, fs, 1, Color{ 120, 220, 140, 255 });
				y += fs + 2.0f;
				DrawOutlinedText(g_smallFont, TextFormat("LAN invite: %s", m_HostInvite.c_str()),
					{ 8.0f, y }, fs, 1, Color{ 160, 180, 160, 255 });
				y += fs + 2.0f;
			}
			else
			{
				DrawOutlinedText(g_smallFont, TextFormat("LAN invite: %s", m_HostInvite.c_str()),
					{ 8.0f, y }, fs, 1, Color{ 120, 220, 140, 255 });
				y += fs + 2.0f;
				DrawOutlinedText(g_smallFont, "UPnP unavailable - internet needs port forward",
					{ 8.0f, y }, fs, 1, Color{ 220, 180, 100, 255 });
				y += fs + 2.0f;
				if (!g_Net.UpnpError().empty())
				{
					DrawOutlinedText(g_smallFont, TextFormat("UPnP: %s", g_Net.UpnpError().c_str()),
						{ 8.0f, y }, fs, 1, Color{ 220, 160, 100, 255 });
					y += fs + 2.0f;
				}
			}
		}
		else
		{
			DrawOutlinedText(g_smallFont,
				TextFormat("Join target: %s:%d | Peers %d | Slot %d",
					m_JoinIp.c_str(), static_cast<int>(m_JoinPort),
					g_Net.ConnectedPeerCount(), g_Lockstep.LocalSlot()),
				{ 8.0f, y }, fs, 1, Color{ 200, 210, 230, 255 });
			y += fs + 2.0f;
		}

		// Seat list under status (left) so it stays clear of the bottom-right buttons.
		const bool hosting = g_Net.GetMode() == NetSession::Mode::Host;
		if (hosting)
		{
			y += 6.0f;
			DrawOutlinedText(g_smallFont, TextFormat("Lobby seats (%d/4):", 1 + g_Lockstep.OccupiedRemoteSlots()),
				{ 8.0f, y }, fs, 1, WHITE);
			y += fs + 2.0f;
			DrawOutlinedText(g_smallFont, TextFormat("1. Host (you) - %s", PlayerColorName(PlayerColorId::Green)),
				{ 8.0f, y }, fs, 1, PlayerColorRgb(PlayerColorId::Green));
			y += fs + 2.0f;
			for (int slot = 1; slot < kMaxPlayers; ++slot)
			{
				const std::string name = g_Lockstep.PeerNameForSlot(slot);
				if (name.empty())
				{
					DrawOutlinedText(g_smallFont, TextFormat("%d. (open)", slot + 1),
						{ 8.0f, y }, fs, 1, Color{ 140, 150, 160, 255 });
				}
				else
				{
					const PlayerColorId color = g_Lockstep.ColorForSlot(slot);
					DrawOutlinedText(g_smallFont,
						TextFormat("%d. %s - %s", slot + 1, name.c_str(), PlayerColorName(color)),
						{ 8.0f, y }, fs, 1, PlayerColorRgb(color));
				}
				y += fs + 2.0f;
			}
		}
	}

	const LobbyButtons btns = LayoutLobbyButtons();
	const bool hosting = g_Net.GetMode() == NetSession::Mode::Host;
	const bool canStart = hosting && g_Net.ConnectedPeerCount() >= 1;

	DrawBtn(btns.host, "Host LAN/IP");
	DrawBtn(btns.copy, "Copy Invite (Ctrl+C)", hosting);
	DrawBtn(btns.paste, "Paste Invite (Ctrl+V)");
	DrawBtn(btns.join, TextFormat("Join %s:%d", m_JoinIp.c_str(), static_cast<int>(m_JoinPort)));
	DrawBtn(btns.start,
		canStart ? "Start Match (host)" : "Start Match (need 1+ player)",
		canStart);
	DrawBtn(btns.back, "Back to Title");

	DrawConsole();
}
