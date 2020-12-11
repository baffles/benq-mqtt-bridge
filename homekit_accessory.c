#include "config.h"
#ifdef ENABLE_HOMEKIT


// per the homekit docs:
// Must update when an accessory, service, or characteristic is added or removed on the accessory server.
// Accessories must increment the config number after a firmware update.
// This must have a range of 1-65535 and wrap to 1 when it overflows.
// This value must persist across reboots, power cycles, etc.
#define HOMEKIT_CONFIG_NUMBER 1


#include "version.h"

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

// the macros provided by the homekit library don't play nicely with C++, so we need to define them
// in a C file that gets compiled by gcc rather than g++

// accessory information
homekit_characteristic_t hkAccessoryName = HOMEKIT_CHARACTERISTIC_(NAME, "");
homekit_characteristic_t hkAccessorySerial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "");
homekit_characteristic_t hkAccessoryIdentify = HOMEKIT_CHARACTERISTIC_(IDENTIFY, NULL);

// television
homekit_characteristic_t hkTelevisionName = HOMEKIT_CHARACTERISTIC_(CONFIGURED_NAME, "");
homekit_characteristic_t hkTelevisionIsActive = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);
homekit_characteristic_t hkTelevisionActiveInput = HOMEKIT_CHARACTERISTIC_(ACTIVE_IDENTIFIER, 0);
homekit_characteristic_t hkRemote = HOMEKIT_CHARACTERISTIC_(REMOTE_KEY);

// inputs
homekit_characteristic_t hkTvInputVisibilities[] = {
	HOMEKIT_CHARACTERISTIC_(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_HIDDEN)
};

homekit_service_t hkTvInputs[] = {
	HOMEKIT_SERVICE_(INPUT_SOURCE, .hidden = true, .characteristics = (homekit_characteristic_t*[]) {
		HOMEKIT_CHARACTERISTIC(IDENTIFIER, 10),
		HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, "HDMI"),
		HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, 1),
		HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_HDMI),
		&hkTvInputVisibilities[0],
		NULL
	}),
	NULL
};

homekit_accessory_t *hkAccessories[] = {
	HOMEKIT_ACCESSORY(.id = 1, .config_number = HOMEKIT_CONFIG_NUMBER, .category = homekit_accessory_category_television, .services = (homekit_service_t*[]) {
		HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
			&hkAccessoryName,
			HOMEKIT_CHARACTERISTIC(MANUFACTURER, "BAF.zone"),
			&hkAccessorySerial,
			HOMEKIT_CHARACTERISTIC(MODEL, "BenQ-Bridge ESP8266"),
			HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, VERSION),
			&hkAccessoryIdentify,
			NULL
		}),
		HOMEKIT_SERVICE(TELEVISION, .primary = true, .characteristics = (homekit_characteristic_t*[]) {

			NULL
		}, .linked = (homekit_characteristic_t*[]) {
			&hkTvInputs[0],
			NULL
		}),
		&hkTvInputs[0],
		NULL
	}),
	NULL
};

homekit_server_config_t hkConfig = {
	.config_number = HOMEKIT_CONFIG_NUMBER,
	.accessories = hkAccessories,
	.password = ""
};

#endif