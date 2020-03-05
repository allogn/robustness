#ifndef __GRAPH_H__
#define __GRAPH_H__

#define HEAD_INFO
//#define HEAD_TRACE

#include <utility>
#include <unordered_set>
#include <exception>
#include <ostream>

#include "sfmt/SFMT.h"
#include "head.h"
#include "DGRAIL/Graph.h"
#include "json.h"

typedef double (*pf)(int, int);
class Graph
{
public:
    int n, m, k;
    nlohmann::json graph_params;
    std::vector<nid_t> inDeg;
    std::vector<nid_t> outDeg;
    std::vector<std::vector<nid_t> > gT; // a list of incoming edges
    std::vector<std::vector<nid_t> > g; // a list of outgoing edges
    std::vector<std::unordered_set<nid_t> > g_set;
    std::vector<nid_t> blocked_nodes;
    std::vector<double> edgelist;
    std::vector<double> weights;
    std::vector<nid_t> id_to_ind;
    std::vector<nid_t> ind_to_id;
    std::string graph_id;
    std::vector<std::vector<double> > probT;
    std::vector<std::vector<double> > prob;

    enum InfluModel {IC, LT, CONT};
    InfluModel influModel;
    void setInfuModel(InfluModel p)
    {
        influModel = p;
        TRACE(influModel == IC);
        TRACE(influModel == LT);
        TRACE(influModel == CONT);
    }

    string folder;
    string graph_file;
    void readNM()
    {
        ifstream cin((folder + "/attributes.txt").c_str());
        if(!cin) {
            std::cout << "Input file " << folder << " does not exist." << std::endl;
            exit(1);
        };
        string s;
        while (cin >> s)
        {
            size_t ind = s.find("=");
            std::string str2 = s.substr(0,ind);
            std::string str3 = s.substr(ind+1);
            graph_params[str2] = str3;

            if (s.substr(0, 2) == "n=")
            {
                n = atoi(s.substr(2).c_str());
                continue;
            }
            if (s.substr(0, 2) == "m=")
            {
                m = atoi(s.substr(2).c_str());
                continue;
            }
            if (s.substr(0, 9) == "graph_id=")
            {
                graph_id = s.substr(9);
                continue;
            }
        }

        TRACE(n, m );
        cin.close();
    }

    void add_edge(nid_t a, nid_t b, double p)
    {
        probT[b].push_back(p);
        prob[a].push_back(p);
        gT[b].push_back(a);
        if (g_set[a].count(b)) {
            std::ostringstream out;
            out << "Graph has duplicate edges: " << a << "->" << b;
            throw std::runtime_error(out.str());
        }
        g[a].push_back(b);
        g_set[a].insert(b);
        inDeg[b]++;
        outDeg[a]++;
    }

