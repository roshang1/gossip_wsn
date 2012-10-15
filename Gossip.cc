#include "Gossip.h"

Define_Module( Gossip);

double Gossip::unifRandom()
{
	double random = rand() / double(RAND_MAX);/*
	while(random == 0)
		random = rand() / double(RAND_MAX);*/
    return random;
}

int Gossip::drawH() {
	return unifRandom() * (maxH -2) + 2;
}

double* Gossip::drawT() {
	double x,y;
	double myX = mobilityModule->getLocation().x;
	double myY = mobilityModule->getLocation().y;
	double* T = new double[2];
	int range = pow ( (int)par("nodeSeparation"), 2);
	double dist;

	while(true) {
		x = unifRandom() * topX;
		y = unifRandom() * topY;
		dist = pow((x - myX), 2) + pow((y - myY), 2);
		if( dist > range)
			break;
	}
	T[0] = x;
	T[1] = y;
	return T;
}

void Gossip::startup() {
	int i;
	stringstream out;
	string temp;
	PEERINFO myInfo;

	int nodeStartupDiff = ((int) par("nodeStartupDiff")) * self;
	nodeStartupDiff += 2000;
	si = unifRandom() * 100000;
	wi = 1.0;

	cModule* node = getParentModule();
	cModule* network = node->getParentModule();
	numOfNodes = network->par("numNodes");
	topX = network->par("field_x");
	topY = network->par("field_y");
	int nodeSeparation = par("nodeSeparation");
	maxH = (int)( (topX + topY) / nodeSeparation );

	trace() << "xi " << si  << " " << wi << " " << maxH;

	roundsBeforeStopping = par("stopGossipAfter");
	neighbourCheckInterval = STR_SIMTIME(par("neighbourCheckInterval"));
	gossipInterval = STR_SIMTIME(par("gossipInterval"));

	packetsSent = gSend = gReceive = gForward = 0;

	myInfo.id = self;
	myInfo.neighbourCount = 1; //Count yourself as well
	myInfo.x = mobilityModule->getLocation().x;
	myInfo.y = mobilityModule->getLocation().y;
	peers.push_back(myInfo);

	// Start sharing neighbours after some initial interval.
	out.str(string()); out << nodeStartupDiff; temp = out.str(); temp += "ms";
	setTimer(GET_NEIGHBOUR, STR_SIMTIME(temp.c_str()));

	// Start gossiping after some initial interval.
	out.str(string()); nodeStartupDiff += 200;
	out << nodeStartupDiff;	temp = out.str(); temp += "ms";
	setTimer(START_GOSSIP, STR_SIMTIME(temp.c_str()));

	/*temp = "500ms";
	setTimer(SAMPLE_VALUE, STR_SIMTIME(temp.c_str()));
*/
	declareOutput("Wasted Requests");
	declareOutput("Stats");
}

void Gossip::timerFiredCallback(int type) {
	PEERINFO myInfo;
	vector<PEERINFO>::iterator it;
	vector<PEERINFO> keepPeers;
	stringstream out;
	double sum = 0.0;
	int i = 0;
	bool peersModified = false;

	switch (type) {

	case SAMPLE_VALUE: {
		string temp = "500ms";
		trace() << si / wi;
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

/*		for (it = newPeers.begin(); it != newPeers.end(); ++it)
			peers.push_back((*it));*/

		peers.at(0).neighbourCount = peers.size() - 1;
		//Skip the first element which is the node itself.
		/*for (it = peers.begin() + 1; it != peers.end(); ++it) {
			(*it).weight = (double) 1 / (double) (1 + ((*it).neighbourCount	> peers.at(0).neighbourCount ?
					(*it).neighbourCount : peers.at(0).neighbourCount));
			sum += (*it).weight;
		}

		peers.at(0).weight = (double) 1 - sum;
		for (it = peers.begin() + 1; it != peers.end(); ++it)
			(*it).weight = (*(it - 1)).weight + (*it).weight;

		if( ( !peersModified && newPeers.size() == 0 ) && ( neighbourCheckInterval < 120.0 )) {
		 neighbourCheckInterval = neighbourCheckInterval + STR_SIMTIME(par("neighbourCheckInterval"));
		 (self == 2 || self == 3 || self == 27) && trace() << "Increase neighbourCheckInterval to " << neighbourCheckInterval;
		 }else if( neighbourCheckInterval > 5 ) {
		 neighbourCheckInterval = neighbourCheckInterval - STR_SIMTIME(par("neighbourCheckInterval"));
		 (self == 2 || self == 3 || self == 27) && trace() << "Decrease neighbourCheckInterval to " << neighbourCheckInterval;
		 }else{
		 (self == 2 || self == 3 || self == 27) && trace() << "No change in neighbourCheckInterval";
		 }
		newPeers.clear();*/
		toNetworkLayer(createGossipDataPacket(PULL_NEIGHBOUR, packetsSent++), BROADCAST_NETWORK_ADDRESS);
		setTimer(GET_NEIGHBOUR, neighbourCheckInterval);
		break;
	}

	case START_GOSSIP: {
			double* T = drawT();
			int H = drawH();
			trace() << "Sending ";
			diffuseSum(H, T);
			gSend++;
			setTimer(START_GOSSIP, gossipInterval);
		break;
	}
	}
}

