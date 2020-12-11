#include "projector.hpp"

#include <sstream>

#include <Arduino.h>

using std::stringstream;

BenQProjector::BenQProjector(Logger &logger, HardwareSerial &in, HardwareSerial &out, int pollIntervalSecs, int minOnSecs, int minOffSecs) :
	logger(logger),
	in(in), out(out), nextSend(0),
	pollInterval(pollIntervalSecs * 1000), nextUpdate(0),
	// don't queue more polling messages if the queue is larger than will be clared out during one
	// interval (i.e., polling every 1000ms and sending every 100, cap the queue at 10 messages)
	maxQueueSizeForPoll(pollInterval / PROJECTOR_SEND_INTERVAL),
	minOnMs(minOnSecs * 1000), minOffMs(minOffSecs * 1000),
	lastOn(0), lastOff(0),
	last10s(0), last60s(0), last360s(0) {
}

void BenQProjector::begin() {
	last10s = last60s = last360s = millis();

	updateState();
}

void BenQProjector::loop() {
	auto now = millis();
	if (now >= nextUpdate && sendQueue.size() < maxQueueSizeForPoll) {
		updateState();
		nextUpdate = now + pollInterval;
	}

	// read and process any incoming data
	bool handledData = checkForRecv();

	// if we didn't handle any incoming data, process our send queue (to keep loops short)
	if (!handledData) {
		checkForSend();
	}

	// update send/recv stats
	if (now > (last10s + 10000)) {
		sendStats.last10s = sendStats.current10s;
		sendStats.last10sRate = 1000.0f * sendStats.last10s / (now - last10s);
		sendStats.current10s = 0;

		recvStats.last10s = recvStats.current10s;
		recvStats.last10sRate = 1000.0f * recvStats.last10s / (now - last10s);
		recvStats.current10s = 0;

		last10s = now;
	}

	if (now > (last60s + 60000)) {
		sendStats.last60s = sendStats.current60s;
		sendStats.last60sRate = 1000.0f * sendStats.last60s / (now - last60s);
		sendStats.current60s = 0;

		recvStats.last60s = recvStats.current60s;
		recvStats.last60sRate = 1000.0f * recvStats.last60s / (now - last60s);
		recvStats.current60s = 0;

		last60s = now;
	}

	if (now > (last360s + 360000)) {
		sendStats.last360s = sendStats.current360s;
		sendStats.last360sRate = 1000.0f * sendStats.last360s / (now - last360s);
		sendStats.current360s = 0;

		recvStats.last360s = recvStats.current360s;
		recvStats.last360sRate = 1000.0f * recvStats.last360s / (now - last360s);
		recvStats.current360s = 0;

		last360s = now;
	}
}

void BenQProjector::updateState() {
	queueQuery("pow");

	if (state.isOn) {
		// the projector only lets us query these items when it's on
		queueQuery("sour");
		queueQuery("vol");
		queueQuery("mute");
		queueQuery("lampm");
		queueQuery("blank");
		queueQuery("freeze");

		// model name won't change, but we can't get it when it's off
		if (state.modelName[0] == 0 || state.modelName[0] == '?') {
			queueQuery("modelname");
		}
	}

	if (state.isOn || state.lampHours == 0) {
		// we can query lamp hours when projector is off, but only do that if we don't know yet
		// (since it won't go up otherwise while off)
		queueQuery("ltim");
	}
}


