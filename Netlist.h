#pragma once
#include "utils.h"

class Node
{
public:
	Node()
	{
		m_bPO = false;
		m_nOrigIndex = 0;
		m_nConstPI = -1;
	}
	vector<int> m_vecnSucc;
	vector<int> m_vecnPred;
	vector<bool> m_vecbPredComp;
	vector<int> m_vecnPredPI;
	vector<bool> m_vecbPredPIComp;
	unsigned int m_nIndex;
	unsigned int m_nOrigIndex;
	bool m_bPO;
	int m_nPartition;
	bool m_bMAJ;
	int m_nConstPI;
	set<int> m_setnConePI;
};


class NetList
{
public:
	NetList() 
	{
		m_nArrayRow = 253;
		m_nNumArray = 4;
	}
	NetList(vector<Node> vecNode, vector<Node> vecIn, vector<int> vecnPo);
	void ReadFromFile(string strFile);
	void ConfigWithXMG(int nOrigOffset = 1000000);
	void ConfigForNOR(string strFile);

	void ConfigMF();
	xmg_network m_net;
	int m_nOffset;
	vector<Node> m_vecNode;
	vector<Node> m_vecIn;
	vector<int> m_vecnPO;
	vector<int> m_vecnSchedule;
	vector<int> m_vecnPOIndex;
	vector<bool> m_vecbPOComp;
	int m_nMF;
	int m_nSize;
	int m_nCross;
	int m_nNumArray;
	int m_nArrayRow;
	int m_nNumPI;
	string m_strBench;
};