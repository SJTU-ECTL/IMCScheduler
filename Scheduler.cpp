#include "Scheduler.h"

Scheduler::Scheduler()
{
	m_nBound = 0;
	m_nThread = 48;
	m_nGraphBound = 80;
	m_epsilon = 0.1;
	m_nMFLow = 0;
	m_bStop = false;
	m_dPO = 2;
}

int Scheduler::CallSMT(NetList& netlist, int nMF)
{//call SMT for sub-netlist
	vector<Node>& vecNode = netlist.m_vecNode;
	vector<Node>& vecIn = netlist.m_vecIn;
	unsigned int nNumNode = vecNode.size();
	unsigned int nTotNumNode = nNumNode + vecIn.size();
	z3::context c;
	z3::optimize s(c);

	z3::set_param("sat.threads", 2);
	z3::params p(c);
	p.set("timeout", (unsigned int)5 * 60 * 1000);
	s.set(p);
	vector<z3::expr_vector> matT;
	for (int n = 0; n < nNumNode; n++) {
		z3::expr_vector tmp(c);
		for (int t = 0; t < nNumNode + 1; t++)
			tmp.push_back(c.bool_const(("T_" + to_string(n) + "," + to_string(t)).c_str()));
		matT.push_back(tmp);
	}

	vector<z3::expr_vector> matA;
	for (int n = 0; n < nTotNumNode; n++) {
		z3::expr_vector tmp(c);
		//tmp.resize(nNumNode + 1);
		matA.push_back(tmp);
	}

	for (unsigned int n = 0; n < nNumNode; n++)
	{
		int nInCone = 0;
		int nOutCone = 0;
		for (int t = 0; t < nInCone + 1; t++)
			s.add(!matT[n][t]);
		for (unsigned int t = nNumNode; t > (nNumNode - 1 - nOutCone); t--)
			s.add(matT[n][t]);
		for (unsigned int t = nInCone + 1; t < (nNumNode - nOutCone - 1); t++)
			s.add(!matT[n][t] || matT[n][t + 1]);
	}

	for (unsigned t = 0; t < nNumNode; t++)
	{
		for (unsigned int i = 1; i < nNumNode; i++)
		{
			for (unsigned int j = 0; j < i; j++)
				s.add(matT[i][t] || !matT[i][t + 1] || matT[j][t] || !matT[j][t + 1]);
		}
	}

	for (Node& Nodei : vecNode)
	{
		unsigned int i = Nodei.m_nIndex;
		for (int j : Nodei.m_vecnSucc)
		{
			for (unsigned t = 0; t < nNumNode; t++)
				s.add(matT[i][t] || !matT[j][t + 1]);
		}
		if (Nodei.m_bPO)
		{
			for (unsigned int t = 0; t < nNumNode + 1; t++)
				matA[i].push_back(matT[i][t]);
		}
		else
		{
			for (unsigned int t = 0; t < nNumNode + 1; t++)
			{
				z3::expr_vector temp(c);
				for (int j : vecNode[i].m_vecnSucc)
					temp.push_back(!matT[j][t]);
				z3::expr tp = mk_or(temp);
				matA[i].push_back((tp && matT[i][t]));
			}
		}
	}

	for (Node& Nodei : vecIn)
	{
		unsigned int i = Nodei.m_nIndex;
		if (Nodei.m_bPO)
		{
			for (unsigned int t = 0; t < nNumNode + 1; t++)
				matA[i].push_back(c.bool_val(true));
		}
		else
		{
			for (unsigned int t = 0; t < nNumNode + 1; t++)
			{
				z3::expr_vector temp(c);
				for (int j : Nodei.m_vecnSucc)
					temp.push_back(!matT[j][t]);
				matA[i].push_back(mk_or(temp));
			}
		}
	}
	z3::expr maxR = c.int_val(0);
	z3::expr_vector vecActInT(c);
	for (unsigned int t = 0; t < nNumNode; t++)
	{
		z3::expr_vector temp(c);
		for (unsigned int n = 0; n < nTotNumNode; n++)
			temp.push_back(matA[n][t + 1]);
		vecActInT.push_back(sum(temp));
	}

	if (nMF == 0)
	{
		for (unsigned int t = 0; t < nNumNode; t++)
			maxR = max(vecActInT[t], maxR);
		s.minimize(maxR);

	}
	else
	{
		for (unsigned int t = 0; t < nNumNode; t++)
			s.add(vecActInT[t] <= nMF);
	}
	int nRet = -1;
	//m_netlist.m_vecnSchedule.clear();
	if (netlist.m_vecnSchedule.empty())
		netlist.m_vecnSchedule.assign(nNumNode, 0);
	/*
	if (nMF == 0)
		cout << "Running minimize mode\n";
	else
		cout << "Runnning feasibility mode, " << "Checking for " << nMF << " rows\n";

	clock_t start = clock();
	if (s.check() != z3::sat)
		cout << "Infeasible for " << nMF << " rows\n";
	else
	*/
	if (s.check() == z3::sat)
	{
		z3::model m = s.get_model();
		for (int n = 0; n < nNumNode; n++)
		{
			int nSched = nNumNode - m.eval(sum(matT[n])).get_numeral_int();
			//cout << "S" << n << ": " << nSched << "\n";
			netlist.m_vecnSchedule[nSched] = n;
		}
		for (unsigned int t = 0; t < nNumNode; t++)
			nRet = max(m.eval(vecActInT[t]).get_numeral_int(), nRet);
		//cout << "Feasible for " << nRet << " rows\n";
	}
	//cout << "Use: " << (clock() - start) * 1000 / double(CLOCKS_PER_SEC) << " ms\n";
	return nRet;
}

