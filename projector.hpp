#ifndef PROJECTOR_HPP
#define PROJECTOR_HPP

// default buffer size of 64 bytes
#define PROJECTOR_RECV_BUFFER_SIZE 64

// default send interval of 100ms
#define PROJECTOR_SEND_INTERVAL 100

// default poweroff dead time of 2 minutes
#define PROJECTOR_POWER_OFF_TIME 120000

#include "logger.hpp"

#include <queue>

#include <HardwareSerial.h>

/**
 * Helper for handling RS232 communication with the projector.
 */
class BenQProjector {
public:

	/**
	 * Set up the projector interface. Accepts separate HardwareSerial references for sending and
	 * receiving because, at least on the ESP8266, the second hardware serial port is send-only.
	 * Due to this, we may end up talking _to_ the projector on one serial port and receiving back
	 * _from_ the projector on a second serial port, depending on how things are wired up WRT also
	 * being able to talk to the computer.
	 * 
	 * This implementation assumes that the serial ports will have been configured as necessary
	 * (with the correct baud rate, etc) and are ready for communicating with the projector when
	 * begin() is called.
	 **/
	BenQProjector(Logger &logger, HardwareSerial &in, HardwareSerial &out, int pollIntervalSecs);

	void begin();
	void loop();

	void queueRaw(const char *raw);
	void queueValue(const char *key, const char *value);
	void queueQuery(const char *key);

	bool isInitialized();

	void turnOn();
	void turnOff();
	bool isOn();
	long getLastOnTime();
	long getLastOffTime();

	const char *getStatusStr();

	void setSource(const char *source);
	const char *getSource();

	void mute();
	void unMute();
	bool isMuted();

	void setVolume(int volume);
	int getVolume();

	void setLampMode(const char *mode);
	const char *getLampMode();

	int getLampHours();
	
	void setImageBlank(bool blank);
	bool isImageBlanked();

	void setImageFreeze(bool freeze);
	bool isImageFrozen();

	const char *getModelName();

	void getSendStats(long &total, int &count10s, int &count60s, int &count360s, float &rate10s, float &rate60s, float &rate360s);
	void getRecvStats(long &total, int &count10s, int &count60s, int &count360s, float &rate10s, float &rate60s, float &rate360s);

private:

	Logger &logger;
	HardwareSerial &in, &out;
	std::queue<const char *> sendQueue;
	long nextSend;
	int pollInterval;
	long nextUpdate;
	int maxQueueSizeForPoll;
	long lastOn, lastOff;

	// just for fun, keep stats of message throughput
	struct {
		long total = 0;
		int current10s = 0, last10s = 0;
		int current60s = 0, last60s = 0;
		int current360s = 0, last360s = 0;
		float last10sRate = 0, last60sRate = 0, last360sRate = 0;
	} sendStats;
	struct {
		long total = 0;
		int current10s = 0, last10s = 0;
		int current60s = 0, last60s = 0;
		int current360s = 0, last360s = 0;
		float last10sRate = 0, last60sRate = 0, last360sRate = 0;
	} recvStats;
	long last10s, last60s, last360s;

	struct {
		bool initialized = false;
		bool isOn = false, isTransitioning = false;
		char *statusStr = "off";
		char source[16] = "none";
		bool isMuted = false;
		int volume = 0, targetVolume = -1;
		char lampMode[8] = "off";
		int lampHours = 0;
		bool isImageBlanked = false;
		bool isImageFrozen = false;
		char modelName[32] = "";
	} state;

	struct {
		char data[PROJECTOR_RECV_BUFFER_SIZE];
		int idx = 0;
		bool overflow = false;
	} recvBuffer;

	void updateState();
	void receiveValue(const char *key, const char *value);
	void checkVolume();

	bool checkForSend();
	bool checkForRecv();
};

#endif