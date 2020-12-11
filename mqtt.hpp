#ifndef MQTT_HPP
#define MQTT_HPP

#include "logger.hpp"
#include "projector.hpp"
#include "power_state.hpp"

#include <EspMQTTClient.h>

/**
 * Bundles up the MQTT support in one central place for easy inclusion/exclusion from firmware.
 */
class MqttSupport {
public:

	MqttSupport(
		Logger &logger, BenQProjector &projector, PowerState &projectorPower,
		int publishIntervalMs,
		const char *clientName, const char *server, const short port, const char *username, const char *password,
		const char *powerSetTopic, const char *volumeSetTopic, const char *sourceSetTopic, const char *lampModeSetTopic,
		const char *remoteTopic,
		const char *rawSendTopic,
		const char *statusTopic
	);

	void setup();
	void loop();

private:

	Logger &logger;
	BenQProjector &projector;
	PowerState &projectorPower;
	EspMQTTClient mqtt;

	int publishInterval;
	const char *powerSetTopic, *volumeSetTopic, *sourceSetTopic, *lampModeSetTopic;
	const char *remoteTopic;
	const char *rawSendTopic;
	const char *statusTopic;

	char lastStatus[128];

	void onConnectionEstablished();
	void scheduleMqttStatus();

	void publishStatus();
};

#endif