#include "switch.h"

SwitchBase *CreateSwitchObject()
{
	// TODO : Your code.
	return new EthernetSwitch();
}

void EthernetSwitch::InitSwitch(int numPorts) { portNum = numPorts; }

int EthernetSwitch::ProcessFrame(int inPort, char *framePtr)
{
	ether_header_t frameHeader;
	memcpy(&frameHeader, framePtr, sizeof(frameHeader));

	if (frameHeader.ether_type == ETHER_CONTROL_TYPE)
	{
		for (auto it = macTable.begin(); it != macTable.end();)
		{
			it->second.second--;
			if (it->second.second == 0)
				macTable.erase(it++);
			else
				it++;
		}
		return -1;
	}
	else if (frameHeader.ether_type == ETHER_DATA_TYPE)
	{
		macTable[frameHeader.ether_src] = std::make_pair(inPort, ETHER_MAC_AGING_THRESHOLD);
		if (macTable.count(frameHeader.ether_dest) == 0)
		{
			return 0;
		}
		else
		{

			if (macTable[frameHeader.ether_dest].first == inPort)
				return -1;
			else
				return macTable[frameHeader.ether_dest].first;
		}
	}
	else
	{
		perror("frame type error\n");
		return -1;
	}
}
