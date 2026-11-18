#include "Harness/StdioSeat.h"

#include <cerrno>
#include <cstring>

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

StdioSeat::~StdioSeat()
{
	Close();
}

bool StdioSeat::Start(const std::string& command, std::string& error)
{
	Close();
	error.clear();
	if (command.empty())
	{
		error = "stdio seat is missing a command";
		return false;
	}

#ifdef _WIN32
	(void)command;
	error = "stdio seats are supported on posix only";
	return false;
#else
	int toChild[2] = { -1, -1 };
	int fromChild[2] = { -1, -1 };
	if (pipe(toChild) != 0 || pipe(fromChild) != 0)
	{
		error = std::string("pipe failed: ") + std::strerror(errno);
		return false;
	}

	const pid_t pid = fork();
	if (pid < 0)
	{
		error = std::string("fork failed: ") + std::strerror(errno);
		::close(toChild[0]);
		::close(toChild[1]);
		::close(fromChild[0]);
		::close(fromChild[1]);
		return false;
	}

	if (pid == 0)
	{
		dup2(toChild[0], STDIN_FILENO);
		dup2(fromChild[1], STDOUT_FILENO);
		::close(toChild[0]);
		::close(toChild[1]);
		::close(fromChild[0]);
		::close(fromChild[1]);
		execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
		_exit(127);
	}

	::close(toChild[0]);
	::close(fromChild[1]);
	m_ToChild = toChild[1];
	m_FromChild = fromChild[0];
	m_Pid = static_cast<int>(pid);
	m_Alive = true;
	return true;
#endif
}

void StdioSeat::Close()
{
#ifndef _WIN32
	if (m_ToChild >= 0)
	{
		::close(m_ToChild);
		m_ToChild = -1;
	}
	if (m_FromChild >= 0)
	{
		::close(m_FromChild);
		m_FromChild = -1;
	}
	if (m_Pid > 0)
	{
		int status = 0;
		if (waitpid(m_Pid, &status, WNOHANG) == 0)
		{
			kill(m_Pid, SIGTERM);
			waitpid(m_Pid, &status, 0);
		}
		m_Pid = -1;
	}
#endif
	m_Alive = false;
}

bool StdioSeat::Exchange(const std::string& observationLine, std::string& replyLine)
{
	replyLine.clear();
	if (!m_Alive)
		return false;

#ifdef _WIN32
	(void)observationLine;
	m_Alive = false;
	return false;
#else
	const std::string line = observationLine + "\n";
	const char* data = line.data();
	size_t left = line.size();
	while (left > 0)
	{
		const ssize_t n = ::write(m_ToChild, data, left);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			m_Alive = false;
			return false;
		}
		data += n;
		left -= static_cast<size_t>(n);
	}

	replyLine.clear();
	char buf[1];
	while (true)
	{
		const ssize_t n = ::read(m_FromChild, buf, 1);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			m_Alive = false;
			return false;
		}
		if (n == 0)
		{
			m_Alive = false;
			return !replyLine.empty();
		}
		if (buf[0] == '\n')
			break;
		if (buf[0] != '\r')
			replyLine.push_back(buf[0]);
	}
	return true;
#endif
}
