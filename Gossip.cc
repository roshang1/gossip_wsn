#include "Gossip.h"

Define_Module( Gossip);

void Gossip::startup() {
	int nodeStartupDiff = ((int) par("nodeStartupDiff")) * self;
	stringstream out;
	string temp, temp2;
	PEERINFO myInfo;

	wait = expectedSeq = currentPeerIndex = packetsSent = 0;
	neighbourCheckInterval = STR_SIMTIME(par("neighbourCheckInterval"));
	gossipInterval = STR_SIMTIME(par("gossipInterval"));
	myInfo.id = self;
	myInfo.neighbourCount = peers.size() + 1; //Count yourself as well
	peers.push_back(myInfo);

	//Node 0 takes a value 1.
	//TO DO: Multiple nodes can take value 1; therefore, several gossips may be in progress.
	gossipMsg = (self == 0) ? 1 : 0;
	isBusy = false;

	// Start sharing neighbours after some initial interval.
	out << nodeStartupDiff;
	temp = out.str();
	temp += "ms";
	setTimer(GET_NEIGHBOUR, STR_SIMTIME(temp.c_str()));

	// Start gossiping after some initial interval.
	out.str(string());
	nodeStartupDiff += 200;
	out << nodeStartupDiff;
	temp2 = out.str();
	temp2 += "ms";
	setTimer(START_GOSSIP, STR_SIMTIME(temp2.c_str()));
}

void Gossip::timerFiredCallback(int type) {
	PEERINFO myInfo;
	vector<PEERINFO>::iterator it;
	double sum = 0.0;
	switch (type) {

	case GET_NEIGHBOUR: {
		trace() << "Request neighbours.";
		peers.clear();
		peers.resize(newPeers.size());
		copy(newPeers.begin(), newPeers.end(), peers.begin());

		myInfo.id = self;
		myInfo.neighbourCount = peers.size() + 1; //Count yourself as well
		for (it = peers.begin(); it != peers.end(); ++it) {
			(*it).weight = (double) 1 / (double) (1 + ((*it).neighbourCount
					> myInfo.neighbourCount ? (*it).neighbourCount
					: myInfo.neighbourCount));
			sum += (*it).weight;
			trace() << "Id = " << (*it).id << ", weight = " << (*it).weight
					<< ", count = " << (*it).neighbourCount;
		}
		myInfo.weight = (double) 1 - sum;
		peers.push_back(myInfo);
		trace() << "Id = " << myInfo.id << ", weight = " << myInfo.weight
				<< ", count = " << myInfo.neighbourCount;

		newPeers.clear();
		toNetworkLayer(createGossipDataPacket(PULL_NEIGHBOUR, packetsSent++),
				BROADCAST_NETWORK_ADDRESS);
		setTimer(GET_NEIGHBOUR, neighbourCheckInterval);
		break;
	}

	case START_GOSSIP: {
		//    trace() << "Start gossip";
		if (wait == 2 || !isBusy) {
			int dest = getPeer();
			trace() << "Gossip with " << dest;
			if (dest != -1 && dest != self) {
				GossipInfo send;
				stringstream out;
				string neighbour;

				isBusy = true;
				out << dest;
				neighbour = out.str();
				trace() << "Send " << gossipMsg << " to " << dest;
				send.data = gossipMsg;
				send.seq = expectedSeq = self;
				toNetworkLayer(createGossipDataPacket(GOSSIP_PULL, send,
						packetsSent++), neighbour.c_str());
				wait = 0;
			}
		} else
			wait++;
		setTimer(START_GOSSIP, gossipInterval);
		break;
	}
	}
}

void Gossip::handleSensorReading(SensorReadingMessage * reading) {

}

void Gossip::handleNeworkControlMessage(cMessage * msg) {

}

