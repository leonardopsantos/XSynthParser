/*************************************************************************/
/*
 *  Circuit description classes
 *  by Gabriel Luca Nazar <glnazar@inf.ufrgs.br>
 *
 */
/*************************************************************************/

#include "circuit.h"

#include <boost/graph/graphviz.hpp>

using namespace std;


void Component::print(FILE* outfile){
	printf("Can't print type %d\n", type);
	return;
}

Component::Component(){
	inputs.clear();
	outputs.clear();
	locX=-1;
	locY=-1;
	depth = 0;
	pred = NULL;
	checker = NULL;
	return;
}

Component::~Component(){
	//cout << "deleting: " << name << "; id = " << id << endl;
}

/*************************************************************************/
Net::~Net(){
	outputs.clear();
	name.clear();
}

Net::Net(string netName, unsigned netId){
	name.assign(netName);
	id = netId;
	value = VARSIG;
	input = NULL;
	outputs.clear();
	isPI = false;
	isPO = false;
}

void Net::setInput(Component* newInput, sigVal newVal){
	if(input != NULL || value != VARSIG){
		cout << "Multiple drivers on net " << name << endl;
		if(input != NULL)
			cout << "Old input: " << input->name << endl;
		cout << "New input: " << newInput->name << endl;
		exit(0);
	}
				
	input = newInput;
	value = newVal;
}

string Net::getName(){
	if(isPO){
		string internal("internal_"); //This is required when a PO is used inside the circuit as well
		internal += name;
		return internal;
	}
	
	return name;
}

/*************************************************************************/
Short::Short(unsigned compId){
	name.assign("short_ckt");
	id = compId;
	type = SHORT;
}

void Short::print(FILE* outfile){
	fprintf(outfile, "  %s <= %s;\n", outputs[0]->getName().c_str(), inputs[0]->getName().c_str());
}

/*************************************************************************/
Lut::~Lut(){
	cout << "deleting " << name << ", id = " << id << endl;
}

Lut::Lut(string newName, unsigned compId){
	name.assign(newName);
	id = compId;
	type = LUT;
	tableO6.clear();
	tableO5.clear();
}

void Lut::printTable(FILE* outfile){
	unsigned char hexdigit, shiftOne;
	int i;
	
	hexdigit = 0;
	shiftOne = inputs.size() == 1 ? 2 : 8;
	for(i=tableO6.size()-1; i>=0; i--){
		if(tableO6[i])
			hexdigit |= shiftOne;
		if(i % 4 == 0){ //time to print a character
			fprintf(outfile, "%X", hexdigit);
			hexdigit = 0;
			shiftOne = 8;
		} else {
			shiftOne = shiftOne >> 1;
		}
	}
}

void Lut::print(FILE* outfile){
	int i;
	
	fprintf(outfile, "  %s : LUT%d\n", name.c_str(), inputs.size());
	fprintf(outfile, "    generic map (\n");
	fprintf(outfile, "      INIT => X\"");
	printTable(outfile);
	
	fprintf(outfile, "\"\n    )\n    port map (\n");
	
	for(i=0; i<inputs.size(); i++){
		fprintf(outfile, "      I%d => %s,\n", i, inputs[i]->getName().c_str());
	}
	fprintf(outfile, "      O => %s\n    );\n", outputs[0]->getName().c_str());
}

void Lut::print(FILE* outfile, map<Net*, Net*> &duplicatedNet){
	int i;
	
	fprintf(outfile, "  %s : LUT%d\n", name.c_str(), inputs.size());
	fprintf(outfile, "    generic map (\n");
	fprintf(outfile, "      INIT => X\"");
	printTable(outfile);
	
	fprintf(outfile, "\"\n    )\n    port map (\n");
	
	for(i=0; i<inputs.size(); i++){
		if(duplicatedNet.count(inputs[i]) == 1)
			fprintf(outfile, "      I%d => %s,\n", i, duplicatedNet[inputs[i]]->getName().c_str());
		else
			fprintf(outfile, "      I%d => %s,\n", i, inputs[i]->getName().c_str());
	}
	fprintf(outfile, "      O => %s\n    );\n", outputs[0]->getName().c_str());
}

