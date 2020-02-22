#ifndef __DIM_H__
#define __DIM_H__

#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <exception>
#include "nheap.h"
#include "Random123/threefry.h"

#define DIMERROR "\x1b[41mERROR: "
#define DIMWARN "\x1b[45mWARN: "
#define DIMINFO "\x1b[32m\x1b[1mINFO: "
#define DIMDEF "\x1b[0m\n"

using namespace std;
typedef long long LL;
typedef unsigned long long ULL;

class hedge {
public:
	set<int> H;
	vector<pair<int, int> > par;
	vector<pair<int, int> > x; // <to, from>
	int z;
};

// Dynamic Influence Maximization
class DIM {
public:

	set<int> V;
	vector<int> Vvec;

	map<pair<int, int>, double> ps;
	// u->v, p
	map<pair<int, int>, double> rs;
	// v<-u, p

	int beta;
	ULL TOT;

	bool _erase_node(ULL at, int v);
	bool _erase_edge(ULL at, int u, int v);
	void _shrink(ULL at);

	void _expand(ULL at, int z);

	void _change(int u, int v, double p);

	r123::Threefry4x32 g;
	const r123::Threefry4x32::key_type key = { { 0xdeadbeef } };

	int _next_target(ULL at);
	double _gen_double(unsigned int a1, unsigned int a2, unsigned int a3, unsigned int a4);
	double _gen_int(unsigned int a1, unsigned int a2, unsigned int a3, unsigned int a4, unsigned int n);

	double _gen_double(ULL at, int u, int v);

	void _clear(ULL at);
	void _weight(ULL at, int dw);
	double _get_R();

	vector<hedge> hs;
	vector<int> ws;
	void init();

	map<int, int> ideg;

	map<int, set<ULL> > index;
	// v -> indices of h

	bool naive_operation; // Naive vert add, vert del, & edge del

	void set_beta(int beta);
	// DO NOT call if |V|=0
	void _adjust();

	// Dynamic updates
	void insert(int v); // Insert Node v
	void erase(int v); // Delete Node v (and its incident edges)
	void insert(int u, int v, double p); // Insert Edge (u,v) with prob p
	void erase(int u, int v); // Delete Edge (u,v)
	void change(int u, int v, double p); //  // Change prob of Edge (u,v) to 0.9

	// Queries
	vector<int> infmax(int k); // Extract an influential set of k vertices
	double infest_naive(vector<int> &S); // Estimate the influence of S (slow impl.)
	double infest(vector<int> &S); // Estimate the influence of S (fast impl.)
	double infest(int v); // Estimate the influence of v

	// Dynamic
	vector<ULL> degs_state;
	vector<bool> I_state;
	int iter_state;
	int total_positive_hs;
	std::unordered_map<int, int> enheaped_iter_state;
//	priority_queue<tuple<LL, int, int> > Q_state;
	void infmax_with_state_init();
	void infmax_with_state_add(int s);
	double infmax_with_state_get(int v = -1);
	void reset() {
		TOT = 0;
		V.clear();
		Vvec.clear();
		hs.clear();
		ps.clear();
		rs.clear();
		ws.clear();
		index.clear();
		ideg.clear();
	}

	vector<double> marginal_gain(); // get a sequence of marginal gains from new seeds
};

#endif