void Gossip::fromNetworkLayer(ApplicationPacket * genericPacket, const char *source, double rssi, double lqi) {
	GossipPacket *rcvPacket = check_and_cast<GossipPacket*> (genericPacket);
	double msgType = rcvPacket->getData();
	GossipInfo receivedData = rcvPacket->getExtraData();
	NodeInfo rcvdNeighbourInfo = rcvPacket->getNodeInfo();
	NodeInfo sendInfo;
	int peer = atoi(source), i;
	PEERINFO neighbour;
	bool found = false;

	switch ((int) msgType) {

	case PULL_NEIGHBOUR:
		// TO DO: Add random backoff, all nodes will respond at the same time. (?Do you have to? The lower layers should handle this.)
		//PUSH selfIP: Temporarily receiver will figure out this from the source IP, add sourceIP to packet later.
		sendInfo.neighbourCount =  (int) peers.size() - 1;
		sendInfo.x = mobilityModule->getLocation().x;
		sendInfo.y = mobilityModule->getLocation().y;
		toNetworkLayer(createGossipDataPacket(PUSH_NEIGHBOUR, sendInfo, packetsSent++), source);
		break;

	case PUSH_NEIGHBOUR:
		//PULL response received, update neighbour list if new peer is discovered.
		for (i = 0; i < peers.size(); i++) {
			if (peers.at(i).id == peer) {
				found = true;
				peers.at(i).staleness = 0;
				peers.at(i).neighbourCount = rcvdNeighbourInfo.neighbourCount;
				peers.at(i).x = rcvdNeighbourInfo.x;
				peers.at(i).y = rcvdNeighbourInfo.y;
				break;
			}
		}
		if (!found) {
			neighbour.id = peer;
			neighbour.staleness = 0;
			neighbour.neighbourCount = rcvdNeighbourInfo.neighbourCount;
			neighbour.x = rcvdNeighbourInfo.x;
			neighbour.y = rcvdNeighbourInfo.y;
			peers.push_back(neighbour);
		}
		break;

	case GOSSIP_PUSH:
		gReceive++;
		si = si + receivedData.data[0];
		wi = wi + receivedData.data[1];
		if(receivedData.H != 1) {
			trace() << "Forwarding ";
			diffuseSum(receivedData.H, receivedData.T);
			gForward++;
		}
		break;

	default:
		trace() << "Incorrect packet received.";
		break;
	}
}

/**
 * Need synchronization to use this method. timerFiredCallBack or fromNetworkLayer may call this method simultaneously.
 */
void Gossip::diffuseSum(int H, double* T){
	stringstream out;
	int dest = getPeer(T);
	if (dest != -1) {
		trace() << "to " << dest ;
		GossipInfo *send = new GossipInfo();
		string neighbour;

		out << dest; neighbour = out.str();

		send->H = H - 1;
		send->T[0] = T[0];
		send->T[1] = T[1];
		send->data[0] = (double (send->H / H) ) * si;
		send->data[1] = (double (send->H / H) ) * wi;

		si = si / H; wi = wi / H;
		toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, *send, packetsSent++), neighbour.c_str());
	}
}

bool Gossip::compareDouble(double num1, double num2) {
	return (fabs(num1 - num2) <= EPSILON);
}

int Gossip::getPeer(double* T) {
	double x = mobilityModule->getLocation().x;
	double y = mobilityModule->getLocation().y;
	double minDist = pow((x - T[0]), 2) + pow((y - T[1]), 2);
	vector<int> peersCloseToT;
	vector<PEERINFO>::iterator it;
	double dist;

	for (it = peers.begin() + 1; it != peers.end(); ++it)
	{
		dist = pow((it->x - T[0]), 2) + pow((it->y - T[1]), 2);
		if(dist < minDist)
			peersCloseToT.push_back(it->id);
	}

	if(peersCloseToT.size() == 0)
		return -1;

	int idx = unifRandom() * (peersCloseToT.size() - 1);
	return peersCloseToT.at(idx);
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
	newPacket->setByteLength(24); // size of extradata.
	return newPacket;
}

GossipPacket* Gossip::createGossipDataPacket(double data, NodeInfo& nodeinfo, unsigned int seqNum) {
	GossipPacket *newPacket = createGossipDataPacket(data, seqNum);
	newPacket->setNodeInfo(nodeinfo);
	newPacket->setByteLength(20); // size of neighbourcount.
	return newPacket;
}

void Gossip::finishSpecific() {
	int i;

	trace() << "Si = " << si << " Wi = " << wi << " Avg = " << si/wi;
	for (i = 0; i < peers.size(); i++) {
		trace() << "Peer No." << peers[i].id;
	}
	collectOutput("Stats", "Sent ", gSend);
	collectOutput("Stats", "Received ", gReceive);
	collectOutput("Stats", "Forwarded ", gForward);
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
