#ifndef COMPNET_LAB4_SRC_SWITCH_H
#define COMPNET_LAB4_SRC_SWITCH_H

#include "types.h"
#include <map>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <iterator>

class SwitchBase
{
public:
	SwitchBase() = default;
	~SwitchBase() = default;

	virtual void InitSwitch(int numPorts) = 0;
	virtual int ProcessFrame(int inPort, char *framePtr) = 0;
};

extern SwitchBase *CreateSwitchObject();

// TODO : Implement your switch class.

struct mac_equal_to
{
	bool operator()(const mac_addr_t &mac1, const mac_addr_t &mac2) const
	{
		for (int i = 0; i < ETH_ALEN; ++i)
			if (mac1[i] != mac2[i])
				return false;
		return true;
	}
};

struct mac_hash
{
	std::size_t operator()(const mac_addr_t &mac) const
	{

		std::size_t h0 = std::hash<uint8_t>()(mac[0]);
		std::size_t h1 = std::hash<uint8_t>()(mac[1]);
		std::size_t h2 = std::hash<uint8_t>()(mac[2]);
		std::size_t h3 = std::hash<uint8_t>()(mac[3]);
		std::size_t h4 = std::hash<uint8_t>()(mac[4]);
		std::size_t h5 = std::hash<uint8_t>()(mac[5]);

		return h0 ^ h1 ^ h2 ^ h3 ^ h4 ^ h5;
	}
};

class EthernetSwitch : public SwitchBase
{
	/*{ mac_addr: (port, counter) }*/
	std::unordered_map<mac_addr_t, std::pair<int, int>, mac_hash, mac_equal_to> macTable;
	int portNum;

public:
	void InitSwitch(int numPorts) override;
	int ProcessFrame(int inPort, char *framePtr) override;
};

#endif // ! COMPNET_LAB4_SRC_SWITCH_H
