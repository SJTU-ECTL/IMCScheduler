#include "Scheduler.h"
int main()
{
	ifstream fin;
	fin.open("benchmarks.txt", ios::in);
	std::ofstream ofCSV;
	string strResultName = "result.csv";
	ofCSV.open(strResultName.c_str(), std::ofstream::out, _SH_DENYWR);
	string strBench, strLine;
	int nBound;
	istringstream sstream;
	while (getline(fin, strLine))
	{
		nBound = -1;
		sstream.clear();
		sstream.str(strLine);
		sstream >> strBench >> nBound;
		Scheduler MyScheduler;
		MyScheduler.m_netlist.m_strBench = strBench;
		MyScheduler.m_netlist.ReadFromFile(strBench);  //XMG netlist (.v / .bliff / .aig)
		//MyScheduler.m_netlist.ConfigForNOR(strBench);  //NOR netlist (.v)
		if (nBound == -1)
			MyScheduler.m_nBound = MyScheduler.m_netlist.m_vecNode.size();
		else
			MyScheduler.m_nBound = nBound;
		MyScheduler.ThreadIterPartScheduler();
		MyScheduler.m_netlist.ConfigMF();
		ofCSV << MyScheduler.m_netlist.m_strBench << "," << MyScheduler.m_netlist.m_nSize 
			<< "," << MyScheduler.m_netlist.m_nMF << "\n";
		ofCSV.flush();
	}
	getchar();
	fin.close();
	ofCSV.close();
	return 0;
}