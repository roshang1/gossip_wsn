#include "Gossip.h"

Define_Module( Gossip);

double Gossip::unifRandom()
{
	double random = rand() / double(RAND_MAX);
	while(random == 0)
		random = rand() / double(RAND_MAX);
    return random;
}

int Gossip::computeR(double delta, double epsilon){
  int r;

  r = (int) ((3/pow(delta, 2)) * (log(4/epsilon)));

  return r;
}

void Gossip::startup() {
	int i;
	stringstream out;
	string temp;
	PEERINFO myInfo;
	int nodeStartupDiff = ((int) par("nodeStartupDiff")) * self;
	double xi = unifRandom() * 100;
	trace() << xi;

	roundsBeforeStopping = par("stopGossipAfter");
	neighbourCheckInterval = STR_SIMTIME(par("neighbourCheckInterval"));
	gossipInterval = STR_SIMTIME(par("gossipInterval"));

	lateResponse = droppedRequests = rounds = wait = expectedSeq = packetsSent = 0;
	gSend = gReceive = gRespond = 0;
	noOfSamples = 1100;
	for(i = 0; i < noOfSamples; i++)
		//gossipMsg[i] = (self == 0) ? 0 : 1;
		gossipMsg[i] = -log(unifRandom()) / xi;
	//gossipMsg = par("gossipMsg");
	//isBusy = false;

	myInfo.id = self;
	myInfo.neighbourCount = peers.size(); //Count yourself as well
	peers.push_back(myInfo);

	//assignNeighbours(self);
	// Start sharing neighbours after some initial interval.
	out << nodeStartupDiff; temp = out.str(); temp += "ms";
	setTimer(GET_NEIGHBOUR, STR_SIMTIME(temp.c_str()));

	// Start gossiping after some initial interval.
	out.str(string()); nodeStartupDiff += 200;
	out << nodeStartupDiff;	temp = out.str(); temp += "ms";
	setTimer(START_GOSSIP, STR_SIMTIME(temp.c_str()));

	temp = "500ms";
	setTimer(SAMPLE_VALUE, STR_SIMTIME(temp.c_str()));

	declareOutput("Wasted Requests");
	declareOutput("Stats");
}

void Gossip::timerFiredCallback(int type) {
	PEERINFO myInfo;
	vector<PEERINFO>::iterator it;
	vector<int> delIndices;
	vector<PEERINFO> keepPeers;
	stringstream out;
	double sum = 0.0;
	int i = 0;
	bool peersModified = false;

	switch (type) {

	case SAMPLE_VALUE: {
/*		string temp = "500ms";
		int size = arrayLength(gossipMsg);
		for( i = 0; i < size; i++ )
			out << gossipMsg[i] << " ";

		trace() << "Values = " << out.str() << " after rounds = " << rounds;
		setTimer(SAMPLE_VALUE, STR_SIMTIME(temp.c_str()));*/
		break;
	}

	case GET_NEIGHBOUR: {
		keepPeers.push_back(peers[0]);
		for (it = peers.begin() + 1, i = 1; it != peers.end(); ++it, i++) {
			(*it).staleness++;
			if ((*it).staleness <= 3)
				keepPeers.push_back((*it)); // Still good, keep it.
			else
				peersModified = true; //A peer got deleted.
		}

		peers.clear();
		peers.resize(keepPeers.size());
		copy(keepPeers.begin(), keepPeers.end(), peers.begin());

		for (it = newPeers.begin(); it != newPeers.end(); ++it)
			peers.push_back((*it));

		peers.at(0).neighbourCount = peers.size() - 1;
		//Skip the first element which is the node itself.
		for (it = peers.begin() + 1; it != peers.end(); ++it) {
			(*it).weight = (double) 1 / (double) (1 + ((*it).neighbourCount	> peers.at(0).neighbourCount ?
					(*it).neighbourCount : peers.at(0).neighbourCount));
			sum += (*it).weight;
		}

		peers.at(0).weight = (double) 1 - sum;
		for (it = peers.begin() + 1; it != peers.end(); ++it)
			(*it).weight = (*(it - 1)).weight + (*it).weight;

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
		toNetworkLayer(createGossipDataPacket(PULL_NEIGHBOUR, packetsSent++), BROADCAST_NETWORK_ADDRESS);
		setTimer(GET_NEIGHBOUR, neighbourCheckInterval);
		break;
	}

	case START_GOSSIP: {
		//if (wait == 1 || !isBusy) {
			//Dequeue
			//expectedSeq = -1;
			//deQueue();
			int dest = getPeer();
			if (dest != -1) {
				GossipInfo send;
				string neighbour;

				isBusy = true;
				out << dest; neighbour = out.str();
				copyArray(gossipMsg, send.data);
				send.seq = expectedSeq = packetsSent++;
				gSend++;
				//(self == 0 || self == 1) && trace() << "Sending.";
				//(self == 0 || self == 1) && printArray(send.data);
				toNetworkLayer(createGossipDataPacket(GOSSIP_PULL, send, expectedSeq), neighbour.c_str());
				wait = 0;
			}
//		} else
	//		wait++;

		if (roundsBeforeStopping != 0)
			setTimer(START_GOSSIP, gossipInterval);
		else {
			trace() << "Stop gossip.";
			isBusy = false;
		}
		break;
	}
	}
}