bool BenQProjector::checkForRecv() {
	bool gotMessage = false;

	// read data, while there is some to read
	while (!gotMessage && in.available() > 0) {
		char read = in.read();

		// in case we get a really long message, guard against overflow by throwing it away
		if (recvBuffer.idx == PROJECTOR_RECV_BUFFER_SIZE) {
			if (read == '\r') {
				recvBuffer.data[PROJECTOR_RECV_BUFFER_SIZE - 1] = 0;

				stringstream log;
				log << "Dropping message longer than " << PROJECTOR_RECV_BUFFER_SIZE << " bytes; message began with: " << recvBuffer.data;
				logger.error(log.str());

				// looks like we ended the message, so reset the buffer and continue on our way
				recvBuffer.idx = 0;
			}

			// just keep reading and dumping now
			continue;
		}

		recvBuffer.data[recvBuffer.idx++] = read;

		// spec says messages are delimited by \r, but my projector was giving me \r\n
		if (read == '\r' || read == '\n') {
			int readBytes = recvBuffer.idx;
			recvBuffer.data[readBytes - 1] = 0; // NULL out the termination character we read
			recvBuffer.idx = 0; // reset our receive buffer for next time

			if (readBytes > 1) {
				// if we read more than just that termination character, attempt to process it
				gotMessage = true;
				char *msg = recvBuffer.data;

				// count any echo or valid-looking message
				if (msg[0] == '>' || msg[0] == '*') {
					recvStats.total++;
					recvStats.current10s++; recvStats.current60s++; recvStats.current360s++;
				}

				if (msg[0] == '>') {
					// projector is echos commands back prefixed with a > character
					// trim off the >* and # pre/postfix and log it
					msg[readBytes - 2] = 0; // trim off the # at the end
					msg += 2; // skip ahead 2 more bytes, past the >*

					logger.commEcho(msg);
					continue;
				}

				logger.commRecv(msg);

				// start processing the message!
				int idx = 0;

				// messages begin with a *
				if (msg[idx] != '*') {
					// invalid message
					logger.error("Received message didn't start with '*'; gibberish?");

					if (!isprint(msg[0])) {
						stringstream log;
						log << "(first character was unprintable, ASCII=" << (int)msg[0] << ")";
						logger.debug(log.str());
					}

					continue;
				}

				// check for error conditions
				if (strncasecmp(msg, "*Illegal format#", 6) == 0) {
					// we sent a bad message
					logger.error("Illegal format response from projector; did we send an incorrectly formatted message?");
				}

				if (strncasecmp(msg, "*Block item#", 12) == 0) {
					// whatever command we tried to run can't be run now
					logger.info("Command was blocked by projector; incorrect projector state?");
					continue;
				}

				if (strncasecmp(msg, "*Unsupported item#", 18) == 0) {
					// we did something that's not supported
					logger.error("Unsupported response from projector");
					continue;
				}

				// move past the *
				idx++;
				
				// start reading the key, it will end with an = or a # (if there was no value)
				char key[16] = "";
				for (int i = 0; idx < readBytes && i < 15 && msg[idx] != '=' && msg[idx] != '#'; ++idx, ++i) {
					key[i] = msg[idx];
				}

				if (msg[idx] != '=' && msg[idx] != '#') {
					// we didn't get an = or a #, so bad message?
					logger.error("Was expecting '=' or '#', key/message too long?");
					continue;
				}

				if (msg[idx] == '=') {
					// we did get the =, so continue on
					idx++;
				}

				// if we had an =, start reading the value (up to the #)
				char value[32] = "";
				if (msg[idx - 1] == '=') {
					for (int i = 0; idx < readBytes && i < 31 && msg[idx] != '#'; ++idx, ++i) {
						value[i] = msg[idx];
					}
				}

				if (msg[idx] != '#') {
					// the message didn't end with a # like we expected, so bad message?
					logger.error("Was expecting '#', value/message too long?");
					continue;
				}

				// now, handle the state update for the message we got
				receiveValue(key, value);
			}
		}
	}
}

void BenQProjector::receiveValue(const char *key, const char *value) {
	if (strcasecmp(key, "pow") == 0) {
		bool nextOn = strcasecmp(value, "on") == 0;

		if (!state.initialized) {
			// take the first power state no matter what
			if (nextOn) {
				state.statusStr = "On";
				lastOn = millis();
			} else {
				state.statusStr = "Off";
				lastOff = millis();
			}

			stringstream log;
			log << "Took initial power value of " << nextOn << " from projector";
			logger.debug(log.str());

			state.isOn = nextOn;
			state.isTransitioning = false;
			state.initialized = true;
		} else if (state.isOn && !nextOn) {
			// projector reports off, so clear values back to defaults
			state.isOn = false;
			state.isTransitioning = true;
			strcpy(state.source, "none");
			strcpy(state.lampMode, "off");
			state.isMuted = state.isImageBlanked = state.isImageFrozen = false;
			state.volume = 0;
			state.targetVolume = -1;

			// we say powering off here because, at least with my projector, we may see another
			// 'on' followed by some illegal state messages, then finally an off once it finishes
			// cooling off
			state.statusStr = "Powering off...";

			logger.info("Looks like the projector is powering off");

			lastOff = millis();
		} else if (!state.isOn) {
			if (nextOn && (millis() - lastOff) > PROJECTOR_POWER_OFF_TIME) {
				// if we see an on AFTER the power off time interval, obey
				state.isOn = true;
				state.isTransitioning = false;
				state.statusStr = "On";

				logger.info("Looks like the projector has powered on");

				lastOn = millis();
			} else if (!nextOn && state.isTransitioning) {
				// if we see our second off at any point, we can switch the status to 'off'
				state.isTransitioning = false;
				state.statusStr = "Off";

				logger.info("Looks like the projector has finished powering off");
			}
		}
	} else if (strcasecmp(key, "sour") == 0) {
		strcpy(state.source, value);
	} else if (strcasecmp(key, "mute") == 0) {
		state.isMuted = strcasecmp(value, "on");
	} else if (strcasecmp(key, "vol") == 0) {
		state.volume = atoi(value);
		checkVolume();
	} else if (strcasecmp(key, "lampm") == 0) {
		strcpy(state.lampMode, value);
	} else if (strcasecmp(key, "blank") == 0) {
		state.isImageBlanked = strcasecmp(value, "on") == 0;
	} else if (strcasecmp(key, "freeze") == 0) {
		state.isImageFrozen = strcasecmp(value, "on") == 0;
	} else if (strcasecmp(key, "ltim") == 0) {
		state.lampHours = atoi(value);
	} else if (strcasecmp(key, "modelname") == 0) {
		strcpy(state.modelName, value);
	}
}