int Scheduler::BiSMT(NetList& netlist, int nUpper, int nLower)
{//binary search with SMT for sub-netlist
	int nRes = CallSMT(netlist, nLower);
	if (nRes != -1)
		return nRes;

	nRes = CallSMT(netlist, nUpper);
	if (nRes == -1)
		return -1;
	nUpper = nRes;

	while ((nUpper - nLower) > 1)
	{
		if (m_bStop)
			break;
		if (m_nMFLow >= nUpper)//tmp
			break;

		int nPres = round((nUpper + nLower) / 2.0);
		nRes = CallSMT(netlist, nPres);
		if (nRes == -1)
			nLower = max(nPres, m_nMFLow - 1);
		else
			nUpper = nRes;
	}
	return nUpper;
}

int Scheduler::PartitionILP(NetList& netlist, double dPOWeight)
{//ILP-based partition
	vector<Node>& vecNode = netlist.m_vecNode;
	vector<Node>& vecIn = netlist.m_vecIn;
	unsigned int nNumNode = (unsigned int)vecNode.size();
	cout << "Partitioning " << nNumNode << " nodes with weight = " << dPOWeight << "\n";;

	unsigned int nIn = (unsigned int)vecIn.size();
	GRBEnv env = GRBEnv(true);
	env.set("LogFile", "mip1.log");
	env.start();
	GRBModel model = GRBModel(env);
	model.set(GRB_IntParam_OutputFlag, 0);
	//model.set(GRB_DoubleParam_MIPGap, 0.1);
	model.set(GRB_DoubleParam_TimeLimit, 3 * 60);
	//model.set(GRB_IntParam_Threads, m_nThread);
	GRBVar* vecP = new GRBVar[nNumNode];
	GRBVar* vecSucc = new GRBVar[nNumNode + nIn];
	for (unsigned int n = 0; n < nNumNode; n++)
	{
		vecP[n] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, "P_" + to_string(n));
		vecSucc[n] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, "Succ_" + to_string(n));
		//if (n >= nNumNode)
			//model.addConstr(vecP[n] == 0);
	}
	for (unsigned int n = nNumNode; n < nNumNode + nIn; n++)
	{
		vecSucc[n] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, "Succ_" + to_string(n));
	}

	for (Node& Nodei : vecNode)
	{
		unsigned int i = Nodei.m_nIndex;
		vector<GRBVar> temp;
		for (int j : Nodei.m_vecnSucc)
		{
			model.addConstr(vecP[i] <= vecP[j]);
			temp.push_back(vecP[j]);
		}
		if (Nodei.m_bPO)
			model.addConstr(vecSucc[i] == 1);
		else
			model.addGenConstrOr(vecSucc[i], temp.data(), (int)temp.size());
	}
	for (Node& Nodei : vecIn)
	{
		unsigned int i = Nodei.m_nIndex;
		if (Nodei.m_bPO)
			model.addConstr(vecSucc[i] == 1);
		else
		{
			vector<GRBVar> temp;
			for (int j : Nodei.m_vecnSucc)
				temp.push_back(vecP[j]);
			model.addGenConstrOr(vecSucc[i], temp.data(), (int)temp.size());
		}
	}

	GRBLinExpr expr = 0;
	for (unsigned int n = 0; n < nNumNode; n++)
		expr += vecP[n];
	model.addConstr(expr, GRB_LESS_EQUAL, (1 + m_epsilon) * (double)nNumNode / 2.0);
	model.addConstr(expr, GRB_GREATER_EQUAL, (1 - m_epsilon) * (double)nNumNode / 2.0);
	GRBQuadExpr qexpr = 0;
	for (unsigned int n = 0; n < nNumNode; n++)
	{
		//if (vecNode[n].m_bPO && (vecNode[n].m_vecnSucc.size() == 0))
		if (vecNode[n].m_bPO)
			qexpr += (1 - vecP[n]) * vecSucc[n] * std::max(1.0, dPOWeight);
		else
			qexpr += (1 - vecP[n]) * vecSucc[n];
	}
	for (unsigned int n = nNumNode; n < nNumNode + nIn; n++)
		qexpr += vecSucc[n];

	model.setObjective(qexpr, GRB_MINIMIZE);

	int nRet = -1;
	int nSecondHalf = 0;
	int nFront = 0;
	clock_t start = clock();
	model.optimize();
	if (model.get(GRB_IntAttr_Status) == GRB_INFEASIBLE)
		cout << "Partition BUG\n";
	else
	{
		for (unsigned int n = 0; n < nNumNode; n++)
		{
			//cout << vecP[n].get(GRB_StringAttr_VarName) << " "
				//<< (unsigned int)round(vecP[n].get(GRB_DoubleAttr_X)) << "\n";
			if (round(vecP[n].get(GRB_DoubleAttr_X)) == 1)
			{
				vecNode[n].m_nPartition = 1;
				nSecondHalf++;
			}
			else if (round(vecSucc[n].get(GRB_DoubleAttr_X)) == 0)
				vecNode[n].m_nPartition = 0;
			else
			{
				vecNode[n].m_nPartition = 2;
				nFront++;
			}
		}
		for (unsigned int n = 0; n < nIn; n++)
		{
			if (round(vecSucc[n + nNumNode].get(GRB_DoubleAttr_X)) == 0)
				vecIn[n].m_nPartition = 0;
			else
			{
				vecIn[n].m_nPartition = 2;
				nFront++;
			}
		}
		cout << "Partitioned into: " << nNumNode - nSecondHalf << " : " << nSecondHalf << "\n";
		cout << "#Frontier=" << nFront << "\n";
		nRet = nFront;
	}
	cout << "Use: " << double(clock() - start) * 1000 / CLOCKS_PER_SEC << " ms\n";

	delete[] vecP;
	delete[] vecSucc;
	return nRet;
}

