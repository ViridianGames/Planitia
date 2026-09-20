#include "NetSession.h"

#include "Geist/Logging.h"

#include "enet/enet.h"

#include <algorithm>
#include <string>

NetSession::~NetSession()
{
	Shutdown();
}

bool NetSession::Init()
{
	if (m_ENetReady)
		return true;
	if (enet_initialize() != 0)
	{
		m_LastError = "enet_initialize failed";
		Log("NetSession: " + m_LastError);
		return false;
	}
	m_ENetReady = true;
	return true;
}

void NetSession::Shutdown()
{
	Disconnect();
	if (m_ENetReady)
	{
		enet_deinitialize();
		m_ENetReady = false;
	}
}

void NetSession::Disconnect()
{
	m_Upnp.Close();
	if (m_Host)
	{
		enet_host_destroy(m_Host);
		m_Host = nullptr;
	}
	m_ServerPeer = nullptr;
	m_Mode = Mode::Offline;
	m_Status = "Offline";
}

bool NetSession::Host(uint16_t port, int maxClients)
{
	if (!Init())
		return false;
	Disconnect();

	ENetAddress address{};
	address.host = ENET_HOST_ANY;
	address.port = port;
	m_Host = enet_host_create(&address, static_cast<size_t>(std::max(1, maxClients)), 2, 0, 0);
	if (!m_Host)
	{
		m_LastError = "enet_host_create failed (port in use?)";
		Log("NetSession: " + m_LastError);
		return false;
	}

	m_Mode = Mode::Host;
	m_Port = port;

	// Best-effort UPnP: open UDP so internet friends can reach us.
	if (m_Upnp.OpenUdp(port, port, "Planitia"))
	{
		const std::string pub = m_Upnp.PublicInvite();
		if (!pub.empty())
			m_Status = "Hosting (UPnP OK) " + pub;
		else
			m_Status = "Hosting on port " + std::to_string(port) + " (UPnP mapped, no public IP)";
	}
	else
	{
		m_Status = "Hosting on port " + std::to_string(port) + " (UPnP failed - LAN/port-forward only)";
		Log("NetSession: UPnP failed: " + m_Upnp.LastError());
	}

	Log("NetSession: " + m_Status);
	return true;
}

bool NetSession::Join(const std::string& ip, uint16_t port)
{
	if (!Init())
		return false;
	Disconnect();

	m_Host = enet_host_create(nullptr, 1, 2, 0, 0);
	if (!m_Host)
	{
		m_LastError = "enet_host_create (client) failed";
		Log("NetSession: " + m_LastError);
		return false;
	}

	ENetAddress address{};
	enet_address_set_host(&address, ip.c_str());
	address.port = port;
	m_ServerPeer = enet_host_connect(m_Host, &address, 2, 0);
	if (!m_ServerPeer)
	{
		m_LastError = "enet_host_connect failed";
		Log("NetSession: " + m_LastError);
		enet_host_destroy(m_Host);
		m_Host = nullptr;
		return false;
	}

	m_Mode = Mode::Client;
	m_Port = port;
	m_Status = "Connecting to " + ip + ":" + std::to_string(port);
	Log("NetSession: " + m_Status);
	return true;
}

void NetSession::SetPeerHandlers(PeerHandler onConnect, PeerHandler onDisconnect)
{
	m_OnPeerConnect = std::move(onConnect);
	m_OnPeerDisconnect = std::move(onDisconnect);
}

int NetSession::ConnectedPeerCount() const
{
	if (!m_Host)
		return 0;
	int n = 0;
	for (size_t i = 0; i < m_Host->peerCount; ++i)
	{
		if (m_Host->peers[i].state == ENET_PEER_STATE_CONNECTED)
			++n;
	}
	return n;
}

void NetSession::Service(PacketHandler handler)
{
	if (!m_Host)
		return;

	ENetEvent event;
	while (enet_host_service(m_Host, &event, 0) > 0)
	{
		int peerIndex = -1;
		if (event.peer && m_Host)
			peerIndex = static_cast<int>(event.peer - m_Host->peers);

		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT:
			if (m_Mode == Mode::Client)
			{
				m_Status = "Connected to host";
				Log("NetSession: " + m_Status);
				const auto hello = Net::PackHello("Player");
				SendToHost(hello);
			}
			else
			{
				m_Status = "Players connected: " + std::to_string(ConnectedPeerCount())
					+ " / " + std::to_string(kMaxPlayers - 1);
				Log("NetSession: client connected peerIndex=" + std::to_string(peerIndex));
				if (m_OnPeerConnect)
					m_OnPeerConnect(peerIndex);
			}
			break;

		case ENET_EVENT_TYPE_RECEIVE:
		{
			if (event.packet && handler)
			{
				const uint8_t* data = event.packet->data;
				const size_t size = event.packet->dataLength;
				const uint8_t* p = data;
				const uint8_t* end = data + size;
				Net::PacketType type{};
				if (Net::UnpackHeader(p, end, type))
					handler(type, p, static_cast<size_t>(end - p), peerIndex);
			}
			enet_packet_destroy(event.packet);
			break;
		}

		case ENET_EVENT_TYPE_DISCONNECT:
			if (m_Mode == Mode::Client)
			{
				m_Status = "Disconnected from host";
				m_ServerPeer = nullptr;
			}
			else
			{
				m_Status = "Players connected: " + std::to_string(ConnectedPeerCount())
					+ " / " + std::to_string(kMaxPlayers - 1);
				Log("NetSession: disconnect peerIndex=" + std::to_string(peerIndex));
				if (m_OnPeerDisconnect)
					m_OnPeerDisconnect(peerIndex);
			}
			break;

		default:
			break;
		}
	}
}

void NetSession::Broadcast(const std::vector<uint8_t>& bytes, bool reliable)
{
	if (!m_Host || bytes.empty())
		return;
	ENetPacket* packet = enet_packet_create(
		bytes.data(),
		bytes.size(),
		reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
	enet_host_broadcast(m_Host, Net::kChannelReliable, packet);
	enet_host_flush(m_Host);
}

void NetSession::SendToPeer(int peerIndex, const std::vector<uint8_t>& bytes, bool reliable)
{
	if (!m_Host || peerIndex < 0 || static_cast<size_t>(peerIndex) >= m_Host->peerCount || bytes.empty())
		return;
	ENetPeer* peer = &m_Host->peers[peerIndex];
	if (peer->state != ENET_PEER_STATE_CONNECTED)
		return;
	ENetPacket* packet = enet_packet_create(
		bytes.data(),
		bytes.size(),
		reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
	enet_peer_send(peer, Net::kChannelReliable, packet);
	enet_host_flush(m_Host);
}

void NetSession::DisconnectPeer(int peerIndex)
{
	if (!m_Host || peerIndex < 0 || static_cast<size_t>(peerIndex) >= m_Host->peerCount)
		return;
	ENetPeer* peer = &m_Host->peers[peerIndex];
	if (peer->state == ENET_PEER_STATE_DISCONNECTED)
		return;
	enet_peer_disconnect_now(peer, 0);
	enet_host_flush(m_Host);
	Log("NetSession: force-disconnected peerIndex=" + std::to_string(peerIndex));
}

void NetSession::SendToHost(const std::vector<uint8_t>& bytes, bool reliable)
{
	if (m_Mode != Mode::Client || !m_ServerPeer || bytes.empty())
		return;
	ENetPacket* packet = enet_packet_create(
		bytes.data(),
		bytes.size(),
		reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
	enet_peer_send(m_ServerPeer, Net::kChannelReliable, packet);
	enet_host_flush(m_Host);
}
