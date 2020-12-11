#ifndef HOMEKIT_HPP
#define HOMEKIT_HPP

#include "logger.hpp"
#include "projector.hpp"

#include <string>

/**
 * Basic HomeKit support provided with Arduino-HomeKit-ESP8266. This library seems to push this
 * chip to the limit; it does some things internally to perform well enough to work right,
 * including automatically boosting the CPU to 160 MHz. This support may be flakey; it took a fair
 * bit of trial and error to get functioning correctly in the first place, and given the nature of
 * HomeKit and the difficulty of debugging it, this could probably break at any time.
 * 
 * Not that anyone reading this probably cares, but this class is intended to be a singleton; there
 * are some assumptions made internally about that (referencing the `homekit` extern that's defined
 * below). This mostly comes back to the very C/global nature of Ardiuno HomeKit.
 */
class HomeKitSupport {
public:

	HomeKitSupport(
		Logger &logger, BenQProjector &projector,
		const char *clientName, const char *homeKitName,
		const char *setupCode
	);

	void setup();
	void loop();
	void reset();

	// if the HTTP server decides to render a page, it will call this
	std::string getHomeKitPageContent(std::string resetUrl);

	const char *getSetupCode();

private:

	Logger &logger;
	BenQProjector &projector;
	char serialNumber[9] = "";
	char setupCode[11] = "";
};

// our singleton instance
extern HomeKitSupport homekit;

#endif