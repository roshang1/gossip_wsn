#include "Gossip.h"

Define_Module( Gossip);

void Gossip::startup() {
	int nodeStartupDiff = ((int) par("nodeStartupDiff")) * self;
	stringstream out;
	string temp, temp2;
	PEERINFO myInfo;

	duplicates = noResponse = lateResponse = droppedRequests = rounds = wait = expectedSeq = packetsSent = 0;
	gSend = gReceive = gRespond = 0;
	roundsBeforeStopping = par("stopGossipAfter") ;
	neighbourCheckInterval = STR_SIMTIME(par("neighbourCheckInterval"));
	gossipInterval = ((int) par("nodeStartupDiff")) * (18);
	trace() << gossipInterval;
	myInfo.id = self;
	myInfo.neighbourCount = peers.size(); //Count yourself as well
	peers.push_back(myInfo);

	//Node 0 takes a value 1.
	//TO DO: Multiple nodes can take value 1; therefore, several gossips may be in progress.
	gossipMsg = (self == 0) ? 1 : 0;
	//gossipMsg = par("gossipMsg");
	expectedReceived = isBusy = false;

	assignNeighbours(self);
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
	trace() << temp2;
	setTimer(START_GOSSIP, STR_SIMTIME(temp2.c_str()));

	temp2 = "5s";
	setTimer(SAMPLE_AVG, STR_SIMTIME( temp2.c_str() ));

	 declareOutput("Wasted Requests");
	 declareOutput("Stats");
}