    std::vector<bool> hasnode;
    void readGraph()
    {
        id_to_ind.clear();
        ind_to_id.clear();
        std::string graph_name = (folder + "/" + graph_file);
        FILE *fin = fopen(graph_name.c_str(), "r");
        ASSERT(fin != nullptr);
        int readCnt = 0;

        std::set<nid_t> blocked_nodes_set(blocked_nodes.begin(), blocked_nodes.end());
        ASSERT(blocked_nodes_set.size() == blocked_nodes.size()); // there are duplicates in blocked nodes
        nid_t j = 0;
        for (nid_t i = 0; i < n; i++) {
          if (blocked_nodes_set.count(i) == 0) {
            id_to_ind.push_back(j);
            ind_to_id.push_back(i);
            j++;
          } else {
            id_to_ind.push_back(-1);
          }
        }
        for (nid_t i = j; i < n; i++) ind_to_id.push_back(i);
        ASSERT(ind_to_id.size() == n);

        std::set<std::pair<nid_t,nid_t>> existing_edges;
        for (nid_t i = 0; i < edgelist.size()/2; i++) {
          auto edge = std::make_pair(edgelist[i*2], edgelist[i*2+1]);
          existing_edges.insert(edge);
        }

        std::map<std::pair<nid_t,nid_t>, double> weight_map;
        ASSERT(weights.size() == existing_edges.size() || weights.size() == 0);
        for (nid_t i = 0; i < weights.size(); i++) {
          auto edge = std::make_pair(edgelist[i*2], edgelist[i*2+1]);
          weight_map[edge] = weights[i];
        }

        nid_t removed_edges = 0;
        for (nid_t i = 0; i < m; i++)
        {
            readCnt ++;
            nid_t a, b;
            double p;
            int c = fscanf(fin, "%d%d%lf", &a, &b, &p); // !!! assuming in graph.csv file all nodes are sequentially enumerated
            ASSERTT(c == 3, a, b, p, c);

            if (existing_edges.size() > 0) {
                auto edge = std::make_pair(a,b);
                if (existing_edges.count(edge) == 0) {
                  removed_edges += 1;
                  continue;
                } else {
                  ASSERT(blocked_nodes_set.count(a) == 0 && blocked_nodes_set.count(b) == 0); // assert adjacent nodes of an edge are not blocked
                  if (weights.size() > 0) {
                    p = weight_map[edge];
                  }
                }
            }

            //TRACE_LINE(a, b);
            ASSERT( a < n );
            ASSERT( b < n );
            hasnode[a] = true;
            hasnode[b] = true;

            if (blocked_nodes_set.count(a) > 0 || blocked_nodes_set.count(b) > 0) {
              removed_edges += 1;
              continue;
            }

            add_edge(id_to_ind[a], id_to_ind[b], p);
        }
        TRACE_LINE_END();



        nid_t s = 0;
        for (nid_t i = 0; i < n; i++)
            if (hasnode[i])
                s++;
        // INFO(s);
        ASSERT(readCnt == m);
        fclose(fin);

        m -= removed_edges;
        n -= blocked_nodes.size(); //must be here
    }

    Graph() {}

    Graph(string folder, string graph_file,
              std::vector<nid_t> blocked_nodes = std::vector<nid_t>(),
							std::vector<double> edgelist = std::vector<double>(),
							std::vector<double> weights = std::vector<double>()):
							folder(folder),
							graph_file(graph_file),
							blocked_nodes(blocked_nodes),
							edgelist(edgelist),
							weights(weights)
    {
        readNM();

        FOR(i, n-blocked_nodes.size())
        {
            gT.push_back(std::vector<nid_t>());
            g.push_back(std::vector<nid_t>());
            g_set.emplace_back();
            hasnode.push_back(false);
            probT.push_back(std::vector<double>());
            prob.push_back(std::vector<double>());
            //hyperToNodes.push_back(std::vector<int>());
            inDeg.push_back(0);
            outDeg.push_back(0);
        }
        readGraph();
    }

    std::vector<double> get_edgelist() {
      ASSERT(gT.size() == n);
      std::vector<double> edgelist;
      for (nid_t i = 0; i < gT.size(); i++) {
        for (nid_t j = 0; j < gT[i].size(); j++) {
          edgelist.push_back(ind_to_id[gT[i][j]]);
          edgelist.push_back(ind_to_id[i]);
          edgelist.push_back(probT[i][j]);
        }
      }
      return edgelist;
    }

    std::string get_graph_id() {
      return graph_id;
    }

    dagger::Graph get_dagger_graph() {
        auto dg = dagger::Graph();

        dg.vsize = n;
        dg.edgeCount = 0;

        for (int i = 0; i < n; i++) {
            dg.vertices.insert(dagger::VertexList::value_type(i,dagger::Vertex(i)));
        }
        for (int i = 0; i < n; i++) {
            for (auto j : g[i]) {
                dg.insertEdge(i,j);
            }
        }

        return dg;
    }

};
double sqr(double t)
{
    return t * t;
}

#endif
