#include "dim.hpp"

using namespace std;

void DIM::init() {
	naive_operation = false;
	TOT = 0;
	beta = 1;

	_adjust();
}

void DIM::set_beta(int _beta) {
	beta = _beta;
}

void DIM::insert(int v) {
	if (V.count(v)) {
		printf(DIMWARN "vertex %d was found." DIMDEF, v);
		return;
	} else {
		ideg[v] = 0;

		V.insert(v);
		Vvec.push_back(v);

		if (!naive_operation) {
			double p = 1.0 / V.size();
			for (ULL at = 0;; at++) {
				double k;
				if (V.size() == 1) {
					k = 0;
				} else {
					double x = _gen_double((unsigned int) (at >> 32),
							(unsigned int) (at), (unsigned int) v, 0);
					k = log(1 - x) / log(1 - p);
				}
				at += (LL) k;
				if (k >= hs.size() || at >= hs.size()) {
					break;
				}
				_clear(at);
				hs[at].z = v;
				_expand(at, v);
			}
			_adjust();
		} else {
			double p = 1.0 / V.size();
			for (ULL at = 0; at < hs.size(); at++) {
				double x = _gen_double((unsigned int) (at >> 32),
						(unsigned int) (at), (unsigned int) v, 0);
				if (x < p) {
					_clear(at);
					hs[at].z = v;
					_expand(at, v);
				}
			}
			_adjust();
		}
	}
}

void DIM::erase(int v) {
	if (!V.count(v)) {
		printf(DIMWARN "vertex %d was not found." DIMDEF, v);
		return;
	} else {
		vector<int> in, out;

		// OUT-edges `from' v
		for (auto it = ps.lower_bound(make_pair(v, 0));
				it != ps.end() && it->first.first == v; it++) {
			int w = it->first.second;
			out.push_back(w);
		}
		// IN-edges `to' v
		for (auto it = rs.lower_bound(make_pair(v, 0));
				it != rs.end() && it->first.first == v; it++) {
			int u = it->first.second;
			in.push_back(u);
		}

		// Remove OUT-edges from each sketch
		for (int w : out) {
			for (int at : index[w]) {
				pair<int, int> e = make_pair(w, v);
				auto jt = lower_bound(hs[at].x.begin(), hs[at].x.end(), e);
				if (jt != hs[at].x.end() && *jt == e) {
					hs[at].x.erase(jt);
				}
				_weight(at, -1); // Removal of (v, w)
			}
		}

		// So that _next_target MUST not return v
		V.erase(v);
		for (int w : out) {
			// Remove (v, w)
			ps.erase(make_pair(v, w));
			rs.erase(make_pair(w, v));
			ideg[w]--;
		}

		// Scan sketches that have v
		vector<ULL> ats(index[v].begin(), index[v].end());
		for (ULL at : ats) {
			if (hs[at].z == v) { // If target = v
				// Reconstruction
				_clear(at);
				hs[at].z = _next_target(at);
				_expand(at, hs[at].z);
			} else { // Otherwise
				// Remove vertices that can no longer reach to z
				if (!naive_operation) {
					_erase_node(at, v);
				} else {
					_shrink(at);
				}
			}
		}

		// Remove IN-edges to v
		for (int u : in) {
			ps.erase(make_pair(u, v));
			rs.erase(make_pair(v, u));
			ideg[v]--;
		}
		ideg.erase(v);

		_adjust();
	}
}

void DIM::insert(int u, int v, double p) {
	if (!V.count(u) || !V.count(v)) {
		printf(DIMWARN "edge (%d, %d) is invalid." DIMDEF, u, v);
		return;
	} else if (u == v) {
		printf(DIMWARN "(%d, %d) is self loop." DIMDEF, u, v);
		return;
	} else if (ps.count(make_pair(u, v))) {
		printf(DIMWARN "edge (%d, %d) exists." DIMDEF, u, v);
		return;
	} else {
		ps[make_pair(u, v)] = 0;
		rs[make_pair(v, u)] = 0;
		ideg[v]++;
		for (ULL at : index[v]) {
			_weight(at, +1);
		}
		_change(u, v, p);
		_adjust();
	}
}

