#include "Gossip.h"

Define_Module(Gossip);

void Gossip::startup(){
	packetsSent = 0;
	gossipMsg = 0;
	neighbourCheckInterval = par("neighbourCheckInterval");

	setTimer(GET_NEIGHBOUR, ( self % neighbourCheckInterval) );

	if(self == 0){
		gossipMsg = GOSSIP;
		setTimer(START_GOSSIP, 10 );
	}
}

void Gossip::timerFiredCallback(int type){
	switch (type){

		case GET_NEIGHBOUR: {
			trace() << "Request neighbours." ;
			toNetworkLayer(createGenericDataPacket(PULL_NEIGHBOUR, packetsSent++), BROADCAST_NETWORK_ADDRESS);
			setTimer(GET_NEIGHBOUR, neighbourCheckInterval);
			break;
		}

		case START_GOSSIP: {
			trace() << "Start gossip.";
			doGossip();
		}
	}
}

void Gossip::handleSensorReading(SensorReadingMessage * reading){

}

void Gossip::handleNeworkControlMessage(cMessage * msg){

}

void Gossip::fromNetworkLayer(ApplicationPacket * rcvPacket, const char *source, double rssi, double lqi){
	double msgType = rcvPacket->getData();
	int peer = atoi(source), i;
	bool found = false;

	switch((int)msgType){

	case PULL_NEIGHBOUR:
		//PUSH selfIP: Temporarily receiver will figure out this from the source IP, add sourceIP to packet later.
		toNetworkLayer(createGenericDataPacket(PUSH_NEIGHBOUR, packetsSent++), source);
		break;

	case PUSH_NEIGHBOUR:
		//PULL response received, update neighbour list
		trace() << "Neighbour " << peer << "reported back.";
		found = false;
		for(i = 0; i < peers.size(); i++)
			if(peer == peers[i]){
				found = true; break;
			}

		if(!found)
			peers.push_back(peer);
		break;

	case GOSSIP:
		if(gossipMsg == 0){ //If not infected yet, do gossip.
			gossipMsg = msgType;
			trace() << "Start gossip.";
			doGossip();
		}else {
			trace() << "Already infected, ignoring gossip msg.";
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
	for(i = 0; i < 3 && i < peers.size(); i++) {
		out.str(string());
		out << peers[i]; neighbour = out.str();
		trace() << "Pushing to " << neighbour ;
		cout << "Pushing to " << neighbour ;
		toNetworkLayer(createGenericDataPacket(GOSSIP, packetsSent++), neighbour.c_str());
	}
}

void Gossip::finishSpecific(){
	int i;
	trace() << "Gossip msg " << gossipMsg;
	for(i = 0; i < peers.size(); i++) {
		trace() << "Peer No." << peers[i];
	}
}

