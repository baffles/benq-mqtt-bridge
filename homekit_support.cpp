#include "config.h"
#ifdef ENABLE_HOMEKIT

#include "homekit_support.hpp"

#include <sstream>

#include <arduino_homekit_server.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>

extern "C" {
	// these are all defined in homekit_accessory.c
	homekit_characteristic_t hkAccessoryName;
	homekit_characteristic_t hkAccessorySerial;
	homekit_characteristic_t hkAccessoryIdentify;

	// television
	homekit_characteristic_t hkTelevisionName;
	homekit_characteristic_t hkTelevisionIsActive;
	homekit_characteristic_t hkTelevisionActiveInput;
	homekit_characteristic_t hkRemote;

	// inputs
	homekit_characteristic_t *hkTvInputVisibilities;
	homekit_service_t *hkTvInputs;

	// full config
	homekit_server_config_t hkConfig;
};

using namespace std;

HomeKitSupport::HomeKitSupport(Logger &logger, BenQProjector &projector, const char *clientName, const char *homeKitName, const char *setupCode)
	: logger(logger), projector(projector) {	

	if (setupCode != NULL) {
		strcpy(this->setupCode, setupCode);
		hkConfig.password = this->setupCode;
	} else {
		hkConfig.password_callback = [](const char *password) {
			strcpy(::homekit.setupCode, password);
		};
	}

	hkAccessoryName.value.string_value = (char *)clientName;
	snprintf(serialNumber, 9, "%08X", ESP.getChipId());
	hkAccessorySerial.value.string_value = serialNumber;
	hkAccessoryIdentify.setter = [](homekit_value_t value) {
		// no-op
	};

	hkTelevisionName.value.string_value = (char *)homeKitName;
	hkTelevisionIsActive.value.bool_value = false;
	hkTelevisionActiveInput.value.uint32_value = 10;

	hkRemote.setter = [](const homekit_value_t value) {
		switch (value.uint8_value) {
			case HOMEKIT_REMOTE_KEY_REWIND:
				::homekit.logger.debug("HKREMOTE: REWIND");
				break;

			case HOMEKIT_REMOTE_KEY_FAST_FORWARD:
				::homekit.logger.debug("HKREMOTE: FAST FORWARD");
				break;

			case HOMEKIT_REMOTE_KEY_NEXT_TRACK:
				::homekit.logger.debug("HKREMOTE: NEXT TRACK");
				break;

			case HOMEKIT_REMOTE_KEY_PREVIOUS_TRACK:
				::homekit.logger.debug("HKREMOTE: PREVIOUS TRACK");
				break;

			case HOMEKIT_REMOTE_KEY_ARROW_UP:
				::homekit.logger.debug("HKREMOTE: UP");
				break;

			case HOMEKIT_REMOTE_KEY_ARROW_DOWN:
				::homekit.logger.debug("HKREMOTE: DOWN");
				break;

			case HOMEKIT_REMOTE_KEY_ARROW_LEFT:
				::homekit.logger.debug("HKREMOTE: LEFT");
				break;

			case HOMEKIT_REMOTE_KEY_ARROW_RIGHT:
				::homekit.logger.debug("HKREMOTE: RIGHT");
				break;

			case HOMEKIT_REMOTE_KEY_SELECT:
				::homekit.logger.debug("HKREMOTE: SELECT");
				break;

			case HOMEKIT_REMOTE_KEY_BACK:
				::homekit.logger.debug("HKREMOTE: BACK");
				break;

			case HOMEKIT_REMOTE_KEY_EXIT:
				::homekit.logger.debug("HKREMOTE: EXIT");
				break;

			case HOMEKIT_REMOTE_KEY_PLAY_PAUSE:
				::homekit.logger.debug("HKREMOTE: PLAY/PAUSE");
				break;

			case HOMEKIT_REMOTE_KEY_INFORMATION:
				::homekit.logger.debug("HKREMOTE: INFORMATION");
				break;
		}
	};

	hkTvInputVisibilities[0].value.uint8_value = HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN;
}

void HomeKitSupport::setup() {
	// all global-y. yuck.
	arduino_homekit_setup(&hkConfig);
}

void HomeKitSupport::loop() {
	arduino_homekit_loop();
}

void HomeKitSupport::reset() {
	homekit_server_reset();
}

string HomeKitSupport::getHomeKitPageContent(string resetUrl) {
	stringstream page;

	page
		<< "<!DOCTYPE html>" << endl
		<< "<html lang=\"en\">" << endl
		<< "	<head><title>BenQ Projector Bridge: HomeKit</title></head>" << endl
		<< "	<body>" << endl
		<< "		<h1>HomeKit Info</h1>" << endl
		<< "		<div>Device Name: <em>" << hkAccessoryName.value.string_value << "</em></div>" << endl
		<< "		<div>Setup Code: <em>" << getSetupCode() << "</em></div>" << endl
		<< "		<h2>HomeKit Status</h2>" << endl
		<< "		<div>Status: " << (arduino_homekit_get_running_server()->paired ? "PAIRED" : "NOT PAIRED") << "</div>" << endl
		<< "		<div>Client Count: " << arduino_homekit_connected_clients_count() << "</div>" << endl
		<< "		<div><form method=\"POST\" action=\"" << resetUrl << "\"><input type=\"submit\" value=\"Reset HomeKit\" />(will unpair existing device)</form></div>" << endl
		<< "	</body>" << endl
		<< "</html>";
	
	return page.str();
}

const char *HomeKitSupport::getSetupCode() {
	return setupCode;
}

#endif