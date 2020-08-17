/* DAGGER - A Scalable Reachability Index For Dynamic Graphs
Copyright (C) 2011 Hilmi Yildirim.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/ */
/*
 * Graph.h
 *
 *  Created on: May 30, 2011
 *      Author: yildih2
 */

#ifndef DAGWRAPPER_H_
#define DAGWRAPPER_H_
#include <vector>
#include <stack>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include "Vertex.h"
#include "GraphUpdateObservable.h"
#include "Dag.h"

#include "ReachabilityLabels.h" // for maintaining blacklist of component indexes

using namespace std;

namespace dagger {
    class Grail;

	typedef unordered_map<int, int > MyMap;
	typedef unordered_set<int> MySet;

	class DagWrapper : public DAG {
	private:
		void constructDag();
		void tarjan(int);

// some containers that are defined as class members to avoid
// passing as arguments
		MyMap high;
		MyMap low;
		MySet inStack;
		MyMap visited;
		stack<int> nodeStack;
		queue<int> nodeQueue;
		int _tIndex;
		int opCnt;
		MyMap sccmap; // can not be accessed! use getScc instead!

	public:
		ReachabilityLabels& labels_to_maintain;
		std::unordered_set<int> single_nodes;

		DagWrapper(Graph& org, ReachabilityLabels& _labels_to_maintain);
//	DagWrapper();

		Graph& orig;
		MyMap csize;
		int dagsize;

		vector<int> templist;
//	EdgeList tellist;
		unordered_set<int> roots;
		int nextNumber;

		int getNextId();
		int dagSize();
		int dagSize(int);
		int getNodeSize(int index) override;
		int getScc(int index);
		int getSccOld(int index);
		void writeMap();
		void printStatistics();

		int addDagEdge(int,int,int);
		int removeDagEdge(int,int);

		void insertEdge(int, int, Grail*);
		void updateDagAfterMerge(int&, vector<int>&);
		int mergeComponents(int src, int trg, vector<int>& list, int & center, Grail* gr);

		void deleteEdge(int, int, Grail*);
		void updateDagEdgesAfterSplit(int&, MySet&);
		int extractComponents(int , int , int , MySet& );
		void formComponentDuringSplit(int, int, MySet&);


		void removeNode(int);

		Vertex& operator[](int index);
		Vertex& getVertex(int index);
		EdgeList& getOutEdges(int index, EdgeList&);
		EdgeList& getInEdges(int index, EdgeList&);
		EdgeList& getOutEdgesFast(int index);
		EdgeList& getInEdgesFast(int index);

		vector<int> edgeAdded(int,int);
		vector<int> edgeDeleted(int, int);
		virtual ~DagWrapper();

		// alvis edition

		int getSccId(int node_id) override {
			return getScc(node_id);
		}
	};
}

#endif /* DAGWRAPPER_H_ */
