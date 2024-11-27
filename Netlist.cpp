#include "Netlist.h"

NetList::NetList(vector<Node> vecNode, vector<Node> vecIn, vector<int> vecnPo)
{
	m_vecNode = vecNode;
	m_vecIn = vecIn;
	m_vecnPO = vecnPo;
}

void NetList::ReadFromFile(string strFile)
{//config XMG from input strFile
	if (strFile != "")
	{
		if (strFile.back() == 'v')
			lorina::read_verilog(strFile, verilog_reader(m_net));
		else if (strFile.back() == 'f')
			m_net = Bliff2Xmg(strFile);
		else
			m_net = Aig2Xmg(strFile);
	}
	ConfigWithXMG();
}

void NetList::ConfigWithXMG(int nOrigOffset)
{//config netlist with XMG
	m_vecIn.clear();
	m_vecNode.clear();
	m_vecnPO.clear();
	m_vecnSchedule.clear();
	unsigned int nNumGates = m_net.num_gates();
	m_nOffset = m_net.size() - nNumGates;
	m_nNumPI = m_nOffset - 1;

	int nNumTI = 0;
	if (m_nOffset > nOrigOffset)
	{
		nNumTI = m_nOffset - nOrigOffset;
		vector<Node> vecNode(nNumTI);
		m_vecIn = vecNode;
		for (unsigned int i = 0; i < nNumTI; i++)
		{
			m_vecIn[i].m_nIndex = i + nNumGates;
			m_vecIn[i].m_nOrigIndex = i + nNumGates;
		}
	}

	vector<Node> vecNode(nNumGates);
	m_vecNode = vecNode;
	for (unsigned int i = 0; i < nNumGates; i++)
	{
		m_vecNode[i].m_nIndex = i;
		m_vecNode[i].m_nOrigIndex = i;
	}

	m_net.foreach_gate([&](auto const& n)
		{
			int nIndexNow = m_net.node_to_index(n) - m_nOffset;
			if (m_net.is_maj(n))
				m_vecNode[nIndexNow].m_bMAJ = true;
			else if (m_net.is_xor3(n))
				m_vecNode[nIndexNow].m_bMAJ = false;
			else
				cout << "ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";

			m_net.foreach_fanin(n, [&](auto const& f)
				{
					bool bCompf = m_net.is_complemented(f);
					auto fNode = m_net.get_node(f);
					int fIndex = m_net.node_to_index(fNode);
					//constant or PI or TI or regular
					if (m_net.is_constant(fNode))//constant
					{
						if (bCompf)
							m_vecNode[nIndexNow].m_nConstPI = 1;
						else
							m_vecNode[nIndexNow].m_nConstPI = 0;
					}//PI or TI or regular
					else if (!m_net.is_pi(fNode))//regular
					{
						int fIndexNow = fIndex - m_nOffset;
						m_vecNode[fIndexNow].m_vecnSucc.push_back(nIndexNow);
						m_vecNode[nIndexNow].m_vecnPred.push_back(fIndexNow);
						//m_vecNode[fIndexNow].m_vecNodeSucc.push_back(&m_vecNode[nIndexNow]);
						//m_vecNode[nIndexNow].m_vecNodePred.push_back(&m_vecNode[fIndexNow]);
						m_vecNode[nIndexNow].m_vecbPredComp.push_back(bCompf);
					}//PI or TI
					else if (fIndex >= nOrigOffset) //TI
					{
						m_vecIn[fIndex - nOrigOffset].m_vecnSucc.push_back(nIndexNow);
						m_vecNode[nIndexNow].m_vecnPred.push_back(fIndex - nOrigOffset + nNumGates);
						m_vecNode[nIndexNow].m_vecbPredComp.push_back(bCompf);
					}//PI
					else
					{
						m_vecNode[nIndexNow].m_vecnPredPI.push_back(fIndex - 1);
						m_vecNode[nIndexNow].m_vecbPredPIComp.push_back(bCompf);
					}
				});
		});

	m_net.foreach_po([&](auto const& f, auto i)
		{
			int nPO = m_net.node_to_index(m_net.get_node(f)) - m_nOffset;
			int nTI = m_net.node_to_index(m_net.get_node(f)) - nOrigOffset;
			if ((nPO >= 0) && !IsInVector(nPO, m_vecnPO))
			{
				m_vecnPO.push_back(nPO);
				m_vecNode[nPO].m_bPO = true;
			}
			if ((nPO < 0) && (nTI >= 0) && !IsInVector(nTI + nNumGates, m_vecnPO))
			{
				m_vecnPO.push_back(nTI + nNumGates);
				m_vecIn[nTI].m_bPO = true;
			}
			m_vecnPOIndex.push_back(m_net.node_to_index(m_net.get_node(f)) - 1);
			m_vecbPOComp.push_back(m_net.is_complemented(f));
		});
	if (nOrigOffset == 1000000)
	{
		for (int i = 0; i < m_vecNode.size(); i++)
		{
			Node& nd = m_vecNode[i];
			for (int pi : nd.m_vecnPredPI)
				nd.m_setnConePI.insert(pi);
			for (int fi : nd.m_vecnPred)
				nd.m_setnConePI.insert(m_vecNode[fi].m_setnConePI.begin(), m_vecNode[fi].m_setnConePI.end());
		}
	}
}