void Gossip::fromNetworkLayer(ApplicationPacket * genericPacket,
	const char *source, double rssi, double lqi) {
	GossipPacket *rcvPacket = check_and_cast<GossipPacket*> (genericPacket);
	double msgType = rcvPacket->getData();
	GossipInfo receivedData = rcvPacket->getExtraData();
	GossipInfo extraData;
	int peer = atoi(source), i;
	PEERINFO neighbour;

	switch ((int) msgType) {

	case PULL_NEIGHBOUR:
		//PUSH selfIP: Temporarily receiver will figure out this from the source IP, add sourceIP to packet later.
		toNetworkLayer(createGossipDataPacket(PUSH_NEIGHBOUR,
				(int) peers.size(), packetsSent++), source);
		break;

	case PUSH_NEIGHBOUR:
		//PULL response received, update neighbour list if new peer is discovered.
		//    trace() << "Neighbour " << peer << "reported back.";
		neighbour.id = peer;
		neighbour.neighbourCount = rcvPacket->getNeighbourCount();
		newPeers.push_back(neighbour);
		break;

	case GOSSIP_PULL:
		if (isBusy) {
			//Send BUSY signal.
			trace() << self << " is busy.";
			toNetworkLayer(createGossipDataPacket(GOSSIP_BUSY, extraData,
					packetsSent++), source);
		} else {
			//Calculate avg, and share.
			isBusy = true;
			extraData.seq = expectedSeq = receivedData.seq;
			extraData.data = (gossipMsg + receivedData.data) / 2;

			toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, extraData,
					packetsSent++), source);
			trace() << "New avg " << extraData.data << " is being sent to "
					<< source;
		}
		break;

	case GOSSIP_PUSH:
		if (receivedData.seq == expectedSeq) {
			//Update avg, send ACK
			extraData.data = gossipMsg = receivedData.data;
			extraData.seq = expectedSeq;

			toNetworkLayer(createGossipDataPacket(GOSSIP_ACK, extraData,
					packetsSent++), source);
			isBusy = false;
			trace() << "Update avg to " << extraData.data << ", send ACK to "
					<< source;
		}
		break;

	case GOSSIP_BUSY:
		//Peer was busy, this node shall try on next trigger.
		isBusy = false;
		trace() << "Peer " << source << " was busy";
		break;

	case GOSSIP_ACK:
		if (receivedData.seq == expectedSeq) {
			//ACK recieved, update avg.
			gossipMsg = receivedData.data;
			isBusy = false;
			trace() << "ACK received, new avg = " << gossipMsg;
		}
		break;

	default:
		trace() << "Incorrect packet received.";
		break;
	}
}

int Gossip::getPeer() {
	double randNum;

	if (peers.size() > 1) {
		currentPeerIndex = (currentPeerIndex + 1) % peers.size(); //Uniformly select peers
		randNum = (double) rand() / (double) RAND_MAX;
		//Communicate with given probability
		if (randNum < peers.at(currentPeerIndex).weight)
			return peers[currentPeerIndex].id;
		else
			return -1;
	}
	return -1;
}

GossipPacket* Gossip::createGossipDataPacket(double data, unsigned int seqNum) {
	GossipPacket *newPacket =
			new GossipPacket("Gossip Msg", APPLICATION_PACKET);
	newPacket->setData(data);
	newPacket->setSequenceNumber(seqNum);
	return newPacket;
}

GossipPacket* Gossip::createGossipDataPacket(double data, GossipInfo extra,
		unsigned int seqNum) {
	GossipPacket *newPacket = createGossipDataPacket(data, seqNum);
	newPacket->setExtraData(extra);
	return newPacket;
}

GossipPacket* Gossip::createGossipDataPacket(double data, int neighbourCount,
		unsigned int seqNum) {
	GossipPacket *newPacket = createGossipDataPacket(data, seqNum);
	newPacket->setNeighbourCount(neighbourCount);
	return newPacket;
}

void Gossip::finishSpecific() {
	int i;
	trace() << "Gossip msg " << gossipMsg;
	for (i = 0; i < peers.size(); i++) {
		trace() << "Peer No." << peers[i].id;
	}
}
