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
 * Vertex.cpp
 *
 *  Created on: Mar 11, 2011
 *      Author: yildih2
 */

#include "Vertex.h"
#include <sstream>
using namespace std;
using namespace dagger;

dagger::Vertex::Vertex(int Id): id(Id){
}

dagger::Vertex::Vertex() {
}

dagger::Vertex::~Vertex() {
}

void dagger::Vertex::eraseOutEdge(int x){
	outList.erase(x);
}
EdgeList& dagger::Vertex::outEdges(){
	return outList;
}
EdgeList& dagger::Vertex::inEdges(){
	return inList;
}
int dagger::Vertex::outDegree(){
	return outList.size();
}
int dagger::Vertex::inDegree(){
	return inList.size();
}
int dagger::Vertex::degree(){
	return inList.size()+outList.size();
}
int dagger::Vertex::hasEdge(int endNode){
	return outList.find(endNode) != outList.end();
}
void dagger::Vertex::addInEdge(int sid){
	inList[sid]++;
}
void dagger::Vertex::addOutEdge(int tid){
	outList[tid]++;
}
