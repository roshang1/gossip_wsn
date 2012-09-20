//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package node.application.gossip;

simple Gossip like node.application.iApplication {
    parameters:
        string applicationID = default("Gossip");
        bool collectTraceInfo = default(true);
        bool isSink = default(false);
		int packetHeaderOverhead = default (8);
		int priority = default (1);
		int constantDataPayload = default (18);
        
        string neighbourCheckInterval = default("5s"); 
        string gossipInterval = default("2s");
        // After every "nodeStartupDiff" time, a node starts looking up its neighbours.
        int nodeStartupDiff = default(100); // in ms, code converts int to simtime_t
        double gossipMsg = default(10);
        //Stop gossip if value remains the same after this count of updates.
        int stopGossipAfter = default(40);
    gates:
        output toCommunicationModule;
        input fromCommunicationModule;
	output toSensorDeviceManager;
	input fromSensorDeviceManager;
	input fromResourceManager;
}