void Lut::print(FILE* outfile, map<Net*, Net*> &duplicatedNet, map<Net*, unsigned> &copyIdx, map<Net*, vector<Component*> > &rSplitLuts){
	int i;
	
	fprintf(outfile, "  %s : LUT%d\n", name.c_str(), inputs.size());
	fprintf(outfile, "    generic map (\n");
	fprintf(outfile, "      INIT => X\"");
	printTable(outfile);
	
	fprintf(outfile, "\"\n    )\n    port map (\n");
	
	for(i=0; i<inputs.size(); i++)
		if(inputs[i]->isPI) {
			fprintf(outfile, "      I%d => %s,\n", i, inputs[i]->getName().c_str());
		} else {
			if(copyIdx.count(inputs[i]) == 0){
				//not printed in this group yet, prints the original Net
				fprintf(outfile, "      I%d => %s,\n", i, inputs[i]->getName().c_str());
				//and marks the printing
				copyIdx[inputs[i]] = 0;
			} else {
				//already printed..
				//if it has no split copies or they were all used
				if(rSplitLuts.count(inputs[i]) == 0 || rSplitLuts[inputs[i]].size() <= copyIdx[inputs[i]]){
					//time to print the duplicated net (it MUST have one, in this case)
					if(duplicatedNet.count(inputs[i]) == 0){
						cout << "problem at LUT " << name << ", input " << inputs[i]->name << ", id = " << inputs[i]->id << endl;
						exit(0);
					}
					fprintf(outfile, "      I%d => %s,\n", i, duplicatedNet[inputs[i]]->getName().c_str());
				} else {
					//prints the next copy and increases the counter
					fprintf(outfile, "      I%d => %s,\n", i, rSplitLuts[inputs[i]][copyIdx[inputs[i]]]->outputs[0]->getName().c_str());
					copyIdx[inputs[i]] += 1;
				}
			}
		}
	fprintf(outfile, "      O => %s\n    );\n", outputs[0]->getName().c_str());
}
//checker printer
void Lut::printChecker(FILE* outfile, map<Net*, vector<Component*> > &rSplitLuts){
	int i;
	
	fprintf(outfile, "  %s : LUT%d\n", name.c_str(), inputs.size());
	fprintf(outfile, "    generic map (\n");
	fprintf(outfile, "      INIT => X\"");
	printTable(outfile);
	
	fprintf(outfile, "\"\n    )\n    port map (\n");
	
	//the checker should always take the last copy available
	for(i=0; i<inputs.size(); i++)
		if(inputs[i]->isPI) {
			fprintf(outfile, "      I%d => %s,\n", i, inputs[i]->getName().c_str());
		} else {
			if(rSplitLuts.count(inputs[i]) == 0) //input with no copy whatsoever (MUST be the parity/copy net)
				fprintf(outfile, "      I%d => %s,\n", i, inputs[i]->getName().c_str());
			else
				fprintf(outfile, "      I%d => %s,\n", i, rSplitLuts[inputs[i]][rSplitLuts[inputs[i]].size()-1]->outputs[0]->getName().c_str());	
		}
	fprintf(outfile, "      O => %s\n    );\n", outputs[0]->getName().c_str());
}

//prints a direct checker (4-adj or unpaired-5)
void Lut::printDAdjCheck(FILE* outfile, map<Net*, Net*> &duplicatedNet, map<Net*, vector<Component*> > &rSplitLuts){
	int i;
	
	fprintf(outfile, "  %s : LUT%d\n", name.c_str(), inputs.size());
	fprintf(outfile, "    generic map (\n");
	fprintf(outfile, "      INIT => X\"");
	printTable(outfile);
	
	fprintf(outfile, "\"\n    )\n    port map (\n");
	
	//the checker should always take the last copy available
	for(i=0; i<inputs.size(); i++){
		if(duplicatedNet.count(inputs[i])){ //this is an input to the group, NOT the outputs of one of the protected LUTs
			fprintf(outfile, "      I%d => %s,\n", i, duplicatedNet[inputs[i]]->getName().c_str());
			continue;
		}
		
		if(rSplitLuts.count(inputs[i]) == 1){ //this may be an input to the group or one of the protected nets
			fprintf(outfile, "      I%d => %s,\n", i, rSplitLuts[inputs[i]][rSplitLuts[inputs[i]].size()-1]->outputs[0]->getName().c_str());
			continue;
		}
		//this may be a PI or a protected net
		fprintf(outfile, "      I%d => %s,\n", i, inputs[i]->getName().c_str());
	}
	fprintf(outfile, "      O => %s\n    );\n", outputs[0]->getName().c_str());
}
/*************************************************************************/
Buf::Buf(string newName, unsigned compId){
	name.assign(newName);
	id = compId;
	type = BUF;
}

void Buf::print(FILE* outfile){
	fprintf(outfile, "  %s : BUF\n", name.c_str());
	fprintf(outfile, "    port map (\n");
	fprintf(outfile, "      I => %s,\n", inputs[0]->getName().c_str());
	fprintf(outfile, "      O => %s\n    );\n", outputs[0]->getName().c_str());
}

/*************************************************************************/
Mux::Mux(string newName, unsigned compId){
	name.assign(newName);
	id = compId;
	type = MUX;
}

