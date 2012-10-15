#ifndef _GOSSIP_H_
#define _GOSSIP_H_

#include "VirtualApplication.h"
#include <vector>
#include <queue>
#include <map>
#include <cmath>
#include <algorithm>
#include "GossipPacket_m.h"


using namespace std;

#define EPSILON 0.0001
#define arrayLength(a) ( sizeof ( a ) / sizeof ( *a ) )

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
	GET_NEIGHBOUR = 1, START_GOSSIP = 2, SAMPLE_VALUE = 3, PROCESS_SAMPLES = 4
};

struct peerInfo {
	int id, neighbourCount, staleness;
	double x, y;
};


typedef struct peerInfo PEERINFO;

class Gossip: public VirtualApplication {
private:
	int packetsSent;
	simtime_t neighbourCheckInterval, gossipInterval;
	//To do: Implement using a list, it's a better option.
	vector<PEERINFO> peers;
	vector<PEERINFO> newPeers;
	double si, wi;
	short roundsBeforeStopping;
	int topX, topY;
	int numOfNodes, maxH;
	int gSend, gReceive, gForward;
	cModule *node, *wchannel, *network;

	bool compareDouble(double num1, double num2);
	void assignNeighbours (int id);
	double unifRandom();
	int drawH();
	double* drawT();
	void diffuseSum(int H, double* T);

protected:
	void startup();
	void finishSpecific();
	void timerFiredCallback(int);
	void handleSensorReading(SensorReadingMessage *);
	void handleNeworkControlMessage(cMessage *);
	void fromNetworkLayer(ApplicationPacket *, const char *, double, double);

	int getPeer(double* T);
	GossipPacket* createGossipDataPacket(double, unsigned int);
	GossipPacket* createGossipDataPacket(double, GossipInfo& , unsigned int);
	GossipPacket* createGossipDataPacket(double, NodeInfo& , unsigned int);
};
#endif
