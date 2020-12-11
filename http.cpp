#include "config.h"
#ifdef ENABLE_HTTP

#include "http.hpp"
#include "version.h"

#include <iomanip>
#include <sstream>

#include <ArduinoJson.h>

using namespace std;

HttpSupport::HttpSupport(
	Logger &logger, BenQProjector &projector,
	int httpPort,
	bool enableOtaUpdates
) : logger(logger), projector(projector),
	httpServer(httpPort),
	updateServer(true) {

	if (enableOtaUpdates) {
		updateServer.setup(&httpServer, "/update");
	}
}

void HttpSupport::setup() {
	httpServer.on("/", HTTP_GET, [this]() {
		stringstream response;

		auto lastPowerEvent = projector.isOn() ? projector.getLastOnTime() : projector.getLastOffTime();
		bool powerFromPreBoot = lastPowerEvent < CONTROLLER_BOOT_THRESHOLD;
		float powerTime = (millis() - lastPowerEvent) / 1000;
		const char *powerUnit = "seconds";

		if (powerTime > 60) {
			powerTime /= 60;
			powerUnit = powerTime > 1 ? "minutes" : "minute";
		}
		if (powerTime > 90) {
			powerTime /= 60;
			powerUnit = powerTime > 1 ? "hours" : "hour";
		}

		response
			<< "<!DOCTYPE html>" << endl
			<< "<html lang=\"en\">" << endl
			<< "	<head><title>BenQ Projector Bridge</title></head>" << endl
			<< "	<body>" << endl
			<< "		<h1>BenQ " << projector.getModelName() << "</h1>" << endl
			<< "		<div>Power: <span style=\" color: " << (projector.isOn() ? "green" : "red") << ";\">" << projector.getStatusStr() << "</span> (for " << (powerFromPreBoot ? "at least " : "") << fixed << setprecision(1) << powerTime << " " << powerUnit << ")</div>" << endl
			<< "		<div>Lamp Hours: " << projector.getLampHours() << "</div>" << endl;
		
		if (projector.isOn()) {
			response
				<< "		<div>Source: " << projector.getSource() << "</div>" << endl
				<< "		<div>Picture: " << (projector.isImageBlanked() ? "<span style=\"color: orange;\">Blank</span>" : projector.isImageFrozen() ? "<span style=\"color: orange;\">Freeze</span>" : "Normal") << "</div>" << endl
				<< "		<div>Volume: " << projector.getVolume() << (projector.isMuted() ? " (<span style=\"color: royalblue;\">muted</span>)" : "") << "</div>" << endl
				<< "		<div>Lamp Mode: " << projector.getLampMode() << "</div>" << endl;
		}

		response
			// << "		<div>Last MQTT Status: <code>" << statusJson << "</code></div>" << endl
			<< "		<div><a href=\"/log\">Log</a></div>" << endl
			<< "	</body>" << endl
			<< "</html>";
		
		httpServer.send(200, "text/html", response.str().c_str());
	});

	httpServer.on("/status", HTTP_GET, [this]() {
		StaticJsonDocument<128> status;

		status["power"] = projector.isOn();
		status["model"] = projector.getModelName();
		
		if (projector.isOn()) {
			status["source"] = projector.getSource();
			status["volume"] = projector.getVolume();
			status["lamp_mode"] = projector.getLampMode();
		}

		char statusJson[128];
		serializeJson(status, statusJson, 128);
		
		httpServer.send(200, "application/json", statusJson);
	});

	httpServer.on("/log", HTTP_GET, [this]() {
		stringstream response;

		response
			<< "<!DOCTYPE html>" << endl
			<< "<html lang=\"en\">" << endl
			<< "	<head><title>BenQ Projector Bridge</title></head>" << endl
			<< "	<body>" << endl
			<< "		<h1>Recent Messages</h1>" << endl
			<< "		<pre>" << endl;
		
		logger.foreach([&response](LogEntry entry) {
			switch (entry.type) {
				case DEBUG_LOG: response << "DEBUG "; break;
				case INFO_LOG:  response << "INFO  "; break;
				case ERROR_LOG: response << "ERROR "; break;
				case COMM_SENT: response << "  &gt;&gt;  "; break;
				case COMM_ECHO: response << "  &lt;&gt;  "; break;
				case COMM_RECV: response << "  &lt;&lt;  "; break;
			}

			response << entry.entry << endl;
		});

		response
			<< "		</pre>" << endl
			<< "		<form method=\"post\" action=\"/send\"><input type=\"text\" name=\"cmd\"><input type=\"submit\" value=\"Send\"></form>" << endl
			<< "	</body>" << endl
			<< "</html>";
		
		httpServer.send(200, "text/html", response.str().c_str());
	});

	httpServer.on("/send", HTTP_POST, [this]() {
		if (httpServer.hasArg("cmd")) {
			projector.queueRaw(httpServer.arg("cmd").c_str());
		}

		httpServer.sendHeader("Location", "/log");
		httpServer.sendHeader("Cache-Control", "no-cache");
		httpServer.send(303);
	});

	httpServer.on("/about", HTTP_GET, [this]() {
		stringstream about;
		about << "BenQ Bridge version " << VERSION << "; copyright (C) 2020 Robert Ferris";

		httpServer.send(200, "text/plain", about.str().c_str());
	});

	httpServer.on("/stats", HTTP_GET, [this]() {
		stringstream response;

		long total;
		int count10s, count60s, count360s;
		float rate10s, rate60s, rate360s;

		response
			<< "<!DOCTYPE html>" << endl
			<< "<html lang=\"en\">" << endl
			<< "	<head><title>BenQ Projector Bridge</title></head>" << endl
			<< "	<body>" << endl
			<< "		<h1>Nerd Stats</h1>" << endl;
		
		projector.getSendStats(total, count10s, count60s, count360s, rate10s, rate60s, rate360s);
		response
			<< "		<h2>Messages Sent</h2>" << endl
			<< "		<div>Total: " << total << "</div>" << endl
			<< "		<div>10s: " << count10s << ", 1m: " << count60s << ", 10m: " << count360s << "</div>" << endl
			<< fixed << setprecision(2)
			<< "		<div>Rate: " << rate10s << "/s (10s), " << rate60s << "/s (1m), " << rate360s << "/s (10m)</div>" << endl;

		projector.getRecvStats(total, count10s, count60s, count360s, rate10s, rate60s, rate360s);
		response
			<< "		<h2>Messages Received</h2>" << endl
			<< "		<div>Total: " << total << "</div>" << endl
			<< "		<div>10s: " << count10s << ", 1m: " << count60s << ", 10m: " << count360s << "</div>" << endl
			<< fixed << setprecision(2)
			<< "		<div>Rate: " << rate10s << "/s (10s), " << rate60s << "/s (1m), " << rate360s << "/s (10m)</div>" << endl;
		
		response
			<< "	</body>" << endl
			<< "</html>";
		
		httpServer.send(200, "text/html", response.str().c_str());
	});

	httpServer.begin();
}

void HttpSupport::addHomeKitSupport(function<string(string)> getStatusPageHtml, function<void()> resetHomeKit) {
	httpServer.on("/homekit", HTTP_GET, [this, getStatusPageHtml]() {
		httpServer.send(200, "text/html", getStatusPageHtml("/reset").c_str());
	});

	httpServer.on("/reset", HTTP_POST, [this, resetHomeKit]() {
		resetHomeKit();

		httpServer.sendHeader("Location", "/homekit");
		httpServer.sendHeader("Cache-Control", "no-cache");
		httpServer.send(303);
	});
}

void HttpSupport::loop() {
	httpServer.handleClient();
}

#endif