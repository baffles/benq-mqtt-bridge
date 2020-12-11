#include "config.h"
#ifdef ENABLE_MQTT

#include "mqtt.hpp"

#include <ArduinoJson.h>

using std::bind;

MqttSupport::MqttSupport(
	Logger &logger, BenQProjector &projector,
	int publishIntervalMs,
	const char *clientName, const char *server, const short port, const char *username, const char *password,
	const char *powerSetTopic, const char *volumeSetTopic, const char *sourceSetTopic, const char *lampModeSetTopic,
	const char *remoteTopic,
	const char *rawSendTopic,
	const char *statusTopic
) : logger(logger), projector(projector),
	publishInterval(publishIntervalMs),
	mqtt(server, port, username, password, clientName),
	powerSetTopic(powerSetTopic), volumeSetTopic(volumeSetTopic), sourceSetTopic(sourceSetTopic), lampModeSetTopic(lampModeSetTopic),
	remoteTopic(remoteTopic),
	rawSendTopic(rawSendTopic),
	statusTopic(statusTopic) {
}

void MqttSupport::setup() {
	// mqtt.enableDebuggingMessages();
	mqtt.setOnConnectionEstablishedCallback(bind(&MqttSupport::onConnectionEstablished, this));
}

void MqttSupport::loop() {
	mqtt.loop();
}


void MqttSupport::publishStatus() {
	StaticJsonDocument<128> status;

	status["power"] = projector.isOn();
	status["model"] = projector.getModelName();
	
	if (projector.isOn()) {
		status["source"] = projector.getSource();
		status["volume"] = projector.getVolume();
		status["lamp_mode"] = projector.getLampMode();
	}

	serializeJson(status, lastStatus, 128);

	// logger.debug(lastStatus);

	mqtt.publish(statusTopic, lastStatus);
}


void onConnectionEstablished() {
	// global forced on us; not used
}

void MqttSupport::onConnectionEstablished() {
	// we're connected, so set up our subscriptions
	mqtt.subscribe(powerSetTopic, [this](const String& payload) {
		const char *val = payload.c_str();

		if (strcasecmp(val, "on") == 0 || strcasecmp(val, "true") == 0) {
			projector.turnOn();
		} else {
			projector.turnOff();
		}
	});

	mqtt.subscribe(volumeSetTopic, [this](const String& payload) {
		projector.setVolume(atoi(payload.c_str()));
	});

	mqtt.subscribe(sourceSetTopic, [this](const String& payload) {
		projector.setSource(payload.c_str());
	});

	mqtt.subscribe(lampModeSetTopic, [this](const String& payload) {
		projector.setLampMode(payload.c_str());
	});

	mqtt.subscribe(rawSendTopic, [this](const String& payload) {
		projector.queueRaw(payload.c_str());
	});

	mqtt.subscribe(remoteTopic, [this](const String& payload) {
		const char *val = payload.c_str();

		//TODO: shouldn't this be internal to BenQProjector?
		if (strcasecmp(val, "INFO") == 0) {
			// info toggles menu on and off
			projector.queueRaw("menu");
		} else if (strcasecmp(val, "BACK") == 0) {
			// back closes menu
			projector.queueValue("menu", "off");
		} else if (strcasecmp(val, "SELECT") == 0) {
			projector.queueRaw("enter");
		} else if (strcasecmp(val, "UP") == 0) {
			projector.queueRaw("up");
		} else if (strcasecmp(val, "DOWN") == 0) {
			projector.queueRaw("down");
		} else if (strcasecmp(val, "LEFT") == 0) {
			projector.queueRaw("left");
		} else if (strcasecmp(val, "RIGHT") == 0) {
			projector.queueRaw("right");
		}
	});

	scheduleMqttStatus();
}

void MqttSupport::scheduleMqttStatus() {
	mqtt.executeDelayed(publishInterval, [this]() {
		publishStatus();
		scheduleMqttStatus();
	});
}

#endif