import networkx as nx
import sys
import os
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','src'))

from Generator import *


if __name__ == "__main__":
    g = Generator({"graph_type": "blogs", "weight_scale": 1, "random_weight": 1}).get()
    print("Blogs:", nx.degree_assortativity_coefficient(g))

    
    g = Generator({"graph_type": "vk", "weight_scale": 1, "random_weight": 1}).get()
    print("VK:", nx.degree_assortativity_coefficient(g), " nodes ", len(g))

    
    g = Generator({"graph_type": "dblp", "weight_scale": 1, "random_weight": 1}).get()
    print("dblp:", nx.degree_assortativity_coefficient(g), " nodes ", len(g))
    
    g = Generator({"graph_type": "gnutella", "weight_scale": 1, "random_weight": 1}).get()
    print("Gnutella:", nx.degree_assortativity_coefficient(g), " nodes ", len(g))
    
    g = Generator({"graph_type": "stanford", "weight_scale": 1, "random_weight": 1}).get()
    print("Stanford:", nx.degree_assortativity_coefficient(g), " nodes ", len(g))