void Scheduler::IterPart(NetList& netlist, double dLeft, double dRight)
{//iterative partition
	vector<Node>& vecNode = netlist.m_vecNode;
	vector<Node>& vecIn = netlist.m_vecIn;
	vector<int>& vecnPo = netlist.m_vecnPO;
	if (vecnPo.size() > m_nMFLow)
		m_nMFLow = vecnPo.size();
	if (vecnPo.size() > m_nBound)
		m_bStop = true;
	if (m_bStop)
		return;
	int nTotSize = vecNode.size();
	if (nTotSize <= m_nGraphBound)
	{
		m_vecPartNet.push_back(netlist);
		return;
	}
	//double dPOWeight = sqrt(dLeft * dRight);
	double dPOWeight = ((double)dLeft - 1.0) / 2 + 1;
	//double dPOWeight = sqrt(dLeft);
	//double dPOWeight = std::max(1.0, 0.9 * dLeft);
	PartitionILP(netlist, dLeft);

	NetList netlist0, netlist1;
	vector<Node>& vecNode0 = netlist0.m_vecNode;
	vector<Node>& vecNode1 = netlist1.m_vecNode;
	vector<Node>& vecIn0 = netlist0.m_vecIn;
	vector<Node>& vecIn1 = netlist1.m_vecIn;
	vector<int>& vecnNewPo0 = netlist0.m_vecnPO;
	vector<int>& vecnNewPo1 = netlist1.m_vecnPO;

	vector<int> vnPart0;
	vector<int> vnPart1;
	vector<int> vnFront;
	for (unsigned int n = 0; n < vecNode.size(); n++)
	{
		int nPart = vecNode[n].m_nPartition;
		if (nPart == 0)
			vnPart0.push_back(n);
		else if (nPart == 1)
			vnPart1.push_back(n);
		else
		{
			vnPart0.push_back(n);
			vnFront.push_back(n);
		}
	}
	sort(vnPart0.begin(), vnPart0.end());
	sort(vnPart1.begin(), vnPart1.end());
	sort(vnFront.begin(), vnFront.end());
	std::map<int, int> map0OldNew;
	std::map<int, int> map1OldNew;
	std::map<int, int> mapFOldNew;
	int nPart0Size = vnPart0.size();
	int nPart1Size = vnPart1.size();
	int nFrontier = vnFront.size();
	for (unsigned int n = 0; n < nPart0Size; n++)
		map0OldNew[vnPart0[n]] = n;
	for (unsigned int n = 0; n < nPart1Size; n++)
		map1OldNew[vnPart1[n]] = n;
	for (unsigned int n = 0; n < nFrontier; n++)
		mapFOldNew[vnFront[n]] = n;

	vector<Node> tmp0(nPart0Size);
	vecNode0 = tmp0;
	for (unsigned int i = 0; i < nPart0Size; i++)
	{
		vecNode0[i].m_nIndex = i;
		vecNode0[i].m_nOrigIndex = vecNode[vnPart0[i]].m_nOrigIndex;
		for (int jold : vecNode[vnPart0[i]].m_vecnSucc)
		{
			if (map0OldNew.find(jold) != map0OldNew.end())
				vecNode0[i].m_vecnSucc.push_back(map0OldNew[jold]);
		}
	}

	vecnNewPo0.clear();
	for (int front : vnFront)
	{
		vecnNewPo0.push_back(map0OldNew[front]);
		vecNode0[map0OldNew[front]].m_bPO = true;
	}

	vector<Node> tmp0i(vecIn.size());
	vecIn0 = tmp0i;
	for (unsigned int i = 0; i < vecIn.size(); i++)
	{
		vecIn0[i].m_nIndex = nPart0Size + i;
		vecIn0[i].m_nOrigIndex = vecIn[i].m_nOrigIndex;
		for (int jold : vecIn[i].m_vecnSucc)
		{
			if (map0OldNew.find(jold) != map0OldNew.end())
				vecIn0[i].m_vecnSucc.push_back(map0OldNew[jold]);
		}
		if (vecIn[i].m_nPartition == 2)
		{
			vecnNewPo0.push_back(nPart0Size + i);
			vecIn0[i].m_bPO = true;
		}
	}
	//IterPart(netlist0, dLeft, dPOWeight);
	IterPart(netlist0, dPOWeight, dRight);

	vector<Node> tmp1(nPart1Size);
	vecNode1 = tmp1;
	for (unsigned int i = 0; i < nPart1Size; i++)
	{
		vecNode1[i].m_nIndex = i;
		vecNode1[i].m_nOrigIndex = vecNode[vnPart1[i]].m_nOrigIndex;
		for (int jold : vecNode[vnPart1[i]].m_vecnSucc)
		{
			if (map1OldNew.find(jold) != map1OldNew.end())
				vecNode1[i].m_vecnSucc.push_back(map1OldNew[jold]);
		}
	}
	vector<int> vecnInFront;
	for (unsigned int i = 0; i < vecIn.size(); i++)
	{
		if (vecIn[i].m_nPartition == 2)
			vecnInFront.push_back(vecIn[i].m_nIndex);
	}
	int nInFront = vecnInFront.size();

	vector<Node> tmp1i(nFrontier + nInFront);
	vecIn1 = tmp1i;
	for (unsigned int i = 0; i < nFrontier; i++)
	{
		vecIn1[i].m_nIndex = nPart1Size + i;
		vecIn1[i].m_nOrigIndex = vecNode[vnFront[i]].m_nOrigIndex;
		for (int jold : vecNode[vnFront[i]].m_vecnSucc)
		{
			if (map1OldNew.find(jold) != map1OldNew.end())
				vecIn1[i].m_vecnSucc.push_back(map1OldNew[jold]);
		}
	}

	vecnNewPo1.clear();
	for (int po : vecnPo)
	{
		if (map1OldNew.find(po) != map1OldNew.end())
		{
			vecnNewPo1.push_back(map1OldNew[po]);
			vecNode1[map1OldNew[po]].m_bPO = true;
		}
		else if (mapFOldNew.find(po) != mapFOldNew.end())
		{
			vecnNewPo1.push_back(mapFOldNew[po] + nPart1Size);
			vecIn1[mapFOldNew[po]].m_bPO = true;
		}
	}
	for (unsigned int i = 0; i < nInFront; i++)
	{
		vecIn1[i + nFrontier].m_nIndex = nPart1Size + nFrontier + i;
		vecIn1[i + nFrontier].m_nOrigIndex = vecIn[vecnInFront[i] - nTotSize].m_nOrigIndex;
		for (int jold : vecIn[vecnInFront[i] - nTotSize].m_vecnSucc)
		{
			if (map1OldNew.find(jold) != map1OldNew.end())
				vecIn1[i + nFrontier].m_vecnSucc.push_back(map1OldNew[jold]);
		}
		if (vecIn[vecnInFront[i] - nTotSize].m_bPO)
		{
			vecnNewPo1.push_back(nPart1Size + nFrontier + i);
			vecIn1[i + nFrontier].m_bPO = true;
		}
	}
	IterPart(netlist1, dPOWeight, dRight);
}

