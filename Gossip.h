#ifndef _GOSSIP_H_
#define _GOSSIP_H_

#include "VirtualApplication.h"
#include <vector>
#include <cmath>
#include "GossipPacket_m.h"

using namespace std;

#define EPSILON 0.0001

enum GOSSIP_CONTROL_MSGS {
	PULL_NEIGHBOUR = 1,
	PUSH_NEIGHBOUR = 2,
	GOSSIP = 3,
	GOSSIP_PUSH = 4,
	GOSSIP_PULL = 5,
	GOSSIP_BUSY = 6,
	GOSSIP_ACK = 7
};

enum GOSSIP_TIMERS {
	GET_NEIGHBOUR = 1, START_GOSSIP = 2, SAMPLE_AVG = 3
};

struct peerInfo {
	int id, neighbourCount, staleness;
	double weight;
};

struct gossipExchMsg {
	int initiator;
	double data;
	simtime_t receivedAt;
};

typedef struct peerInfo PEERINFO;
typedef struct gossipExchMsg GOSSIP_EXCH_MSG;

class Gossip: public VirtualApplication {
private:
	int packetsSent;
	simtime_t neighbourCheckInterval, gossipInterval;
	//To do: Implement using a list, it's a better option.
	vector<PEERINFO> peers;
	vector<PEERINFO> newPeers;
	queue<GOSSIP_EXCH_MSG> waitQueue;
	double gossipMsg;
	bool isBusy;
	short roundsBeforeStopping;
	int expectedSeq;
	short wait;
	int rounds, lateResponse, droppedRequests;
	int gSend, gReceive, gRespond;

	bool compareDouble(double num1, double num2);
	void assignNeighbours (int id);

protected:
	void startup();
	void finishSpecific();
	void timerFiredCallback(int);
	void handleSensorReading(SensorReadingMessage *);
	void handleNeworkControlMessage(cMessage *);
	void fromNetworkLayer(ApplicationPacket *, const char *, double, double);

	int getPeer();
	GossipPacket* createGossipDataPacket(double, unsigned int);
	GossipPacket* createGossipDataPacket(double, GossipInfo, unsigned int);
	GossipPacket* createGossipDataPacket(double, int, unsigned int);
	void enQueue(GOSSIP_EXCH_MSG);
	void deQueue();
};
#endif
