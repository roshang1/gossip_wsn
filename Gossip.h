
#ifndef _GOSSIP_H_
#define _GOSSIP_H_

#include "VirtualApplication.h"
#include <vector>

using namespace std;

enum GOSSIP_CONTROL_MSGS {
	PULL_NEIGHBOUR = 1,
	PUSH_NEIGHBOUR = 2,
	GOSSIP = 3
};

enum GOSSIP_TIMERS {
	GET_NEIGHBOUR = 1,
	START_GOSSIP = 2
};

class Gossip: public VirtualApplication {
	private:
		int packetsSent;
		int neighbourCount;
		int neighbourCheckInterval;
		vector<int> peers;
		int gossipMsg;
	protected:
		void startup();
		void finishSpecific();
		void timerFiredCallback(int);
		void handleSensorReading(SensorReadingMessage *);
		void handleNeworkControlMessage(cMessage *);
		void fromNetworkLayer(ApplicationPacket *, const char *, double, double);

		void doGossip();
};
#endif