void DIM::erase(int u, int v) {
	if (!ps.count(make_pair(u, v))) {
		printf(DIMWARN "edge (%d, %d) does not exist." DIMDEF, u, v);
		return;
	} else {
		_change(u, v, 0);
		for (auto it = index[v].begin(); it != index[v].end(); it++) {
			_weight(*it, -1);
		}
		ps.erase(make_pair(u, v));
		rs.erase(make_pair(v, u));
		ideg[v]--;
		_adjust();
	}
}

void DIM::change(int u, int v, double p) {
	if (!ps.count(make_pair(u, v))) {
		printf(DIMWARN "edge (%d, %d) does not exist." DIMDEF, u, v);
		return;
	} else {
		_change(u, v, p);
		_adjust();
	}
}

int DIM::_next_target(ULL at) {
	unsigned int at1 = (unsigned int) (at >> 32);
	unsigned int at2 = (unsigned int) (at);
	int n = Vvec.size();
	for (unsigned int cnt = 0;; cnt++) {
		int num = n * _gen_double(at1, at2, cnt, 0);
		int z = Vvec[num];
		if (V.count(z)) {
			return z;
		}
	}
}

double DIM::_gen_double(unsigned int a1, unsigned int a2, unsigned int a3,
		unsigned int a4) {
	r123::Threefry4x32::ctr_type ctr = { {a1, a2, a3, a4}};
	r123::Threefry4x32::ctr_type rand = g(ctr, key);
	unsigned int x = rand[0], y = rand[1];
	return ((x >> 5) * 67108864.0 + (y >> 6)) * (1.0 / (1LL << 53));
}

double DIM::_gen_int(unsigned int a1, unsigned int a2, unsigned int a3,
		unsigned int a4, unsigned int n) {
	return (int) (n * _gen_double(a1, a2, a3, a4));
}

double DIM::_gen_double(ULL at, int u, int v) {
	unsigned int at1 = (unsigned int) (at >> 32);
	unsigned int at2 = (unsigned int) (at);

	return _gen_double(at1, at2, u, v);
}

void DIM::_change(int u, int v, double p) {
	ps[make_pair(u, v)] = p;
	rs[make_pair(v, u)] = p;

	vector<ULL> ats(index[v].begin(), index[v].end());
	for (ULL at : ats) {
		auto jt = lower_bound(hs[at].x.begin(), hs[at].x.end(),
				make_pair(v, u));
		bool uv1 = jt != hs[at].x.end() && jt->first == v && jt->second == u;
		bool uv2 = _gen_double(at, u, v) < p;
		// Case 1: dead -> live
		if (!uv1 && uv2) {
			hs[at].x.insert(jt, make_pair(v, u));
			if (hs[at].H.count(u)) {
				// u can already reach to z
				// Do nothing
			} else {
				// Start rev. BFS from u
				auto kt = lower_bound(hs[at].par.begin(), hs[at].par.end(),
						make_pair(v, u));
				hs[at].par.insert(kt, make_pair(v, u));
				_expand(at, u);
			}
		}

		// Case 2:live -> dead
		if (uv1 && !uv2) {
			hs[at].x.erase(jt);
			if (!naive_operation) {
				_erase_edge(at, u, v);
			} else {
				_shrink(at);
			}
		}
	}
}

void DIM::_expand(ULL at, int z) {
	if (hs[at].H.count(z)) {
		return;
	} else {
		// Rev. BFS from z
		queue<int> Q;
		Q.push(z);
		hs[at].H.insert(z);
		_weight(at, +1 + ideg[z]);
		for (; !Q.empty();) {
			int v = Q.front();
			Q.pop();
			index[v].insert(at);
			for (auto it = rs.lower_bound(make_pair(v, 0));
					it != rs.end() && it->first.first == v; it++) {
				int u = it->first.second;
				double p = it->second;
				double x = _gen_double(at, u, v);
				if (x < p) { // (u, v) is live.
					hs[at].x.push_back(make_pair(v, u));
					if (!hs[at].H.count(u)) {
						Q.push(u);
						hs[at].H.insert(u);
						_weight(at, +1 + ideg[u]);
						hs[at].par.push_back(make_pair(v, u));
					}
				}
			}
		}
		// Sort live edges and parents.
		sort(hs[at].x.begin(), hs[at].x.end());
		sort(hs[at].par.begin(), hs[at].par.end());
	}
}

