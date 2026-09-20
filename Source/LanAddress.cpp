#include "LanAddress.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

std::vector<std::string> EnumerateLocalIPv4()
{
	std::vector<std::string> ips;
#ifdef _WIN32
	ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
	ULONG size = 0;
	if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW)
		return ips;
	std::vector<uint8_t> buf(size);
	auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
	if (GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &size) != NO_ERROR)
		return ips;
	for (auto* a = addrs; a; a = a->Next)
	{
		if (a->OperStatus != IfOperStatusUp)
			continue;
		for (auto* u = a->FirstUnicastAddress; u; u = u->Next)
		{
			auto* sa = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
			char str[INET_ADDRSTRLEN] = {};
			if (InetNtopA(AF_INET, &sa->sin_addr, str, sizeof(str)))
			{
				std::string ip(str);
				if (ip != "127.0.0.1")
					ips.push_back(ip);
			}
		}
	}
#else
	ifaddrs* list = nullptr;
	if (getifaddrs(&list) != 0)
		return ips;
	for (ifaddrs* ifa = list; ifa; ifa = ifa->ifa_next)
	{
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		auto* sa = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
		char str[INET_ADDRSTRLEN] = {};
		if (!inet_ntop(AF_INET, &sa->sin_addr, str, sizeof(str)))
			continue;
		std::string ip(str);
		if (ip == "127.0.0.1")
			continue;
		ips.push_back(ip);
	}
	freeifaddrs(list);
#endif
	// Prefer common private LAN ranges first.
	std::sort(ips.begin(), ips.end(), [](const std::string& a, const std::string& b)
	{
		auto rank = [](const std::string& s) {
			if (s.rfind("192.168.", 0) == 0) return 0;
			if (s.rfind("10.", 0) == 0) return 1;
			if (s.rfind("172.", 0) == 0) return 2;
			return 3;
		};
		const int ra = rank(a), rb = rank(b);
		if (ra != rb) return ra < rb;
		return a < b;
	});
	ips.erase(std::unique(ips.begin(), ips.end()), ips.end());
	return ips;
}
