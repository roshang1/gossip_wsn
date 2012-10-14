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
	nodeStartupDiff += 2000;
	double xi = unifRandom() * 100;
	trace() << xi;

	roundsBeforeStopping = par("stopGossipAfter");
	neighbourCheckInterval = STR_SIMTIME(par("neighbourCheckInterval"));
	gossipInterval = STR_SIMTIME(par("gossipInterval"));

	lateResponse = droppedRequests = rounds = wait = expectedSeq = packetsSent = 0;
	gSend = gReceive = gRespond = 0;
	noOfSamples = 1000;
	msgSize = 10;
	for(i = 0; i < noOfSamples; i++){
		//gossipMsg[i] = (self == 0) ? 0 : 1;
		gossipMsg[i] = -log(unifRandom()) / xi;
		//trace() << gossipMsg[i] ;
	}
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
//	setTimer(SAMPLE_VALUE, STR_SIMTIME(temp.c_str()));
	setTimer(PROCESS_SAMPLES, STR_SIMTIME(temp.c_str()));

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
		string temp = "500ms";

		for(map<int, SAMPLES_AND_COUNT>::iterator ii = msgsFromSenders.begin(); ii != msgsFromSenders.end(); ii++)
			trace() << ii->first << " " << ii->second.count;

		setTimer(SAMPLE_VALUE, STR_SIMTIME(temp.c_str()));
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
			int dest = getPeer();
			if (dest != -1) {
				trace() << "Sending to " << dest; ;
				GossipInfo *send;
				string neighbour;
				list<int> sendLater;
				isBusy = true;
				out << dest; neighbour = out.str();

				for(i = 0; i < noOfSamples / msgSize ; i++) {
					// TO DO: Check the memory usage of the program, if its too high use an array of pointers to preallocated GossipInfo structs.
					if( unifRandom() >= 0.5 ) {
						send = new GossipInfo();
						send->start = i * 10;
						copyArray(gossipMsg, send->data, send->start, 0);
						send->seq = expectedSeq = packetsSent++;
						gSend++;
						(self == 0 || self == 1) && trace() << "Sending: " << send->start;
						//(self == 0 || self == 1) && printArray(send->data, 0);
						toNetworkLayer(createGossipDataPacket(GOSSIP_PULL, *send, expectedSeq), neighbour.c_str());
					} else
						sendLater.push_back(i);
				}

				for(list<int>::iterator ii = sendLater.begin(); ii != sendLater.end(); ii++){
					send = new GossipInfo();
					send->start = (*ii) * 10;
					copyArray(gossipMsg, send->data, send->start, 0);
					send->seq = expectedSeq = packetsSent++;
					gSend++;
					(self == 0 || self == 1) && trace() << "Sending: " << send->start;
					//(self == 0 || self == 1) && printArray(send->data, 0);
					toNetworkLayer(createGossipDataPacket(GOSSIP_PULL, *send, expectedSeq), neighbour.c_str());
				}
				wait = 0;
			}
			setTimer(START_GOSSIP, gossipInterval);

		break;
	}

	case PROCESS_SAMPLES: {
		//Process collected samples from peers, if the last message was received about more than a second ago. Then discard samples.
		//If there arent enough samples even after timeout discard the samples.

		//??TO DO: Does this need synchronization with the fromNetworkLayer method, both access msgsFromSenders simultaneously
		simtime_t timeout = STR_SIMTIME("3000ms"); //TO DO: make this configurable
		int sampleThreshold = 100; //TO DO: Find out what is a good number for required accuracy and adjust accordingly.
		list<int> removeFromMap;
		string temp;

		for(map<int, SAMPLES_AND_COUNT>::iterator ii = msgsFromSenders.begin(); ii != msgsFromSenders.end(); ii++) {
			if( (getClock() - ii->second.lastMsgReceivedAt) >= timeout) {
				//if( ii->second.count >= sampleThreshold )
					//process(ii->second.samples, ii->second.count);
					gossipFunction(ii->second.samples);
				trace() << "Erase " <<  ii->first << " " << ii->second.count;
				removeFromMap.push_back(ii->first);
			}
		}

		//Might need to acquire lock
		for(list<int>::iterator ii = removeFromMap.begin(); ii != removeFromMap.end(); ii++)
			msgsFromSenders.erase( (*ii) );
		temp = "200ms"; //TO DO : Vary this parameter and check the behaviour.
		setTimer(PROCESS_SAMPLES, STR_SIMTIME(temp.c_str()));
		break;
	}
	}
}

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
		//If map contains src, insert data.
		//else create new entry in map to store msgs.
		//(ensure that the array is deleted after processing)

		//(self == 0 || self == 1) && trace() << "Received: " << receivedData.start;
		//(self == 0 || self == 1) && printArray(receivedData.data, 0);

		SAMPLES_AND_COUNT* receivedSamples;
		//double *samples;
		if(msgsFromSenders.count(peer) == 0) {
			//(self == 0 || self == 1) && trace() << "Adding peer.";

			receivedSamples = new SAMPLES_AND_COUNT();
			//samples = new double[noOfSamples];

			fill_n(receivedSamples->samples, noOfSamples, -1.0);
			copyArray(receivedData.data, receivedSamples->samples, 0, receivedData.start);

			//receivedSamples->samples = samples;
			receivedSamples->count = msgSize;
			receivedSamples->lastMsgReceivedAt = getClock();
			msgsFromSenders[peer] = *receivedSamples;

			//(self == 0 || self == 1) && printArray(receivedSamples->samples, receivedData.start);
			//(self == 0 || self == 1) && trace() << "Count " << receivedSamples->count;
		} else {
			//(self == 0 || self == 1) && trace() << "Peer exists.";
			//(self == 0 || self == 1) && trace() << "Count before update " << msgsFromSenders[peer].count << " msg size " << msgSize;

			msgsFromSenders[peer].count += msgSize;
			msgsFromSenders[peer].lastMsgReceivedAt = getClock();
			copyArray(receivedData.data, msgsFromSenders[peer].samples, 0, receivedData.start);

			//(self == 0 || self == 1) && printArray(msgsFromSenders[peer].samples, receivedData.start);
			//(self == 0 || self == 1) && trace() << "Count " << msgsFromSenders[peer].count;
		}
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