/*void Gossip::enQueue(GOSSIP_EXCH_MSG peer) {
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

	for (i = 0; i < size; i++) {
		msg = waitQueue.front();
		waitQueue.pop();
		waitTime = currentTime - msg.receivedAt;
		if (waitTime < timeOut) {
			gossipMsg = (gossipMsg + msg.data) / 2; //Update Avg
			sendData.data = gossipMsg;
			sendData.seq = msg.seq;

			out << msg.initiator;
			neighbour = out.str();
			gRespond++;
			toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, sendData, packetsSent++), neighbour.c_str());
		} else {
			droppedRequests++;
		}
	}
}*/

void Gossip::fromNetworkLayer(ApplicationPacket * genericPacket, const char *source, double rssi, double lqi) {
	GossipPacket *rcvPacket = check_and_cast<GossipPacket*> (genericPacket);
	double msgType = rcvPacket->getData();
	GossipInfo receivedData = rcvPacket->getExtraData();
	GossipInfo sendData;
	GOSSIP_EXCH_MSG msg;
	int peer = atoi(source), i;
	PEERINFO neighbour;
	bool found = false;

	switch ((int) msgType) {

	case PULL_NEIGHBOUR:
		//PUSH selfIP: Temporarily receiver will figure out this from the source IP, add sourceIP to packet later.
		toNetworkLayer(createGossipDataPacket(PUSH_NEIGHBOUR, (int) peers.size() - 1, packetsSent++), source);
		break;

	case PUSH_NEIGHBOUR:
		//PULL response received, update neighbour list if new peer is discovered.
		for (i = 0; i < peers.size(); i++) {
			if (peers.at(i).id == peer) {
				found = true;
				peers.at(i).staleness = 0;
				peers.at(i).neighbourCount = rcvPacket->getNeighbourCount();
			}
		}
		if (!found) {
			neighbour.id = peer;
			neighbour.staleness = 0;
			neighbour.neighbourCount = rcvPacket->getNeighbourCount();
			newPeers.push_back(neighbour);
		}
		break;

	case GOSSIP_PULL:
		/*if (isBusy) {
			//Enqueue
			msg.initiator = peer;
			msg.data = receivedData.data;
			msg.seq = receivedData.seq;
			msg.receivedAt = getClock();
			enQueue(msg);
		} else {*/
			//Restart stopped gossip if required, this will certainly affect any ongoing schedule.
			if (roundsBeforeStopping <= 0 && ( !compareArray(gossipMsg, receivedData.data) )) {
				roundsBeforeStopping = (int) par("stopGossipAfter");
				trace() << "Restart gossip";
				setTimer(START_GOSSIP, gossipInterval);
				//isBusy = false;
			}
			//Calculate avg, and share.
			sendData.seq = receivedData.seq;
			//(self == 0 || self == 1) && trace() << "Received.";
			//(self == 0 || self == 1) && printArray(receivedData.data);
			gossipFunction(gossipMsg, receivedData.data);
			copyArray(gossipMsg, sendData.data);
			gRespond++;
			//(self == 0 || self == 1) && trace() << "Responding with.";
			//(self == 0 || self == 1) && printArray(sendData.data);
			toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, sendData, packetsSent++), source);
			//TO DO: Restart stopped gossip if node receives different values from other nodes.
		//}
		break;

	case GOSSIP_PUSH:
		if (receivedData.seq == expectedSeq) {
			gReceive++;
			//(self == 0 || self == 1) && trace() << "Received back.";
			//(self == 0 || self == 1) && printArray(receivedData.data);
			//Update avg, send ACK
			if ( !compareArray(gossipMsg, receivedData.data) )
				gossipFunction(gossipMsg, receivedData.data);
			else
				roundsBeforeStopping--;
			isBusy = false; //As busy is set to false, timer will take care of dequeueing accumulated requests.
			rounds++;
		} else
			lateResponse++;
		break;
	default:
		trace() << "Incorrect packet received.";
		break;
	}
}

