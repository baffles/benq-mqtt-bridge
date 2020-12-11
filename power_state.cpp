#include "power_state.hpp"

#include <Arduino.h>

PowerState::PowerState(
	Logger &logger, BenQProjector &projector,
	int minimumOnSeconds, int maximumOnSeconds, int minimumOffSeconds,
	int virtualOffGracePeriodSeconds,
	int skipGracePeriodAfterSeconds
) :
	logger(logger), projector(projector),
	minimumOnMillis(minimumOnSeconds * 1000), maximumOnMillis(maximumOnSeconds * 1000), minimumOffMillis(minimumOffSeconds * 1000),
	virtualOffGracePeriodMillis(virtualOffGracePeriodSeconds * 1000), skipGracePeriodAfterMillis(skipGracePeriodAfterSeconds * 1000),
	initialized(false), lastKnownPowerState(false), lastKnownBlankState(false),
	offTimeByLimit(-1), pendingOffTime(-1) {
}

void PowerState::begin() {
	// nothing to do at initialization
}

void PowerState::loop() {
	long now = millis();

	if (!initialized) {
		// get our initial state
		if (projector.isInitialized()) {
			lastKnownPowerState = projector.isOn();

			if (maximumOnMillis > 0 && lastKnownPowerState) {
				// we're probably off, but set the limit off time now
				offTimeByLimit = now + maximumOnMillis;
			}
		} else {
			// don't do anything else until initialized
			return;
		}
	}

	bool nextOn = projector.isOn();
	bool nextBlank = projector.isImageBlanked();

	if (nextOn && !nextBlank && lastKnownBlankState) {
		// blanking was turned off manually, so treat that as a 'power on' and cancel out the
		// pending off
		pendingOffTime = -1;
	}

	if (nextOn == lastKnownPowerState) {
		// projector hasn't turned on or off on this loop, so check and see if we need to do
		// anything
		if ((pendingOffTime > 0 && now >= pendingOffTime) || (offTimeByLimit > 0 && now >= offTimeByLimit)) {
			// do the actual shutdown
			projector.turnOff();
			pendingOffTime = -1;
			offTimeByLimit = -1;
		}
	} else {
		// projector has turned on or off, so cancel out any pending operations
		pendingOffTime = -1;
		offTimeByLimit = -1;

		if (maximumOnMillis > 0 && nextOn) {
			// projector turned on, so queue our off-by-limit
			offTimeByLimit = now + maximumOnMillis;
		}
	}
}

bool PowerState::requestPowerOn() {
	long now = millis();

	if (projector.isOn()) {
		if (pendingOffTime > 0) {
			// projector is on, but was "virtually off" - so turn it "back on"
			projector.setImageBlank(false);
			pendingOffTime = -1;
			return true;
		} else {
			// projector is on and not "virtually off" - so we can't do anything
			return false;
		}
	} else if ((now - projector.getLastOffTime()) < minimumOffMillis) {
		// projector is off but hasn't been off for long enough
		return false;
	} else {
		// projector has been off for the minimum off time
		projector.turnOn();
		return true;
	}
}

void PowerState::requestPowerOff() {
	long now = millis();

	if (projector.isOn() && pendingOffTime > 0) {
		auto onTime = now - projector.getLastOnTime();

		// projector is on and not already pending being turned off
		if (onTime >= skipGracePeriodAfterMillis && onTime >= minimumOnMillis) {
			// projector has been on long enough to instantly power off
			projector.turnOff();
		} else {
			// projector has been on short enough that we fake the power off with a blank
			projector.setImageBlank(true);

			if (onTime < minimumOnMillis && (minimumOnMillis - onTime) > virtualOffGracePeriodMillis) {
				// we haven't met the minimum on time AND that time would be longer than the
				// "virtual off" time; note that the minimum on time starts from the on time, so
				// we subtract however long it was on (i.e, if it was on for 4 minutes but the
				// minimum was 5, we only have to delay 1 more minute)
				pendingOffTime = now + minimumOnMillis - onTime;
			} else {
				// either we've met the minimum on time, or the "virtual off" time is greater
				// than the minimum on time anyway
				pendingOffTime = now + virtualOffGracePeriodMillis;
			}
		}
	}
}

// we consider the projector on if, on last check, it was on and didn't have a pending off time
bool PowerState::getVirtualPowerState() { return projector.isOn() && (pendingOffTime <= 0); }
bool PowerState::getRealPowerState() { return projector.isOn(); }

long PowerState::getPendingRealOffTime() {
	// return the soonest off time
	if (offTimeByLimit > 0 && offTimeByLimit < pendingOffTime) {
		return offTimeByLimit;
	} else {
		return pendingOffTime;
	}
}

void PowerState::cancelOffByLimit() {
	// turn off the limit-off this time around
	offTimeByLimit = -1;
}

long PowerState::getAllowedOnTime() {
	// get the time we're allowing a turn-on
	auto turnOnTime = projector.getLastOffTime() + minimumOffMillis;

	if (!projector.isOn() && millis() < turnOnTime) {
		return turnOnTime;
	}

	return -1;
}
