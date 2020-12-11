#ifndef LOG_HPP
#define LOG_HPP

// default number of log entries to keep is 32, messages beyond this get purged
#define LOG_ENTRIES 32

#include <deque>
#include <functional>
#include <list>
#include <string>

enum LogEntryType {
	// various remark levels
	DEBUG_LOG, INFO_LOG, ERROR_LOG,

	// special types for messages sent to and received from the projector over RS232
	COMM_SENT, COMM_ECHO, COMM_RECV,
};

struct LogEntry {
	LogEntry(LogEntryType type, std::string entry);

	LogEntryType type;
	std::string entry;
};

/**
 * Logger that manages a rotating log of the last `LOG_ENTRIES` entries. This is available for
 * display (e.g., on a web UI), can optionally be logged to a Serial debugging console, and can
 * also be subscribed to externally (e.g., to send over MQTT)
 */
class Logger {
public:

	void addListener(std::function<void(LogEntry)> listener);

	void log(LogEntryType type, std::string entry);
	void debug(std::string entry);
	void info(std::string entry);
	void error(std::string entry);
	void commSent(std::string entry);
	void commEcho(std::string entry);
	void commRecv(std::string entry);

	void foreach(std::function<void(LogEntry)> f);

private:

	std::list<std::function<void(LogEntry)>> listeners;
	std::deque<LogEntry> entries;

	void trimLog();
};

#endif