#ifndef _PLANITIA_STDIO_SEAT_H_
#define _PLANITIA_STDIO_SEAT_H_

#include <string>

// One external AI. Stdout is the command channel. Stderr is left alone for its logs.
class StdioSeat
{
public:
	StdioSeat() = default;
	~StdioSeat();

	StdioSeat(const StdioSeat&) = delete;
	StdioSeat& operator=(const StdioSeat&) = delete;

	bool Start(const std::string& command, std::string& error);
	void Close();
	bool Alive() const { return m_Alive; }

	// Writes one observation line and reads one reply line.
	bool Exchange(const std::string& observationLine, std::string& replyLine);

private:
	int m_Pid = -1;
	int m_ToChild = -1;
	int m_FromChild = -1;
	bool m_Alive = false;
};

#endif
