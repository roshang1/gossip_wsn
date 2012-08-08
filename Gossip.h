
#ifndef _GOSSIP_H_
#define _GOSSIP_H_

#include "VirtualApplication.h"
#include <vector>
#include "GossipPacket_m.h"

using namespace std;

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
  GET_NEIGHBOUR = 1,
  START_GOSSIP = 2
};

class Gossip: public VirtualApplication {
 private:
  int packetsSent;
  simtime_t neighbourCheckInterval, gossipInterval;
  vector<int> peers;
  double gossipMsg;
  bool isBusy;
  short currentPeerIndex;
  int expectedSeq;
  short wait;

 protected:
  void startup();
  void finishSpecific();
  void timerFiredCallback(int);
  void handleSensorReading(SensorReadingMessage *);
  void handleNeworkControlMessage(cMessage *);
  void fromNetworkLayer(ApplicationPacket *, const char *, double, double);

  void doGossip();
  int getPeer();
  GossipPacket* createGossipDataPacket(double , unsigned int );
  GossipPacket* createGossipDataPacket(double , GossipInfo , unsigned int );
};
#endif