/*
double Gossip::gossipFunction(double myValue, double neighboursValue) {
	return (myValue < neighboursValue ? myValue : neighboursValue);
}
*/

void Gossip::gossipFunction(double* myData, double* neighboursData) {
	int i;
	int size = noOfSamples;
	for( i = 0; i < size; i++ )
		if(myData[i] > neighboursData[i])
			myData[i] = neighboursData[i];
}

void Gossip::copyArray(double* src, double* dest) {
	int i;
	int size = noOfSamples;
	for( i = 0; i < size; i++ )
		dest[i] = src[i];
}

bool Gossip::compareArray(double* first, double* second) {
	int i;
	int size = noOfSamples;
	for( i = 0; i < size; i++ )
		if(!compareDouble(first[i], second[i]))
			return false;
	return true;
}

bool Gossip::printArray(double* data) {
	int i;
	stringstream out;
	int size = noOfSamples;

	for( i = 0; i < size; i++ )
		out << data[i] << " ";

	trace() << "Data: " << out.str() ;
	return true;
}

bool Gossip::compareDouble(double num1, double num2) {
	return (fabs(num1 - num2) <= EPSILON);
}

int Gossip::getPeer() {
	double randNum;
	vector<PEERINFO>::iterator it;

	if (peers.size() > 1) {
		randNum = unifRandom();
		//Communicate with node if randNum < CDF for that node.
		for (it = peers.begin(); it != peers.end(); ++it)
			if (randNum < (*it).weight)
				return (it == peers.begin()) ? -1 : (*it).id; //peers.begin() is equal to self; therefore return -1
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

GossipPacket* Gossip::createGossipDataPacket(double data, GossipInfo extra,	unsigned int seqNum) {
	GossipPacket *newPacket = createGossipDataPacket(data, seqNum);
	newPacket->setExtraData(extra);
	return newPacket;
}

GossipPacket* Gossip::createGossipDataPacket(double data, int neighbourCount, unsigned int seqNum) {
	GossipPacket *newPacket = createGossipDataPacket(data, seqNum);
	newPacket->setNeighbourCount(neighbourCount);
	return newPacket;
}

double Gossip::calculateSum() {
	int size = noOfSamples;
	int i ;
	double sum = 0.0;
	for( i = 0; i < size; i++ )
		sum += gossipMsg[i];
	return noOfSamples / sum;
}

void Gossip::finishSpecific() {
	int i;
	stringstream out;
	int size = arrayLength(gossipMsg);

	out << calculateSum();

	trace() << "Final Sum = " << out.str() << " Rounds = " << rounds;
	for (i = 0; i < peers.size(); i++) {
		trace() << "Peer No." << peers[i].id;
	}
	collectOutput("Wasted Requests", "Dropped Requests", droppedRequests);
	collectOutput("Wasted Requests", "Received Late response for", lateResponse);
	collectOutput("Stats", "Sent", gSend);
	collectOutput("Stats", "Got Back", gReceive);
	collectOutput("Stats", "Responded to", gRespond);
}

void Gossip::handleSensorReading(SensorReadingMessage * reading) {

}

void Gossip::handleNeworkControlMessage(cMessage * msg) {

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