void Gossip::timerFiredCallback(int type) {
	stringstream out;
	string temp2;
	PEERINFO myInfo;
	vector<PEERINFO>::iterator it;
	vector<int> delIndices;
	vector<PEERINFO> keepPeers;
	double sum = 0.0;
	int i =0;
	bool peersModified = false;

	switch (type) {



	  case SAMPLE_AVG:{
		  trace() << "Current Avg = " << gossipMsg << " Rounds = " << rounds;
		  temp2 = "5s";
		  setTimer(SAMPLE_AVG, STR_SIMTIME( temp2.c_str() ));
		 break;
	  }

	case GET_NEIGHBOUR: {
		//trace() << "Request neighbours.";
		//Skip the first element which is the node itself.
		keepPeers.push_back(peers[0]);
		for (it = peers.begin() + 1, i = 1; it != peers.end(); ++it, i++) {
			(*it).staleness++;
			if((*it).staleness <= 3)
				keepPeers.push_back((*it)); // Still good, keep it.
			else
				peersModified = true; //A peer got deleted.
		}

		peers.clear();
		peers.resize(keepPeers.size());
		copy(keepPeers.begin(), keepPeers.end(), peers.begin());

		for (it = newPeers.begin(); it != newPeers.end(); ++it) {
			peers.push_back((*it));
		}

		peers.at(0).neighbourCount = peers.size() - 1;
		//Skip the first element which is the node itself.
		for (it = peers.begin() + 1; it != peers.end(); ++it) {
			(*it).weight = (double) 1 / (double) (1 + ((*it).neighbourCount > peers.at(0).neighbourCount ? (*it).neighbourCount : peers.at(0).neighbourCount));
			sum += (*it).weight;
			//(self == 2 || self == 3 || self == 27) &&  trace() << "Id = " << (*it).id << ", weight = " << (*it).weight	<< ", count = " << (*it).neighbourCount;
		}

		peers.at(0).weight = (double) 1 - sum;
		//(self == 2 || self == 3 || self == 27) && trace() << "Id = " << peers.at(0).id << ", weight = " << peers.at(0).weight << ", count = " << peers.at(0).neighbourCount;
		for (it = peers.begin() + 1; it != peers.end(); ++it) {
			(*it).weight = (* (it - 1) ).weight + (*it).weight;
		//(self == 2 || self == 3 || self == 27) && trace() << "Id = " << (*it).id << ", weight = " << (*it).weight << ", count = " << (*it).neighbourCount;
		}
		//(self == 2 || self == 3 || self == 27) && trace() << "Id = " << peers.at(0).id << ", weight = " << peers.at(0).weight << ", count = " << peers.at(0).neighbourCount;

		/*if( ( !peersModified && newPeers.size() == 0 ) && ( neighbourCheckInterval < 120.0 )) {
				neighbourCheckInterval = neighbourCheckInterval + STR_SIMTIME(par("neighbourCheckInterval"));
				(self == 2 || self == 3 || self == 27) && trace() << "Increase neighbourCheckInterval to " << neighbourCheckInterval;
		}else if( neighbourCheckInterval > 5 ) {
				neighbourCheckInterval = neighbourCheckInterval - STR_SIMTIME(par("neighbourCheckInterval"));
				(self == 2 || self == 3 || self == 27) && trace() << "Decrease neighbourCheckInterval to " << neighbourCheckInterval;
		}else{
			(self == 2 || self == 3 || self == 27) && trace() << "No change in neighbourCheckInterval";
		}*/
		newPeers.clear();
		////toNetworkLayer(createGossipDataPacket(PULL_NEIGHBOUR, packetsSent++), BROADCAST_NETWORK_ADDRESS);
		////setTimer(GET_NEIGHBOUR, neighbourCheckInterval);
		break;
	}

	case START_GOSSIP: {
		//trace() << "Start gossip";
		//if (wait == 2 || !isBusy) {
			if(expectedSeq != -1 && isBusy)
				noResponse++;
			//Dequeue
			expectedSeq = -1;
			deQueue();
			int dest = getPeer();
			//(self == 2 || self == 3 || self == 27)  && trace() << "Gossip with " << dest;
			if (dest != -1 && dest != self) {
				GossipInfo send;
				stringstream out;
				string neighbour;

				isBusy = true;
				expectedReceived = false;
				out << dest;
				neighbour = out.str();
				//(self == 2 || self == 3 || self == 27) && trace() << "Send " << gossipMsg << " to " << dest;
				send.data = gossipMsg;
				send.seq = expectedSeq =  packetsSent++;
				gSend++;
				toNetworkLayer(createGossipDataPacket(GOSSIP_PULL, send, expectedSeq), neighbour.c_str());
				wait = 0;
			}
		//} else
			//wait++;

		//if( roundsBeforeStopping != 0 ){
			out.str(string());
			out << gossipInterval;
			temp2 = out.str();
			temp2 += "ms";
			setTimer(START_GOSSIP, STR_SIMTIME(temp2.c_str()));
		//}
		//else {
			//trace() << "Stop gossip at " << self;
			//isBusy = false;
		//}
		break;
	}
	}
}


void Gossip::enQueue(GOSSIP_EXCH_MSG peer) {
	waitQueue.push(peer);
}

