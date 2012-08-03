#include "Gossip.h"

Define_Module(Gossip);

void Gossip::startup(){
	int nodeStartupDiff = ( (int)par("nodeStartupDiff") ) * self;
	stringstream out;
	string temp;
	packetsSent = 0;
	gossipMsg = 0;
	neighbourCheckInterval = STR_SIMTIME(par("neighbourCheckInterval"));
	gossipInterval = STR_SIMTIME(par("gossipInterval"));

	out << nodeStartupDiff;
	temp = out.str();
	temp += "ms";
	setTimer( GET_NEIGHBOUR, STR_SIMTIME( temp.c_str() ) );

	setTimer( START_GOSSIP, STR_SIMTIME( temp.c_str() ) );
	if(self == 0){
		gossipMsg = GOSSIP;
	}
}

void Gossip::timerFiredCallback(int type){
	switch (type){

		case GET_NEIGHBOUR: {
			//trace() << "Request neighbours." ;
			toNetworkLayer(createGossipDataPacket(PULL_NEIGHBOUR, 0, packetsSent++), BROADCAST_NETWORK_ADDRESS);
			setTimer(GET_NEIGHBOUR, neighbourCheckInterval);
			break;
		}

		case START_GOSSIP: {
			trace() << "Start gossip.";
			doGossip();
			setTimer(START_GOSSIP, gossipInterval);
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
	int peer = atoi(source), i;
	bool found = false;

	switch((int)msgType){

	case PULL_NEIGHBOUR:
		//PUSH selfIP: Temporarily receiver will figure out this from the source IP, add sourceIP to packet later.
		toNetworkLayer(createGossipDataPacket(PUSH_NEIGHBOUR, 0 , packetsSent++), source);
		break;

	case PUSH_NEIGHBOUR:
		//PULL response received, update neighbour list
		//trace() << "Neighbour " << peer << "reported back.";
		found = false;
		for(i = 0; i < peers.size(); i++)
			if(peer == peers[i]){
				found = true; break;
			}

		if(!found)
			peers.push_back(peer);
		break;

	case GOSSIP:
		if( rcvPacket->getExtraData() > gossipMsg ) {
			trace() << self << " infected.";
			gossipMsg = rcvPacket->getExtraData();
		}else {
			trace() << "Ignoring gossip msg.";
		}
		break;

	default:
		trace() << "Incorrect packet received.";
		break;

	}
}

void Gossip::doGossip(){
	int i;
	stringstream out;
	string neighbour;

	for(i = 0; i < 4 && i < peers.size(); i++) {
		out.str(string());
		out << peers[i]; neighbour = out.str();
		//trace() << "Pushing to " << neighbour ;
		toNetworkLayer( createGossipDataPacket( GOSSIP, gossipMsg , packetsSent++ ), neighbour.c_str() );
	}
}

GossipPacket* Gossip::createGossipDataPacket(double data, int extra, unsigned int seqNum)
{
	GossipPacket *newPacket = new GossipPacket("Gossip Msg", APPLICATION_PACKET);
	newPacket->setData(data);
	newPacket->setExtraData (extra);
	newPacket->setSequenceNumber(seqNum);
	return newPacket;
}

void Gossip::finishSpecific(){
	int i;
	trace() << "Gossip msg " << gossipMsg;
	for(i = 0; i < peers.size(); i++) {
		trace() << "Peer No." << peers[i];
	}
}