void Scheduler::ScheduleThread()
{//scheduling thread
	while (WaitForSingleObject(m_hEventKillThread, 1) != WAIT_OBJECT_0)
	{
		if (m_bStop)
			break;
		LONG nID = InterlockedIncrement(&m_nProcNet);
		if (nID > m_nTotNet)
			break;
		int nSch = BiSMT(m_vecPartNet[nID - 1], m_nBound, m_nMFLow);
		if (nSch == -1)
		{
			m_bStop = true;
			break;
		}
		EnterCriticalSection(&m_rCritical);
		if (m_nMFLow < nSch)
			m_nMFLow = nSch;
		m_nProgressDone += m_vecPartNet[nID - 1].m_vecNode.size();
		LeaveCriticalSection(&m_rCritical);
		cout << "=====================Progress: scheduled " << m_nProgressDone << "/" << m_nProgressTotal << " nodes. Sub MF: "
			<< nSch << "\n";
	}
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
	if (lpParam == NULL)
		return 0;

	Scheduler* pMyScheduler = (Scheduler*)lpParam;
	pMyScheduler->ScheduleThread();
	return 0;
}

void Scheduler::ThreadIterPartScheduler()
{//multi-thread interative partition scheduler
	cout << "Scheduling netlist with " << m_netlist.m_vecNode.size() << " nodes with MF bound " << m_nBound << "\n";
	m_bStop = false;
	m_nProgressTotal = m_netlist.m_vecNode.size();
	m_nProgressDone = 0;
	m_nMFLow = 0;
	m_vecPartNet.clear();
	clock_t start = clock();
	IterPart(m_netlist, m_dPO, 1);
	if (m_bStop)
	{
		cout << "MF after partition too large\n";
		return;
	}
	m_nTotNet = m_vecPartNet.size();
	m_nProcNet = 0;
	cout << "Partition Run time: " << double(clock() - start) * 1000 / CLOCKS_PER_SEC << " ms\n";
	cout << "MF lower bound: " << m_nMFLow << "\n";

	InitializeCriticalSection(&m_rCritical);
	m_hEventKillThread = CreateEvent(NULL, TRUE, FALSE, NULL); // manual reset, initially reset
	HANDLE* pThreadHandle = new HANDLE[m_nThread];
	PDWORD pdwThreadID = new DWORD[m_nThread];
	for (int i = 0; i < m_nThread; i++)
	{
		pThreadHandle[i] = CreateThread(NULL, 0, MyThreadFunction, this, 0, pdwThreadID + i);
		assert(pThreadHandle[i] != NULL);
	}
	WaitForMultipleObjects(m_nThread, pThreadHandle, TRUE, INFINITE);
	for (int i = 0; i < m_nThread; i++)
		CloseHandle(pThreadHandle[i]);

	delete[] pThreadHandle;
	delete[] pdwThreadID;
	CloseHandle(m_hEventKillThread);
	DeleteCriticalSection(&m_rCritical);

	for (NetList& net : m_vecPartNet)
	{
		for (int n : net.m_vecnSchedule)
			m_netlist.m_vecnSchedule.push_back(net.m_vecNode[n].m_nOrigIndex);
	}
	m_nFootPrint = m_nMFLow;
	if (m_nFootPrint < m_netlist.m_vecIn.size())
		m_nFootPrint = m_netlist.m_vecIn.size();
	cout << "Schedule ends, MF = " << m_nFootPrint << "\n";
}