void Gossip::deQueue() {
	simtime_t currentTime = getClock();
	GOSSIP_EXCH_MSG msg;
	simtime_t timeOut = gossipInterval * 1.5;
	simtime_t waitTime;
	GossipInfo sendData;
	int i = 0;
	int size = waitQueue.size(); //Take a snapshot, the size may keep increasing.
	stringstream out;
	string neighbour;

	for(i = 0; i < size; i++){
		msg = waitQueue.front(); waitQueue.pop();
		waitTime = currentTime - msg.receivedAt;
		if(waitTime < timeOut) {
			gossipMsg = (gossipMsg + msg.data) / 2; //Update Avg
			sendData.data = gossipMsg;
			sendData.seq = msg.seq;

			out << msg.initiator;
			neighbour = out.str();
			gRespond++;
			toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, sendData, packetsSent++), neighbour.c_str());
			//(self == 2 || self == 3 || self == 27) && trace() << "Dequeued requests: New avg  " << sendData.data << ", is being sent to " << msg.initiator;
		}else{
			droppedRequests++;
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
	GOSSIP_EXCH_MSG msg;
	int peer = atoi(source), i;
	PEERINFO neighbour;
	bool found = false;

	//trace() << "Received ";

	switch ((int) msgType) {

	case PULL_NEIGHBOUR:
		//PUSH selfIP: Temporarily receiver will figure out this from the source IP, add sourceIP to packet later.
		toNetworkLayer(createGossipDataPacket(PUSH_NEIGHBOUR,(int) peers.size() - 1, packetsSent++), source);
		break;

	case PUSH_NEIGHBOUR:
		//PULL response received, update neighbour list if new peer is discovered.
		for(i = 0; i < peers.size(); i++) {
			if(peers.at(i).id == peer) {
				found = true;
				peers.at(i).staleness = 0;
				peers.at(i).neighbourCount = rcvPacket->getNeighbourCount();
			}
		}
		if(!found) {
			neighbour.id = peer;
			neighbour.staleness = 0;
			neighbour.neighbourCount = rcvPacket->getNeighbourCount();
			//trace() << "New neighbour " << neighbour.id;
			newPeers.push_back(neighbour);
		}
		break;

	case GOSSIP_PULL:
		if (isBusy) {
			//Enqueue
			msg.initiator = peer;
			msg.data = receivedData.data;
			msg.seq = receivedData.seq;
			msg.receivedAt = getClock();
			enQueue(msg);
			//(self == 2 || self == 3 || self == 27) && trace() << "Add to queue "	<< source;
		} else {
			//Restart stopped gossip if required.
			//(self == 2 || self == 3 || self == 27) && trace() << "roundsBeforeStopping " << roundsBeforeStopping << " gossipMsg " << gossipMsg << " receivedData.data " << receivedData.data;
			/*if( roundsBeforeStopping <= 0 && (gossipMsg != receivedData.data)) {
				roundsBeforeStopping = (int) par("stopGossipAfter");
				setTimer(START_GOSSIP, gossipInterval);
				isBusy = false;
				trace() << "Restart gossip at " << self;
			}*/
			//Calculate avg, and share.
			extraData.seq = receivedData.seq;
			gossipMsg = extraData.data = (gossipMsg + receivedData.data) / 2; //Update Avg, synchronize gossipMsg if different threads perform updates.
			gRespond++;
			toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, extraData, packetsSent++), source);
			//(self == 2 || self == 3 || self == 27) && trace() << "New avg  " << extraData.data << ", is being sent to " << source;
			//TO DO: Suppose gossip is stopped, and this node receives several different values from other nodes. Restart gossip in such a case.
		}
		break;

	case GOSSIP_PUSH:
		if (receivedData.seq == expectedSeq) {
			gReceive++;
			//expectedSeq = -1;
			//Update avg, send ACK
			if( (gossipMsg - receivedData.data) != 0) {
				gossipMsg = receivedData.data;
				//(self == 2 || self == 3 || self == 27) && trace() << "Update avg to " << gossipMsg << ", peer was " << source;
			} else {
				roundsBeforeStopping--;
			}
			isBusy = false; //As busy is set to false, timer will take care of dequeueing accumulated requests.
			if(!expectedReceived){
				expectedReceived = true;
				rounds++;
			}else{
				duplicates++;
			}
		} else
			lateResponse++;
		break;
	default:
		trace() << "Incorrect packet received.";
		break;
	}
}