void BenQProjector::checkVolume() {
	// we have to adjust one at a time to get to our target volume
	//TODO: guard against invalid volumes causing us to continually spam vol-/+
	if (state.targetVolume >= 0) {
		stringstream log;
		log << "Volume is " << state.volume << ", target is " << state.targetVolume << ", updating...";
		logger.debug(log.str());

		if (state.targetVolume > state.volume) {
			queueValue("vol", "+");
		} else if (state.targetVolume < state.volume) {
			queueValue("vol", "-");
		} else {
			// we're there!
			state.targetVolume = -1;
		}
	}
}


void BenQProjector::queueRaw(const char *raw) {
	char *toQueue = new char[strlen(raw) + 1]; // room for the null termination
	strcpy(toQueue, raw);
	sendQueue.push(toQueue);
}

void BenQProjector::queueValue(const char *key, const char *value) {
	char *toQueue = new char[strlen(key) + strlen(value) + 2]; // room for the null termination
	strcpy(toQueue, key);
	strcat(toQueue, "=");
	strcat(toQueue, value);
	sendQueue.push(toQueue);
}

void BenQProjector::queueQuery(const char *key) {
	queueValue(key, "?");
}

bool BenQProjector::checkForSend() {
	// don't flood the projector by sending too much; otherwise messages get dropped and things end
	// up getting corrupted
	auto now = millis();
	if (now >= nextSend && !sendQueue.empty()) {
		// send next from queue
		const char *toSend = sendQueue.front();
		sendQueue.pop();

		out.print("\r*");
		out.print(toSend);
		out.print("#\r");

		logger.commSent(toSend);
		nextSend = now + PROJECTOR_SEND_INTERVAL;

		sendStats.total++;
		sendStats.current10s++; sendStats.current60s++; sendStats.current360s++;

		delete toSend;
		return true;
	}
	
	return false;
}




bool BenQProjector::isInitialized() {
	return state.initialized;
}

void BenQProjector::turnOn() {
	if ((millis() - lastOff) > minOffMs) {
		logger.debug("Turning projector on...");
		queueValue("pow", "on");
	} else {
		logger.info("Ignoring projector-on request, it hasn't been off long enough");
	}
}
void BenQProjector::turnOff() {
	if ((millis() - lastOn) > minOnMs) {
		logger.debug("Turning projector off...");
		queueValue("pow", "off");
	} else {
		logger.info("Ignoring projector-off request, it hasn't been on long enough");
	}
}
bool BenQProjector::isOn() {
	return state.isOn;
}
long BenQProjector::getLastOnTime() {
	return lastOn;
}
long BenQProjector::getLastOffTime() {
	return lastOff;
}

const char *BenQProjector::getStatusStr() {
	return state.statusStr;
}

void BenQProjector::setSource(const char *source) {
	queueValue("sour", source);
}
const char *BenQProjector::getSource() {
	return state.source;
}

bool BenQProjector::isMuted() {
	return state.isMuted;
}

void BenQProjector::setVolume(int volume) {
	state.targetVolume = volume;
	checkVolume();
}
int BenQProjector::getVolume() {
	// publicly, report the volume we're working on achieving
	return state.targetVolume >= 0 ? state.targetVolume : state.volume;
}

void BenQProjector::setLampMode(const char *mode) {
	queueValue("lampm", mode);
}
const char *BenQProjector::getLampMode() {
	return state.lampMode;
}

int BenQProjector::getLampHours() {
	return state.lampHours;
}

void BenQProjector::setImageBlank(bool blank) {
	queueValue("blank", blank ? "on" : "off");
}
bool BenQProjector::isImageBlanked() {
	return state.isImageBlanked;
}

void BenQProjector::setImageFreeze(bool freeze) {
	queueValue("freeze", freeze ? "on" : "off");
}
bool BenQProjector::isImageFrozen() {
	return state.isImageFrozen;
}

const char *BenQProjector::getModelName() {
	return state.modelName;
}

void BenQProjector::getSendStats(long &total, int &count10s, int &count60s, int &count360s, float &rate10s, float &rate60s, float &rate360s) {
	total = sendStats.total;
	count10s = sendStats.last10s;
	count60s = sendStats.last60s;
	count360s = sendStats.last360s;
	rate10s = sendStats.last10sRate;
	rate60s = sendStats.last60sRate;
	rate360s = sendStats.last360sRate;
}

void BenQProjector::getRecvStats(long &total, int &count10s, int &count60s, int &count360s, float &rate10s, float &rate60s, float &rate360s) {
	total = recvStats.total;
	count10s = recvStats.last10s;
	count60s = recvStats.last60s;
	count360s = recvStats.last360s;
	rate10s = recvStats.last10sRate;
	rate60s = recvStats.last60sRate;
	rate360s = recvStats.last360sRate;
}
