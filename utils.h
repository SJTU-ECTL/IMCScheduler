#pragma once
#include <iostream>
#include <time.h>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include<string>
#include<fstream>
#include <cmath>
#include<stack>
#include <algorithm>
#include <random>
#include "mockturtle\mockturtle.hpp"
#include "lorina\aiger.hpp"
#include "C:\\gurobi1001\\win64\\include\\gurobi_c++.h"
#include "z3include\\z3++.h"
#pragma comment(lib, "C:\\z3\\libZ3.lib")
#pragma comment(lib, "gurobi100.lib")
#pragma comment(lib, "gurobi_c++md2017.lib")

using namespace mockturtle;
using namespace std;

inline xmg_network Aig2Xmg(string strAigName)
{
	mockturtle::aig_network aig;
	lorina::read_aiger(strAigName, mockturtle::aiger_reader(aig));
	xmg_npn_resynthesis resyn;
	exact_library<xmg_network, xmg_npn_resynthesis> lib(resyn);
	map_params ps;
	ps.skip_delay_round = true;
	ps.required_time = numeric_limits<double>::max();
	xmg_network xmg = mockturtle::map(aig, lib, ps);

	functional_reduction(xmg);
	xmg = cleanup_dangling(xmg);

	if (strAigName == "bench\\sqrt.aig")
		return xmg;

	depth_view xmg_depth{ xmg };
	fanout_view xmg_fanout{ xmg_depth };
	xmg_resubstitution(xmg_fanout);
	xmg_network xmg_res = xmg_fanout;
	xmg_res = cleanup_dangling(xmg);
	if (xmg_res.size() < xmg.size())
		xmg = xmg_res;

	return xmg;
}

inline xmg_network Bliff2Xmg(string strBliffName)
{
	cover_network cover;
	lorina::read_blif(strBliffName, blif_reader(cover));
	aig_network aig;
	convert_cover_to_graph(aig, cover);

	xmg_npn_resynthesis resyn;
	exact_library<xmg_network, xmg_npn_resynthesis> lib(resyn);

	map_params ps;
	ps.skip_delay_round = true;
	ps.required_time = numeric_limits<double>::max();
	xmg_network xmg = mockturtle::map(aig, lib, ps);
	functional_reduction(xmg);
	xmg = cleanup_dangling(xmg);

	depth_view xmg_depth{ xmg };
	fanout_view xmg_fanout{ xmg_depth };
	xmg_resubstitution(xmg_fanout);
	xmg_network xmg_res = xmg_fanout;
	xmg_res = cleanup_dangling(xmg);
	if (xmg_res.size() < xmg.size())
		xmg = xmg_res;

	return xmg;
}

inline void Trim(string& s)
{//remove ' ' and '\\'
	s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
	s.erase(std::remove(s.begin(), s.end(), '\\'), s.end());
}

inline string ExtractStr(string& s, string strLeft, string strRight)
{//extract substring from strLeft to strRight in s
	size_t left = s.find(strLeft);
	if (left == string::npos)
		return "";
	size_t right = s.find(strRight, left + strLeft.length());
	if (right == string::npos)
		return "";
	//cout << left << " " << right << "\n";
	string res = s.substr(left + strLeft.length(), right - left - strLeft.length());
	s.erase(left, right - left);
	return res;
}

inline int GetIndexInVector(int n, vector<int>& vecn)
{//get index of n in vecn
	for (int i = 0; i < vecn.size(); i++)
	{
		if (vecn[i] == n)
			return i;
	}
	return -1;
}

inline int GetFromArray(vector<int>& vecMem, int nArray, int nArraySize = 256)
{//get empty row in nArray
	int nRow = nArray * nArraySize;
	int nBack = (nArray + 1) * nArraySize;
	for (; nRow < nBack; nRow++)
	{
		if (vecMem[nRow] == 0)
			break;
	}
	if (nRow == nBack)
		nRow = -1;
	return nRow;
}

inline bool IsInVector(int n, vector<int>& vecn)
{//whether n is in vecn
	for (int i = 0; i < vecn.size(); i++)
	{
		if (vecn[i] == n)
			return true;
	}
	return false;
}