int Gossip::getPeer() {
	double randNum;
	vector<PEERINFO>::iterator it;

	if (peers.size() > 1) {
		randNum = (double) rand() / (double) RAND_MAX;
		//Communicate with node if randNum < CDF for that node.
		for (it = peers.begin(); it != peers.end(); ++it)
			if (randNum < (*it).weight)
				return (it == peers.begin()) ? -1 : (*it).id;
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
	trace() << "Final Avg = " << gossipMsg << " Rounds = " << rounds;
	trace() << "gSend = " << gSend << ", gReceive = " << gReceive << ", gRespond = " << gRespond;
	for (i = 0; i < peers.size(); i++) {
		trace() << "Peer No." << peers[i].id;
	}
	collectOutput("Wasted Requests", "Dropped Requests", droppedRequests);
	collectOutput("Wasted Requests", "Received Late response for", lateResponse);
	collectOutput("Stats", "Sent", gSend);
	collectOutput("Stats", "Got Back", gReceive);
	collectOutput("Stats", "Responded to", gRespond);
	collectOutput("Stats", "No Response", noResponse);
	collectOutput("Stats", "Duplicates", duplicates);
}

void Gossip::assignNeighbours (int id) {
	PEERINFO neighbour;
	switch (id) {

	case 0:
		neighbour.id = 1;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 2;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 3;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 4;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		break;
	case 1:
		neighbour.id = 0;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		newPeers.push_back(neighbour);

		neighbour.id = 6;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		newPeers.push_back(neighbour);

		neighbour.id = 7;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);
		break;

	case 2:
		neighbour.id = 0;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		newPeers.push_back(neighbour);

		neighbour.id = 8;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		newPeers.push_back(neighbour);

		neighbour.id = 7;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);
		break;

	case 3:
		neighbour.id = 0;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		newPeers.push_back(neighbour);

		neighbour.id = 6;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		newPeers.push_back(neighbour);

		neighbour.id = 5;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		newPeers.push_back(neighbour);
		break;

	case 4:
		neighbour.id = 0;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		newPeers.push_back(neighbour);

		neighbour.id = 8;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		newPeers.push_back(neighbour);

		neighbour.id = 5;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		newPeers.push_back(neighbour);
		break;

	case 5:
		neighbour.id = 3;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 4;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);
		break;

	case 6:
		neighbour.id = 3;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 1;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);
		break;

	case 8:
		neighbour.id = 4;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 2;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);
		break;

	case 7:
		neighbour.id = 1;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 2;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 9;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);
		break;

	case 9:
		neighbour.id = 7;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 10;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 12;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);
		break;

	case 13:
		neighbour.id = 10;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);


		neighbour.id = 12;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 14;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);

		neighbour.id = 16;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		newPeers.push_back(neighbour);
		break;

	case 16:
		neighbour.id = 13;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		newPeers.push_back(neighbour);

		neighbour.id = 15;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		newPeers.push_back(neighbour);

		neighbour.id = 17;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		newPeers.push_back(neighbour);
		break;

	case 14:
			neighbour.id = 13;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 4;
			newPeers.push_back(neighbour);

			neighbour.id = 11;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 2;
			newPeers.push_back(neighbour);

			neighbour.id = 17;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 2;
			newPeers.push_back(neighbour);
			break;

	case 12:
			neighbour.id = 13;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 4;
			newPeers.push_back(neighbour);

			neighbour.id = 9;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 3;
			newPeers.push_back(neighbour);

			neighbour.id = 15;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 2;
			newPeers.push_back(neighbour);
			break;

	case 10:
			neighbour.id = 0;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 4;
			newPeers.push_back(neighbour);

			neighbour.id = 9;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 3;
			newPeers.push_back(neighbour);

			neighbour.id = 11;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 2;
			newPeers.push_back(neighbour);
			break;

	case 15:
			neighbour.id = 12;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 3;
			newPeers.push_back(neighbour);

			neighbour.id = 16;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 3;
			newPeers.push_back(neighbour);
			break;

	case 11:
			neighbour.id = 14;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 3;
			newPeers.push_back(neighbour);

			neighbour.id = 10;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 3;
			newPeers.push_back(neighbour);
			break;

	case 17:
			neighbour.id = 16;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 3;
			newPeers.push_back(neighbour);

			neighbour.id = 14;
			neighbour.staleness = 0;
			neighbour.neighbourCount = 3;
			newPeers.push_back(neighbour);
			break;
	}
}
