#include "logger.hpp"

LogEntry::LogEntry(LogEntryType type, std::string entry) :
	type(type), entry(entry) {
}

void Logger::addListener(std::function<void(LogEntry)> listener) {
	listeners.push_back(listener);
}

void Logger::log(LogEntryType type, std::string entry) {
	LogEntry logEntry(type, entry);

	entries.push_back(logEntry);
	trimLog();

	for (auto it = listeners.begin(); it != listeners.end(); it++) {
		(*it)(logEntry);
	}
}

void Logger::trimLog() {
	while (entries.size() > LOG_ENTRIES) {
		entries.pop_front();
	}
}

void Logger::debug(std::string entry) { log(DEBUG_LOG, entry); }
void Logger::info(std::string entry) { log(INFO_LOG, entry); }
void Logger::error(std::string entry) { log(ERROR_LOG, entry); }
void Logger::commSent(std::string entry) { log(COMM_SENT, entry); }
void Logger::commEcho(std::string entry) { log(COMM_ECHO, entry); }
void Logger::commRecv(std::string entry) { log(COMM_RECV, entry); }

void Logger::foreach(std::function<void(LogEntry)> f) {
	for (auto it = entries.begin(); it != entries.end(); it++) {
		f(*it);
	}
}
