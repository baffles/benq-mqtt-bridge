/** BenQ projector/MQTT bridge.
 * This bridge talks to BenQ projectors over RS232 and publishes status information to MQTT as well
 * as subscribing to MQTT to allow control over the network.
 *
 * Copyright 2020 Robert Ferris
 */

#include "config.h"

#include <functional>

#include "logger.hpp"
#include "projector.hpp"
#include "power_state.hpp"

using std::bind;
using namespace std::placeholders;

// set up logging
Logger logger;

BenQProjector projector(
	logger,
	// Serial1 is transmit only, so we send on that, and need to use Serial to receive
	// (this lets us keep transmitting to a serial console on Serial)
	Serial, Serial1,
	//TODO: configurability for these next values?
	// poll every second
	3
	// block cycling the projector absurdly often, so force an on time of 10 mins, off time of 5 mins
	// 600, 300
);

PowerState projectorPower(
	logger, projector,
	10 * 60, // stay on for at least 10 minutes
	6 * 60 * 60, // power off after 6 hours by default
	5 * 60, // stay off for at least 5 minutes
	2 * 60, // 2 minute grace period on power off
	2 * 60 * 60 // (unless projector has been on for 2 hours already)
);


// MQTT setup
#ifdef ENABLE_MQTT

#include "mqtt.hpp"
MqttSupport mqtt(
	logger, projector, projectorPower,
	MQTT_STATUS_INTERVAL,
	CLIENT_NAME,
	MQTT_SERVER, MQTT_SERVER_PORT,
	MQTT_USERNAME, MQTT_PASSWORD,
	MQTT_SET_POWER_TOPIC, MQTT_SET_VOLUME_TOPIC, MQTT_SET_SOURCE_TOPIC, MQTT_SET_LAMP_MODE_TOPIC,
	MQTT_REMOTE_COMMAND_TOPIC,
	MQTT_RAW_COMMAND_TOPIC,
	MQTT_STATUS_TOPIC
);

#endif


// HTTP setup
#ifdef ENABLE_HTTP

#include "http.hpp"
HttpSupport http(
	logger, projector, projectorPower,
	HTTP_PORT,
	#ifdef ENABLE_HTTP_OTA_UPDATE
		true
	#else
		false
	#endif
);

#endif


// HomeKit setup
#ifdef ENABLE_HOMEKIT


#include "homekit_support.hpp"
HomeKitSupport homekit(logger, projector, CLIENT_NAME, HOMEKIT_NAME, NULL);

#endif


void setup() {
	// Serial1 only transmit, so we read input on the main serial port
	// this means anything sent on serial from a connected computer will be interpreted as from the
	// projector
	Serial.begin(SERIAL_BAUD_RATE);
	Serial1.begin(SERIAL_BAUD_RATE);

	#ifdef SERIAL_LOGGING
	logger.addListener([](LogEntry entry) {
		switch (entry.type) {
			case DEBUG_LOG: Serial.print("DEBUG "); break;
			case INFO_LOG:  Serial.print("INFO  "); break;
			case ERROR_LOG: Serial.print("ERROR "); break;
			case COMM_SENT: Serial.print("  >>  "); break;
			case COMM_ECHO: Serial.print("  <>  "); break;
			case COMM_RECV: Serial.print("  <<  "); break;
		}

		Serial.println(entry.entry.c_str());
	});
	#endif

	// setup wifi
	WiFi.mode(WIFI_STA);
	WiFi.hostname(CLIENT_NAME);
	WiFi.persistent(false);
	WiFi.setAutoReconnect(true);
	WiFi.begin(WIFI_SSID, WIFI_PSK);

	while(!WiFi.isConnected()) {
		delay(100);
	}

	// projector communication is ready to go
	projector.begin();
	projectorPower.begin();

	#ifdef ENABLE_HTTP
	http.setup();
	#endif

	#ifdef ENABLE_MQTT
	mqtt.setup();
	#endif

	#ifdef ENABLE_HOMEKIT
	#ifdef ENABLE_HTTP
	http.addHomeKitSupport(bind(&HomeKitSupport::getHomeKitPageContent, homekit, _1), bind(&HomeKitSupport::reset, homekit));
	#endif

	homekit.setup();
	#endif
}

void loop() {
	projector.loop();
	projectorPower.loop();

	#ifdef ENABLE_HTTP
	http.loop();
	#endif

	#ifdef ENABLE_MQTT
	mqtt.loop();
	#endif

	#ifdef ENABLE_HOMEKIT
	homekit.loop();
	#endif
}