/**
 * First argument must be gossipMsg i.e. the samples, not the data received in gossip msg.
 */
void Gossip::gossipFunction(double* neighboursData) {
	int i;
	rounds++;
	for( i = 0; i < noOfSamples; i++ )
		if(neighboursData[i] != -1.0 && gossipMsg[i] > neighboursData[i])
			gossipMsg[i] = neighboursData[i];
}

void Gossip::process(double* neighbourData, short count){
	printToCompare(neighbourData);

	if( count == noOfSamples )
		gossipFunction(neighbourData);
	else {
		int i;
		//Estimate neighbour's value
		double neighbour_xi = calculateXi(neighbourData, count);
		//Generate missing values.
		for( i = 0; i < noOfSamples; i++ ){
			if(neighbourData[i] == -1.0)
				neighbourData[i] = -log(unifRandom()) / neighbour_xi;
		}
		gossipFunction(neighbourData);
	}
}

/**
 * First argument must be gossipMsg i.e. the samples, not the data received in gossip msg.
 */
void Gossip::copyArray(double* src, double* dest, short srcStart, short destStart) {
	int i, j;
	int end = srcStart + msgSize;
	for( i = srcStart, j = destStart; i < end ; i++, j++)
		dest[j] = src[i];
}

/**
 * First argument must be gossipMsg i.e. the samples, not the data received in gossip msg.
 */
bool Gossip::compareArray(double* neighboursData, short msgStart) {
	int i;
	int end = msgStart + msgSize;
	for( i = msgStart; i < end && i < noOfSamples; i++ )
		if(!compareDouble(gossipMsg[i], neighboursData[i - msgStart]))
			return false;
	return true;
}

void Gossip::printToCompare(double* data) {
	int i;
	for( i = 0; i < noOfSamples; i++ )
		trace() << gossipMsg[i] << " " << data[i];
}

void Gossip::printArray(double* data, short msgStart) {
	int i;
	stringstream out;
	int end = msgStart + msgSize;

	for( i = msgStart; i < end && i < noOfSamples; i++ )
		out << data[i] << " ";

	trace() << "Data: " << out.str() ;
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
	GossipPacket *newPacket = new GossipPacket("Gossip Msg", APPLICATION_PACKET);
	newPacket->setData(data);
	newPacket->setSequenceNumber(seqNum);
	return newPacket;
}

GossipPacket* Gossip::createGossipDataPacket(double data, GossipInfo& extra,	unsigned int seqNum) {
	GossipPacket *newPacket = createGossipDataPacket(data, seqNum);
	newPacket->setExtraData(extra);
	newPacket->setByteLength(msgSize * 8 + 4 + 2); // size of extradata.
	return newPacket;
}

GossipPacket* Gossip::createGossipDataPacket(double data, int neighbourCount, unsigned int seqNum) {
	GossipPacket *newPacket = createGossipDataPacket(data, seqNum);
	newPacket->setNeighbourCount(neighbourCount);
	newPacket->setByteLength(4); // size of neighbourcount.
	return newPacket;
}

double Gossip::calculateXi(double* samples,short sampleCount) {
	int i ;
	double sum = 0.0;
	for( i = 0; i < noOfSamples; i++ ){
		if(samples[i] >= 0.0)
			sum += samples[i];
	}
	return sampleCount / sum;
}

void Gossip::finishSpecific() {
	int i;
	stringstream out;
	int size = arrayLength(gossipMsg);

	out << calculateXi(gossipMsg, noOfSamples);

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
