#include "Gossip.h"

Define_Module(Gossip);

void Gossip::startup() {
  int nodeStartupDiff = ( (int)par("nodeStartupDiff") ) * self;
  stringstream out;
  string temp, temp2;

  wait = expectedSeq = currentPeerIndex = packetsSent = 0;
  neighbourCheckInterval = STR_SIMTIME(par("neighbourCheckInterval"));
  gossipInterval = STR_SIMTIME(par("gossipInterval"));
  gossipMsg = (self % 2 == 0) ? 10 : 20;
  isBusy = false;

  out << nodeStartupDiff; temp = out.str(); temp += "ms";
  setTimer( GET_NEIGHBOUR, STR_SIMTIME( temp.c_str() ) );

  out.str(string());
  nodeStartupDiff += 200; out << nodeStartupDiff; temp2 = out.str(); temp2 += "ms";
  setTimer( START_GOSSIP, STR_SIMTIME( temp2.c_str() ) );
}

void Gossip::timerFiredCallback(int type) {
  switch (type){

  case GET_NEIGHBOUR: {
    //    trace() << "Request neighbours.";
    toNetworkLayer(createGossipDataPacket(PULL_NEIGHBOUR, packetsSent++), BROADCAST_NETWORK_ADDRESS);
    setTimer(GET_NEIGHBOUR, neighbourCheckInterval);
    break;
  }

  case START_GOSSIP: {
    //    trace() << "Start gossip";
    if ( wait == 2 || !isBusy  ) {
      int dest = getPeer();
      if (dest != -1 ) {
	GossipInfo send;
	stringstream out;
	string neighbour;

	isBusy = true;      
	out << dest;
	neighbour = out.str();
	trace() << "Send " << gossipMsg << " to " << dest;
	send.data = gossipMsg;
	send.signal = GOSSIP_PULL;
       	send.seq = expectedSeq = self;
	toNetworkLayer( createGossipDataPacket( GOSSIP, send , packetsSent++),   neighbour.c_str());
	wait = 0;
      }
    } else
      wait++;
    setTimer(START_GOSSIP, gossipInterval);
    break;
  }
  }
}

void Gossip::handleSensorReading(SensorReadingMessage * reading){

}

void Gossip::handleNeworkControlMessage(cMessage * msg){

}

void Gossip::fromNetworkLayer(ApplicationPacket * genericPacket, const char *source, double rssi, double lqi){
  GossipPacket *rcvPacket = check_and_cast<GossipPacket*>(genericPacket);
  double msgType = rcvPacket->getData();
  GossipInfo receivedData = rcvPacket->getExtraData();
  GossipInfo extraData;
  int peer = atoi(source), i;
  bool found = false;

  switch((int)msgType){

  case PULL_NEIGHBOUR:
    //PUSH selfIP: Temporarily receiver will figure out this from the source IP, add sourceIP to packet later.
    toNetworkLayer(createGossipDataPacket(PUSH_NEIGHBOUR , packetsSent++), source);
    break;

  case PUSH_NEIGHBOUR:
    //PULL response received, update neighbour list if new peer is discovered.
    //    trace() << "Neighbour " << peer << "reported back.";
    found = false;
    for(i = 0; i < peers.size(); i++)
      if(peer == peers[i]){
	found = true; break;
      }

    if(!found)
      peers.push_back(peer);
    break;

  case GOSSIP:
    //    if( isBusy && expectedSeq != receivedData.seq ){
      //Send BUSY signal.
      //extraData.signal = GOSSIP_BUSY;
    //      trace() << self << " is busy.";
    //      toNetworkLayer(createGossipDataPacket(GOSSIP, extraData , packetsSent++), source);
    //    } else {
      switch ( receivedData.signal ) {
      case GOSSIP_PULL:
	if( isBusy ) {
	  //Send BUSY signal.
	  extraData.signal = GOSSIP_BUSY;
	  trace() << self << " is busy.";
          toNetworkLayer(createGossipDataPacket(GOSSIP, extraData , packetsSent++), source);
        } else {
	  //Calculate avg, and share.
	  isBusy = true;
	  extraData.seq = expectedSeq = receivedData.seq;
	  extraData.data = ( gossipMsg + receivedData.data ) / 2;
	  extraData.signal = GOSSIP_PUSH;
	
	  toNetworkLayer(createGossipDataPacket(GOSSIP, extraData , packetsSent++), source);
	  trace() << "New avg " << extraData.data << " is being sent to " << source;
	}
	break;
      
      case GOSSIP_PUSH:
	if(receivedData.seq == expectedSeq) {
	  //Update avg, send ACK
	  extraData.data = gossipMsg = receivedData.data;
	  extraData.signal = GOSSIP_ACK;
	  extraData.seq = expectedSeq;

	  toNetworkLayer(createGossipDataPacket(GOSSIP, extraData , packetsSent++), source);
	  isBusy = false;
	  trace() << "Update avg to " << extraData.data << ", send ACK to " << source;
	}
	break;
      
      case GOSSIP_BUSY:
	//Peer was busy, this node shall try on next trigger.
	isBusy = false;
	trace() << "Peer " << source << " was busy";
	break;
      
      case GOSSIP_ACK:
	if(receivedData.seq == expectedSeq) {
	  //ACK recieved, update avg.
	  gossipMsg = receivedData.data;
	  isBusy = false;
	  trace() << "ACK received, new avg = " << gossipMsg;
	}
	break;
      }
      //    }
    break;

  default:
    trace() << "Incorrect packet received.";
    break;
  }
}

/*void Gossip::doGossip(){
  int i;
  stringstream out;
  string neighbour;

  for(i = 0; i < 4 && i < peers.size(); i++) {
    out.str(string());
    out << peers[i]; neighbour = out.str();
    //trace() << "Pushing to " << neighbour ;
    toNetworkLayer( createGossipDataPacket( GOSSIP, gossipMsg , packetsSent++ ), neighbour.c_str() );
  }
  }*/

//Make this more intelligent:
// 1. Add randomization
// 2. Discard old peers
int Gossip::getPeer() {
  if (peers.size() > 0) {
    currentPeerIndex = (currentPeerIndex + 1) % peers.size();
    //    trace() << "New index: " << currentPeerIndex << " size: " << peers.size();
    return peers[currentPeerIndex];
  }
  return -1;
}

GossipPacket* Gossip::createGossipDataPacket(double data, unsigned int seqNum)
{
  GossipPacket *newPacket = new GossipPacket("Gossip Msg", APPLICATION_PACKET);
  newPacket->setData(data);
  newPacket->setSequenceNumber(seqNum);
  return newPacket;
}

GossipPacket* Gossip::createGossipDataPacket(double data, GossipInfo extra, unsigned int seqNum)
{
  GossipPacket *newPacket = createGossipDataPacket(data, seqNum);
  newPacket->setExtraData (extra);
  return newPacket;
}

void Gossip::finishSpecific(){
  int i;
  trace() << "Gossip msg " << gossipMsg;
  for(i = 0; i < peers.size(); i++) {
    trace() << "Peer No." << peers[i];
  }
}