bool DIM::_erase_node(ULL at, int vv) {
	// <vv, *> are already removed.
	queue<int> Q;
	map<int, int> tree;
	Q.push(vv);
	for (; !Q.empty();) {
		int v = Q.front();
		Q.pop();
		for (auto it = lower_bound(hs[at].par.begin(), hs[at].par.end(),
				make_pair(v, 0)); it != hs[at].par.end() && it->first == v;
				it++) {
			Q.push(it->second);
			tree[it->second] = v;
		}
	}

	// Search vv's parent
	for (auto it = hs[at].par.begin(); it != hs[at].par.end(); it++) {
		if (it->second == vv) {
			tree[vv] = it->first;
			// it = hs[at].par.erase(it);
			// (vv, parent[vv]) will be removed LATER.
			break;
		}
	}

	set<int> reach;
	for (auto it : tree) {
		int v = it.first;
		for (auto jt = ps.lower_bound(make_pair(v, 0));
				jt != ps.end() && jt->first.first == v; jt++) {
			int w = jt->first.second;
			const pair<int, int> re = make_pair(w, v);
			if (binary_search(hs[at].x.begin(), hs[at].x.end(), re)) {
				if (!tree.count(w) && hs[at].H.count(w)) {
					reach.insert(v);
					// Remove OLD parent of v
					auto kt1 = lower_bound(hs[at].par.begin(), hs[at].par.end(),
							make_pair(tree[v], v));
					hs[at].par.erase(kt1);
					// Insert NEW parent of v
					auto kt2 = lower_bound(hs[at].par.begin(), hs[at].par.end(),
							re);
					hs[at].par.insert(kt2, re);
					break;
				}
			}
		}
	}

	for (int v : reach) {
		Q.push(v);
	}

	for (; !Q.empty();) {
		int v = Q.front();
		Q.pop();
		for (auto it = lower_bound(hs[at].x.begin(), hs[at].x.end(),
				make_pair(v, 0)); it != hs[at].x.end() && it->first == v;
				it++) {
			const int u = it->second;
			if (!reach.count(u) && tree.count(u)) {
				reach.insert(u);
				Q.push(u);
				// Remove the OLD parent of u
				auto kt1 = lower_bound(hs[at].par.begin(), hs[at].par.end(),
						make_pair(tree[u], u));
				hs[at].par.erase(kt1);
				// Insert the NEW parent of u
				auto kt2 = lower_bound(hs[at].par.begin(), hs[at].par.end(),
						make_pair(v, u));
				hs[at].par.insert(kt2, make_pair(v, u));
			}
		}
	}

	for (auto it : tree) {
		int v = it.first, w = it.second;
		if (!reach.count(v)) {
			// Remove v's in-live-edges
			for (auto where = lower_bound(hs[at].x.begin(), hs[at].x.end(),
					make_pair(v, 0));
					where != hs[at].x.end() && where->first == v;
					where = hs[at].x.erase(where))
				;
			_weight(at, -ideg[v]);				// in-deg of v
			_weight(at, -1);				// v
			hs[at].H.erase(v);
			index[v].erase(at);

			auto jt = lower_bound(hs[at].par.begin(), hs[at].par.end(),
					make_pair(w, v));
			hs[at].par.erase(jt);
		}
	}
	return true;
}

