///////////////////////////////////////////////////////////////////////////
//
// Name:     UPNPPORTMAP.H
// Purpose:  UPnP IGD port mapping for listen-server hosting (open on Host,
//           close on Disconnect). Uses vendored miniupnpc.
//
///////////////////////////////////////////////////////////////////////////

#ifndef _PLANITIA_UPNPPORTMAP_H_
#define _PLANITIA_UPNPPORTMAP_H_

#include <cstdint>
#include <string>

class UpnpPortMap
{
public:
	UpnpPortMap() = default;
	~UpnpPortMap();

	// Discover IGD, add UDP mapping for externalPort -> localPort on this machine.
	// leaseSeconds 0 = permanent (some routers require that).
	bool OpenUdp(uint16_t externalPort, uint16_t localPort, const char* description = "Planitia", unsigned int leaseSeconds = 0);

	// Remove the mapping we created (safe to call if none).
	void Close();

	bool IsMapped() const { return m_Mapped; }
	const std::string& ExternalIp() const { return m_ExternalIp; }
	uint16_t ExternalPort() const { return m_ExternalPort; }
	const std::string& LanIp() const { return m_LanIp; }
	const std::string& LastError() const { return m_LastError; }

	// "1.2.3.4:43000" if mapped with a public IP; empty otherwise.
	std::string PublicInvite() const;

private:
	void ClearUrls();

	bool m_Mapped = false;
	uint16_t m_ExternalPort = 0;
	uint16_t m_LocalPort = 0;
	std::string m_ExternalIp;
	std::string m_LanIp;
	std::string m_ControlUrl;
	std::string m_ServiceType;
	std::string m_LastError;
};

#endif