void NetList::ConfigForNOR(string strFile)
{//config NOR from strFile
	cout << "Begin read\n";
	string strLine;
	ifstream fin;
	fin.open(strFile, ios::in);
	if (!fin.is_open())
	{
		cout << "No net file\n";
		return;
	}
	int nIndex = 0;
	vector<string> vecIn = { ".a(" ,".b(" ,".c(" ,".d(" };
	set<string> setPO;
	std::map<string, int> mapNameIndex;

	while (getline(fin, strLine))
	{
		Trim(strLine);
		string res = ExtractStr(strLine, "output", ",");
		if (!res.empty())
		{
			setPO.insert(res);
			while (!strLine.empty())
			{
				res = ExtractStr(strLine, ",", ",");
				if (!res.empty())
				{
					setPO.insert(res);
				}
				else
					break;
			}
			break;
		}
	}
	string res = ExtractStr(strLine, ",", ";");
	if (!res.empty())
		setPO.insert(res);
	else
	{
		while (getline(fin, strLine))
		{
			Trim(strLine);
			strLine.insert(0, ",");
			while (!strLine.empty())
			{
				string res = ExtractStr(strLine, ",", ",");
				if (!res.empty())
					setPO.insert(res);
				else
					break;
			}
			string res = ExtractStr(strLine, ",", ";");
			if (!res.empty())
			{
				setPO.insert(res);
				break;
			}
		}
	}

	while (getline(fin, strLine))
	{
		Trim(strLine);
		string strOut = ExtractStr(strLine, ".O(", ")");
		if (strOut.empty())
			continue;
		if (strLine.find(".a(") == string::npos || strLine.compare(0, 3, "buf") == 0)
			continue;
		Node nd;
		nd.m_nIndex = nIndex;
		nd.m_nOrigIndex = nIndex;
		mapNameIndex[strOut] = nIndex;
		for (string& strLeft : vecIn)
		{
			string strIn = ExtractStr(strLine, strLeft, ")");
			if ((strIn.empty()) || (mapNameIndex.find(strIn) == mapNameIndex.end()))
				continue;
			int nIn = mapNameIndex[strIn];
			nd.m_vecnPred.push_back(nIn);
			m_vecNode[nIn].m_vecnSucc.push_back(nIndex);
		}
		if (setPO.find(strOut) != setPO.end())
		{
			nd.m_bPO = true;
			m_vecnPO.push_back(nIndex);
		}
		m_vecNode.push_back(nd);
		nIndex++;
	}
	cout << "End read\n";
}


void NetList::ConfigMF()
{//config MF with scheduling result
	int nNumNode = m_vecNode.size();
	m_nSize = nNumNode;
	m_nNumPI = m_nOffset - 1;
	vector<int> vecNumUnSchedSuc(nNumNode, 0);
	vector<int> vecMemStatus;
	for (unsigned int n = 0; n < nNumNode; n++)
		vecNumUnSchedSuc[n] = m_vecNode[n].m_vecnSucc.size();
	int nMemRow;
	m_nCross = 0;
	for (int& n : m_vecnSchedule)
	{
		Node& node = m_vecNode[n];
		vector<int> vecPredArray;
		for (int& i : node.m_vecnPred)
		{
			int nPredMem = GetIndexInVector(i, vecMemStatus);
			if (nPredMem == -1)
			{
				m_nMF = -1;
				return;
			}
			vecPredArray.push_back((int)(nPredMem + m_nOffset) / m_nArrayRow);
			vecNumUnSchedSuc[i]--;
			if (vecNumUnSchedSuc[i] == 0 && !m_vecNode[i].m_bPO)
				vecMemStatus[nPredMem] = -1;
		}
		for (nMemRow = 0; nMemRow < vecMemStatus.size(); nMemRow++)
		{
			if (vecMemStatus[nMemRow] == -1)
				break;
		}
		if (nMemRow == vecMemStatus.size())
			vecMemStatus.push_back(-1);
		vecMemStatus[nMemRow] = n;
		int nNowArray = (int)(nMemRow + m_nOffset) / m_nArrayRow;
		for (int i : vecPredArray)
		{
			if (i != nNowArray)
				m_nCross++;
		}
		for (int nIn : node.m_vecnPredPI)
		{
			if (nNowArray != ((int)nIn / m_nArrayRow))
				m_nCross++;
		}
	}
	for (int i : m_vecnPO)
	{
		int nMem = GetIndexInVector(i, vecMemStatus);
		if (nMem == -1)
		{
			m_nMF = -1;
			return;
		}
	}
	m_nMF = vecMemStatus.size();
	cout << "Checking result: Size = " << m_nSize << "; MF = " << m_nMF << "\n";
}