bool DIM::_erase_edge(ULL at, int uu, int vv) {
	if (binary_search(hs[at].par.begin(), hs[at].par.end(),
			make_pair(vv, uu))) {
	} else {
		// (uu, vv) is not in T
		// Removal of (uu, vv) does not affect T
		return true;
	}

	queue<int> Q;
	map<int, int> tree;
	Q.push(uu);
	tree[uu] = vv;
	// <u -> v>
	for (; !Q.empty();) {
		const int v = Q.front();
		Q.pop();
		for (auto it = lower_bound(hs[at].par.begin(), hs[at].par.end(),
				make_pair(v, 0)); it != hs[at].par.end() && it->first == v;
				it++) {
			Q.push(it->second);
			tree[it->second] = v;
		}
	}

	set<int> reach; // Vertices in T that can still reach to z
	for (auto it : tree) {
		int v = it.first;
		// Check whether v enters H-T
		for (auto jt = ps.lower_bound(make_pair(v, 0));
				jt != ps.end() && jt->first.first == v; jt++) {
			int w = jt->first.second;
			const pair<int, int> re = make_pair(w, v);
			if (binary_search(hs[at].x.begin(), hs[at].x.end(), re)) {
				if (!tree.count(w) && hs[at].H.count(w)) {
					reach.insert(v);
					// Remove OLD parent of v
					auto kt1 = lower_bound(hs[at].par.begin(), hs[at].par.end(),
							make_pair(tree[v], v));
					hs[at].par.erase(kt1);

					// Insert NEW parent of v
					auto kt2 = lower_bound(hs[at].par.begin(), hs[at].par.end(),
							re);
					hs[at].par.insert(kt2, re);
					break;
				}
			}
		}
	}

	for (auto v : reach) {
		Q.push(v);
	}

	// Rev. BFS from v that enters  H-T
	for (; !Q.empty();) {
		int v = Q.front();
		Q.pop();
		for (auto it = lower_bound(hs[at].x.begin(), hs[at].x.end(),
				make_pair(v, 0)); it != hs[at].x.end() && it->first == v;
				it++) {
			const int u = it->second;
			if (!reach.count(u) && tree.count(u)) {
				reach.insert(u);
				Q.push(u);
				// Remove OLD parent of u
				auto kt1 = lower_bound(hs[at].par.begin(), hs[at].par.end(),
						make_pair(tree[u], u));
				hs[at].par.erase(kt1);
				// Insert NEW parent of u
				auto kt2 = lower_bound(hs[at].par.begin(), hs[at].par.end(),
						make_pair(v, u));
				hs[at].par.insert(kt2, make_pair(v, u));
			}
		}
	}

	// Remove vertices that can no longer reach to z
	for (auto it : tree) {
		int v = it.first, w = it.second;
		if (!reach.count(v)) {
			for (auto where = lower_bound(hs[at].x.begin(), hs[at].x.end(),
					make_pair(v, 0));
					where != hs[at].x.end() && where->first == v;
					where = hs[at].x.erase(where))
				;
			_weight(at, -ideg[v]); // in-deg of v
			_weight(at, -1); // v
			hs[at].H.erase(v);
			index[v].erase(at);

			auto jt = lower_bound(hs[at].par.begin(), hs[at].par.end(),
					make_pair(w, v));
			hs[at].par.erase(jt);
		}
	}
	return true;
}

void DIM::_shrink(ULL at) {
	hs[at].par.clear();

	// Reverse BFS from z using only live edges
	queue<int> Q;
	Q.push(hs[at].z);
	set<int> X;
	X.insert(hs[at].z);
	for (; !Q.empty();) {
		const int v = Q.front();
		Q.pop();
		for (auto it = lower_bound(hs[at].x.begin(), hs[at].x.end(),
				make_pair(v, 0)); it != hs[at].x.end() && it->first == v;
				it++) {
			const int u = it->second;
			if (!X.count(u)) {
				Q.push(u);
				X.insert(u);
				hs[at].par.push_back(make_pair(v, u));
			}
		}
	}
	sort(hs[at].par.begin(), hs[at].par.end());

	// Vertices in Y will be removed.
	set<int> Y;
	for (int v : hs[at].H) {
		if (!X.count(v)) {
			Y.insert(v);
		}
	}

	// Remove vertices that can no longer reach to z
	for (int v : Y) {
		// Remove edges entering v
		for (auto where = lower_bound(hs[at].x.begin(), hs[at].x.end(),
				make_pair(v, 0)); where != hs[at].x.end() && where->first == v;
				where = hs[at].x.erase(where))
			;
		_weight(at, -ideg[v]);
		_weight(at, -1);
		hs[at].H.erase(v);
		index[v].erase(at);
	}
}

void DIM::_clear(ULL at) {
	for (int v : hs[at].H) {
		index[v].erase(at);
	}
	_weight(at, -ws[at]);
	hs[at].H.clear();
	hs[at].x.clear();
	hs[at].par.clear();
}

inline void DIM::_weight(ULL at, int dw) {
	ws[at] += dw;
	TOT += dw;
}

double DIM::_get_R() {
	int n = V.size(), m = ps.size();
	return beta * (n + m) * log2(n);
}