void Mux::print(FILE* outfile){
	
	switch(type){
		case MUXCY:
			fprintf(outfile, "  %s : MUXCY\n", name.c_str());
			break;
		case MUXF7:
			fprintf(outfile, "  %s : MUXF7\n", name.c_str());
			break;
		case MUXF8:
			fprintf(outfile, "  %s : MUXF8\n", name.c_str());
			break;
		default:
			printf("Unexpected MUX type %d\n", type);
	}
	
	fprintf(outfile, "    port map (\n");
	
	if(type == MUXCY){
		fprintf(outfile, "      CI => %s,\n", inputs[0]->getName().c_str());
		fprintf(outfile, "      DI => %s,\n", inputs[1]->getName().c_str());
	} else {
		fprintf(outfile, "      I0 => %s,\n", inputs[0]->getName().c_str());
		fprintf(outfile, "      I1 => %s,\n", inputs[1]->getName().c_str());
	}
	
	fprintf(outfile, "      S => %s,\n", inputs[2]->name.c_str());
	fprintf(outfile, "      O => %s\n    );\n", outputs[0]->getName().c_str());
	
}

/*************************************************************************/
Adder::Adder(string newName, unsigned compId){
	name.assign(newName);
	id = compId;
	type = ADDER;
}

void Adder::print(FILE* outfile){
	int i;
	fprintf(outfile, "  %s : CARRY4\n", name.c_str());
	fprintf(outfile, "    port map (\n");
	fprintf(outfile, "      CI => %s,\n", inputs[0]->getName().c_str());
	fprintf(outfile, "      CYINIT => %s,\n", inputs[1]->getName().c_str());	
	for(i=3; i>=0; i--)
		fprintf(outfile, "      DI(%d) => %s,\n", i, inputs[i+2]->getName().c_str());
	
	for(i=3; i>=0; i--)
		fprintf(outfile, "      S(%d) => %s,\n", i, inputs[i+6]->getName().c_str());
	
	for(i=3; i>=0; i--)
		fprintf(outfile, "      O(%d) => %s,\n", i, outputs[i]->getName().c_str());
	
	for(i=3; i>=1; i--)
		fprintf(outfile, "      CO(%d) => %s,\n", i, outputs[i+4]->getName().c_str());
	fprintf(outfile, "      CO(0) => %s\n", outputs[4]->getName().c_str());
	
	fprintf(outfile, "    );\n");
}

/*************************************************************************/
Xor::Xor(string newName, unsigned compId){
	name.assign(newName);
	id = compId;
	type = XORCY;
}

void Xor::print(FILE* outfile){
	fprintf(outfile, "  %s : XORCY\n", name.c_str());
	fprintf(outfile, "    port map (\n");
	fprintf(outfile, "      CI => %s,\n", inputs[0]->getName().c_str());
	fprintf(outfile, "      LI => %s,\n", inputs[1]->getName().c_str());
	fprintf(outfile, "      O => %s\n    );\n", outputs[0]->getName().c_str());	
}

/*************************************************************************/
Inv::Inv(string newName, unsigned compId){
	name.assign(newName);
	pred = NULL;
	checker = NULL;
	id = compId;
	type = INV;
	inputs.clear();
	outputs.clear();
	locX=-1;
	locY=-1;
	depth = 0;
}

/*************************************************************************/
FlipFlop::FlipFlop(string newName, unsigned compId, compType ffType){
	name.assign(newName);
	id = compId;
	type = ffType;
}

