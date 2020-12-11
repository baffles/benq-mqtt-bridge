#ifndef HTTP_HPP
#define HTTP_HPP

// for any power states that were before 30s of uptime we will assume that state was pre-boot and
// say "at least" in the UI
#define CONTROLLER_BOOT_THRESHOLD 30000

#include "logger.hpp"
#include "projector.hpp"

#include <functional>
#include <string>

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

using std::function;
using std::string;

/**
 * Bundles up the HTTP support in one central place for easy inclusion/exclusion from firmware.
 */
class HttpSupport {
public:

	HttpSupport(
		Logger &logger, BenQProjector &projector,
		int httpPort,
		bool enableOtaUpdates
	);

	void setup();
	void loop();

	void addHomeKitSupport(function<string(string)> getStatusPageHtml, function<void()> resetHomeKit);

private:

	Logger &logger;
	BenQProjector &projector;
	ESP8266WebServer httpServer;
	ESP8266HTTPUpdateServer updateServer;
};

#endif