void DIM::_adjust() {
	double R = _get_R();

	int rem = 0, add = 0;
	for (; TOT < R;) {
		int z = _next_target(hs.size());

		hedge h;
		h.z = z;
		hs.push_back(h);
		ws.push_back(0);
		_expand(hs.size() - 1, h.z);

		add++;
	}

	for (ULL at = hs.size() - 1; hs.size() >= 1; at = hs.size() - 1) {
		if (TOT - ws[at] >= R) {
		} else {
			break;
		}
		_clear(at);
		hs.pop_back();
		ws.pop_back();
		rem++;
	}

}

void DIM::infmax_with_state_init() {
	int n = *(V.rbegin()) + 1;
	degs_state = vector<ULL>(n);
	I_state = vector<bool>(hs.size());
	enheaped_iter_state = std::unordered_map<int, int>();
	iter_state = 0;
	total_positive_hs = 0;

	for (int v : V) {
		degs_state[v] = index[v].size();
		enheaped_iter_state[v] = 0;
	}
};

void DIM::infmax_with_state_add(int v) {
	for (ULL at : index[v]) {
		if (!I_state[at]) {
			I_state[at] = true;
			total_positive_hs++;
			for (int v : hs[at].H) {
				degs_state[v]--;
			}
		}
	}
	iter_state++;
};

double DIM::infmax_with_state_get(int v) {
	int new_positives = 0;
	if (v > -1) {
		for (LL at : index[v]) {
			if (!I_state[at]) new_positives++;
		}
	}

	if (hs.size() == 0) {
		return 1; // workaround for single-node graphs
	}

	return (double) (total_positive_hs + new_positives) / hs.size() * V.size();
};

vector<int> DIM::infmax(int k) {
	int n = *(V.rbegin()) + 1;
	vector<ULL> degs(n);
	vector<bool> I(hs.size());

	priority_queue<tuple<LL, int, int> > Q;
	// <deg, v, tick>

	for (int v : V) {
		degs[v] = index[v].size();
		Q.push(make_tuple(degs[v], v, 0));
	}

	vector<int> S;
	for (int iter = 0; iter < k;) {
		assert(Q.size() > 0);
		auto e = Q.top();
		Q.pop();
		int v = get < 1 > (e), tick = get < 2 > (e);
		if (tick == iter) {
			S.push_back(v);
			for (ULL at : index[v]) {
				if (!I[at]) {
					I[at] = true;
					for (int v : hs[at].H) {
						degs[v]--;
					}
				}
			}
			iter++;
		} else {
			Q.push(make_tuple(degs[v], v, iter));
		}
	}
	return S;
}

vector<double> DIM::marginal_gain() {
	int n = *(V.rbegin()) + 1;
	vector<ULL> degs(n);
	vector<bool> I(hs.size());

	priority_queue<tuple<LL, int, int> > Q;
	// <deg, v, tick>

	for (int v : V) {
		degs[v] = index[v].size();
		Q.push(make_tuple(degs[v], v, 0));
	}

	vector<double> S;
	for (int iter = 0; iter < V.size();) {
		auto e = Q.top();
		Q.pop();
		int gain = get < 0 > (e), v = get < 1 > (e), tick = get < 2 > (e);
		if (tick == iter) {
			S.push_back((double)gain / hs.size() * V.size());
			for (ULL at : index[v]) {
				if (!I[at]) {
					I[at] = true;
					for (int v : hs[at].H) {
						degs[v]--;
					}
				}
			}
			iter++;
		} else {
			Q.push(make_tuple(degs[v], v, iter));
		}
	}
	return S;
}

double DIM::infest_naive(vector<int> &S) {
	LL hit = 0;
	for (ULL i = 0; i < hs.size(); i++) {
		for (int s : S) {
			if (hs[i].H.count(s)) {
				hit++;
				break;
			}
		}
	}
	return (double) hit / hs.size() * V.size();
}

double DIM::infest(vector<int> &S) {
	vector<LL> I;
	for (int v : S) {
		for (LL at : index[v]) {
			I.push_back(at);
		}
	}
	sort(I.begin(), I.end());
	I.erase(unique(I.begin(), I.end()), I.end());
	if (hs.size() == 0) {
		return 1; // workaround for single-node graphs
 	}
	return (double) I.size() / hs.size() * V.size();
}

double DIM::infest(int v) {
	return (double) index[v].size() / hs.size() * V.size();
}
