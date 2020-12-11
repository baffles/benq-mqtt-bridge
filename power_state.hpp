#ifndef POWER_STATE_HPP
#define POWER_STATE_HPP

#include "logger.hpp"
#include "projector.hpp"

/**
 * This is a simple state machine for managing the power state of the projetor. We apply several
 * rules with this state machine:
 * * Enforcing minimum on and off times before changing the power state
 * * Providing a 'virtual off' state where the screen is just blanked before turning the projector
 *   off. This time that the 'virtual off' stays in effect drops based on how long the projector
 *   has been on for.
 * * Providing for a maximum on-time, after which the projector will be automatically powered off.
 *   This can be temporarily overridden via HTTP or MQTT on a case by case basis.
 * 
 * This is meant to wrap a BenQProjector instance and serve as a proxy for power state control.
 */
class PowerState {
public:

	PowerState(
		Logger &logger, BenQProjector &projector,
		// the on/off time limits; 0 to disable
		int minimumOnSeconds, int maximumOnSeconds, int minimumOffSeconds,
		// the "grace period" for the virtual off time
		int virtualOffGracePeriodSeconds,
		// the on-time, after which, the virtual off grace period is skipped
		int skipGracePeriodAfterSeconds
	);

	void begin();
	void loop();

	bool requestPowerOn(); // returns false if the projector can't be turned on right now
	void requestPowerOff();

	bool getVirtualPowerState();
	bool getRealPowerState();

	long getPendingRealOffTime();
	void cancelOffByLimit();

	long getAllowedOnTime();

private:

	Logger &logger;
	BenQProjector &projector;
	int minimumOnMillis, maximumOnMillis, minimumOffMillis;
	int virtualOffGracePeriodMillis, skipGracePeriodAfterMillis;

	// projector state tracking
	bool initialized, lastKnownPowerState, lastKnownBlankState;

	// our own state
	long offTimeByLimit; // pending power off time due to on-time limit
	long pendingOffTime; // pending off time for a virtual power off
};

#endif