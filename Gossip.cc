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
	int H = unifRandom() * (maxH -2) + 2;
	//trace() << " H " << H;
	return H;
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
	//trace() << "x " << x << " y " << y;
	return T;
}

void Gossip::startup() {
	int i;
	stringstream out;
	string temp;
	PEERINFO myInfo;

	int nodeStartupDiff = ((int) par("nodeStartupDiff")) * self;
	//nodeStartupDiff += 2000;
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
	myInfo.neighbourCount = 0; //Count yourself as well
	myInfo.x = mobilityModule->getLocation().x;
	myInfo.y = mobilityModule->getLocation().y;
	peers.push_back(myInfo);

	//assignNeighbours(self);
	// Start sharing neighbours after some initial interval.
	out.str(string()); out << nodeStartupDiff; temp = out.str(); temp += "ms";
	setTimer(GET_NEIGHBOUR, STR_SIMTIME(temp.c_str()));

	// Start gossiping after some initial interval.
	out.str(string()); nodeStartupDiff += 10000;
	out << nodeStartupDiff;	temp = out.str(); temp += "ms";
	setTimer(START_GOSSIP, STR_SIMTIME(temp.c_str()));


	temp = "5000ms";
	setTimer(SAMPLE_VALUE, STR_SIMTIME(temp.c_str()));


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
		string temp = "5000ms";
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

		peers.at(0).neighbourCount = peers.size() - 1;
		toNetworkLayer(createGossipDataPacket(PULL_NEIGHBOUR, packetsSent++), BROADCAST_NETWORK_ADDRESS);
		setTimer(GET_NEIGHBOUR, neighbourCheckInterval);
		break;
	}

	case START_GOSSIP: {
			double* T = drawT();
			int H = drawH();
			stringstream out;
			int dest = getPeer(T);
			if (dest != -1) {
				//trace() << "Sending ";
				//trace() << "to " << dest ;
				GossipInfo *send = new GossipInfo();
				string neighbour;

				out << dest; neighbour = out.str();

				send->H = H - 1;
				send->T[0] = T[0];
				send->T[1] = T[1];
				send->data[0] = ( ( (double) send->H )/ H ) * si;
				send->data[1] = ( ( (double) send->H )/ H ) * wi;
				//(self == 1 || self == 9 ) && trace() << si << " " << wi << " " << si/wi;
				//(self == 1 || self == 9 ) && trace() << "Sending";
				//(self == 1 || self == 9 ) && trace() << send->data[0] << " " << send->data[1];
				si = si / H; wi = wi / H;
				//(self == 1 || self == 9 ) && trace() << si << " " << wi << " " << si/wi;
				toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, *send, packetsSent++), neighbour.c_str());
				gSend++;
			}
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
		//(self == 1 || self == 9 ) && trace() << "Received";
		//(self == 1 || self == 9 ) && trace() << receivedData.data[0] << " " << receivedData.data[0];
		//(self == 1 || self == 9 ) && trace() << si << " " << wi << " " << si/wi;
		si += (receivedData.data[0] / receivedData.H);
		wi += (receivedData.data[1] / receivedData.H);
		//(self == 1 || self == 9 ) && trace() << si << " " << wi << " " << si/wi;
		if(receivedData.H != 1) {
			stringstream out;
			int dest = getPeer(receivedData.T);
			if (dest != -1) {
				//trace() << "Forwarding ";
				//trace() << "to " << dest ;
				GossipInfo *send = new GossipInfo();
				string neighbour;

				out << dest; neighbour = out.str();

				send->H = receivedData.H - 1;
				send->T[0] = receivedData.T[0];
				send->T[1] = receivedData.T[1];

				send->data[0] = ( ( (double) send->H )/ receivedData.H ) *  receivedData.data[0];
				send->data[1] = ( ( (double) send->H )/ receivedData.H ) *  receivedData.data[1];

				//si = si / H; wi = wi / H;
				toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, *send, packetsSent++), neighbour.c_str());
				gForward++;
			}else{
				//(self == 1 || self == 9 ) && trace() << si << " " << wi << " " << si/wi;
				si += ( ( (double) receivedData.H - 1 )/ receivedData.H ) *  receivedData.data[0];
				wi += ( ( (double) receivedData.H - 1 )/ receivedData.H ) *  receivedData.data[1];
				//(self == 1 || self == 9 ) && trace() << si << " " << wi << " " << si/wi;
			}
		} else {

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
bool Gossip::diffuseSum(int H, double* T){
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

		//si = si / H; wi = wi / H;
		toNetworkLayer(createGossipDataPacket(GOSSIP_PUSH, *send, packetsSent++), neighbour.c_str());
		return true;
	}
	return false;
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
		neighbour.x = 20.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 5;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 2;
		break;

	case 1:
		neighbour.id = 0;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		neighbour.x = 0.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 2;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 40.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 6;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 2:
		neighbour.id = 1;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 20.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 3;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 60.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 7;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 3:
		neighbour.id = 2;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 40.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 4;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		neighbour.x = 80.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 8;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 4:
		neighbour.id = 3;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 60.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 9;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 2;
		break;

	case 5:
		neighbour.id = 0;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		neighbour.x = 0.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 6;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 10;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 6:
		neighbour.id = 1;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 20.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 5;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 7;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 11;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 7:
		neighbour.id = 2;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 40.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 6;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 8;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 12;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 8:
		neighbour.id = 3;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 60.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 7;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 9;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 13;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 9:
		neighbour.id = 4;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		neighbour.x = 80.0;
		neighbour.y = 0.0;
		peers.push_back(neighbour);

		neighbour.id = 8;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 14;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 10:
		neighbour.id = 5;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 11;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 15;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 11:
		neighbour.id = 6;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 10;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 12;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 16;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 12:
		neighbour.id = 7;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 11;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 13;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 17;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 13:
		neighbour.id = 8;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 12;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 14;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 18;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 14:
		neighbour.id = 9;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 20.0;
		peers.push_back(neighbour);

		neighbour.id = 13;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 19;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 15:
		neighbour.id = 10;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 16;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 20;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		neighbour.x = 0.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 16:
		neighbour.id = 11;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 15;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 17;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 21;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 20.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 17:
		neighbour.id = 12;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 16;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 18;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 22;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 40.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 18:
		neighbour.id = 13;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 17;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 19;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 23;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 60.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 4;
		break;

	case 19:
		neighbour.id = 14;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 40.0;
		peers.push_back(neighbour);

		neighbour.id = 18;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 24;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		neighbour.x = 80.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 20:
		neighbour.id = 15;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 0.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 21;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 20.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 2;
		break;

	case 21:
		neighbour.id = 16;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 20.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 20;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		neighbour.x = 0.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		neighbour.id = 22;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 40.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 22:
		neighbour.id = 17;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 40.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 21;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 20.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		neighbour.id = 23;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 60.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 23:
		neighbour.id = 18;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 4;
		neighbour.x = 60.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 22;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 40.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		neighbour.id = 24;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 2;
		neighbour.x = 80.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 3;
		break;

	case 24:
		neighbour.id = 19;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 80.0;
		neighbour.y = 60.0;
		peers.push_back(neighbour);

		neighbour.id = 23;
		neighbour.staleness = 0;
		neighbour.neighbourCount = 3;
		neighbour.x = 60.0;
		neighbour.y = 80.0;
		peers.push_back(neighbour);

		peers.at(0).neighbourCount = 2;
		break;
	}
}