void FlipFlop::print(FILE* outfile){
	switch(type){
		case FDR:
			fprintf(outfile, "  %s : FDR\n", name.c_str());
			fprintf(outfile, "    port map (\n");
			fprintf(outfile, "      C => %s,\n", inputs[0]->getName().c_str());
			fprintf(outfile, "      D => %s,\n", inputs[1]->getName().c_str());
			fprintf(outfile, "      R => %s,\n", inputs[2]->getName().c_str());
			fprintf(outfile, "      Q => %s\n    );\n", outputs[0]->getName().c_str());
			break;
		case FDRS:
			fprintf(outfile, "  %s : FDRS\n", name.c_str());
			fprintf(outfile, "    port map (\n");
			fprintf(outfile, "      C => %s,\n", inputs[0]->getName().c_str());
			fprintf(outfile, "      D => %s,\n", inputs[1]->getName().c_str());
			fprintf(outfile, "      R => %s,\n", inputs[2]->getName().c_str());
			fprintf(outfile, "      S => %s,\n", inputs[3]->getName().c_str());
			fprintf(outfile, "      Q => %s\n    );\n", outputs[0]->getName().c_str());
			break;
		case FDRSE:
			fprintf(outfile, "  %s : FDRSE\n", name.c_str());
			fprintf(outfile, "    port map (\n");
			fprintf(outfile, "      C => %s,\n", inputs[0]->getName().c_str());
			fprintf(outfile, "      CE => %s,\n", inputs[1]->getName().c_str());
			fprintf(outfile, "      D => %s,\n", inputs[2]->getName().c_str());
			fprintf(outfile, "      R => %s,\n", inputs[3]->getName().c_str());
			fprintf(outfile, "      S => %s,\n", inputs[4]->getName().c_str());
			fprintf(outfile, "      Q => %s\n    );\n", outputs[0]->getName().c_str());
			break;
		case FDRE:
			fprintf(outfile, "  %s : FDRE\n", name.c_str());
			fprintf(outfile, "    port map (\n");
			fprintf(outfile, "      C => %s,\n", inputs[0]->getName().c_str());
			fprintf(outfile, "      CE => %s,\n", inputs[1]->getName().c_str());
			fprintf(outfile, "      D => %s,\n", inputs[2]->getName().c_str());
			fprintf(outfile, "      R => %s,\n", inputs[3]->getName().c_str());
			fprintf(outfile, "      Q => %s\n    );\n", outputs[0]->getName().c_str());
			break;
		default:
			printf("Unknown FF type %d\n", type);
	}	

}

Circuit::Circuit(){
}

void Circuit::printPIs() {
	for(std::vector<Net*>::iterator it = PIs.begin(); it != PIs.end(); ++it) {
		Net *n = *it;
	    cout << "PI: " << n->name << endl;
	}
}

void Circuit::addToGraph(vector<Component*> outputs, int depth)
{
	for(int ci=0; ci < outputs.size(); ci++) {
		Component *c = outputs[ci];
		for(int i = 0; i < depth; i++)
			cout << "\t";
		cout << "Component " << c->name << endl;
		for(int ni=0; ni < c->outputs.size(); ni++) {
			Net *n = c->outputs[ni];
			for(int i = 0; i < depth; i++)
				cout << "\t";
			cout << "Net " << n->name << endl;
			if( n->isPO == false ) {
				addToGraph(n->outputs, depth+1);
			} else {
				add_edge (c->key, n->key, this->circGraph);
				cout << "add_edge ("<< c->name <<"(" << c->key << "), "<< n->name <<"(" << n->key << ")" << ", this->circGraph);"<< endl;
				return;
			}
			for(int xi=0; xi < n->outputs.size(); xi++) {
				Component *cx = n->outputs[xi];
				add_edge (c->key, cx->key, this->circGraph);
				cout << "add_edge ("<< c->name <<"(" << c->key << "), "<< cx->name <<"(" << cx->key << ")" << ", this->circGraph);"<< endl;
			}
		}
	}

	return;
}

template <class Name>
class myEdgeWriter {
public:
     myEdgeWriter(Name _name, vector<Entity*> _allIn) : allIn(_allIn) {
     }
     template <class VertexOrEdge>
     void operator()(std::ostream& out, const VertexOrEdge& v) const {
            out << "[label=" << allIn[v]->name << "]";
     }
private:
     vector<Entity*> allIn;
};

void Circuit::buildPIsGraph() {

	myEdgeWriter<circGraph_t> w(this->circGraph, this->allIn);

	for(int pi=0; pi<PIs.size(); pi++) {
		Net *n = PIs[pi];
		cout << "PI: " << n->name << endl;
		addToGraph(n->outputs, 1);

		for(int xi=0; xi < n->outputs.size(); xi++) {
			Component *cx = n->outputs[xi];
			add_edge (n->key, cx->key, this->circGraph);
			cout << "add_edge ("<< n->name <<"(" << n->key << "), "<< cx->name <<"(" << cx->key << ")" << ", this->circGraph);"<< endl;
		}
	}

#if 0
	circGraph_t::vertex_iterator vertexIt, vertexEnd;
	circGraph_t::adjacency_iterator neighbourIt, neighbourEnd;
	tie(vertexIt, vertexEnd) = vertices(this->circGraph);
	for (; vertexIt != vertexEnd; ++vertexIt) {
//		cout << *vertexIt << " is connected with ";
		cout <<this->allIn[*vertexIt]->name << " is connected with ";
	    tie(neighbourIt, neighbourEnd) = adjacent_vertices(*vertexIt, this->circGraph);
	    for (; neighbourIt != neighbourEnd; ++neighbourIt)
//	    	cout << *neighbourIt << " ";
	    	cout <<this->allIn[*neighbourIt]->name << " ";
	    cout << "\n";
	}
#endif

	std::ofstream dot("graph.dot");
	boost::write_graphviz(dot, this->circGraph, w);
}
