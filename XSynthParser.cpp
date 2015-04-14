/*************************************************************************/
/*
 *  Parser for Xilinx post-synthesis netlist
 *  by Gabriel Luca Nazar <glnazar@inf.ufrgs.br>
 *
 */
/*************************************************************************/

#include <boost/foreach.hpp>
//using namespace boost;
//#define foreach BOOST_FOREACH

#include "XSynthParser.h"

using namespace std;


Circuit* circg;

void XSynthParser::parse(char *filename, Circuit &circ)
{
	ifstream inFile;
	char buf[BUF_SIZE], name[BUF_SIZE], bitName[BUF_SIZE], direction[4], type[128], to_downto[32], lutInit[18];
	Net* newNet;
	Lut* newLut;
	Short* newShort;
	Mux* newMux;
	Buf* newBuf;
	Xor* newXor;
	FlipFlop* newFF;
	unsigned int upper, lower, locX, locY;
	int i, j;
	bool end=false;
	Net *netTarg, *netSrc;
	
	inFile.open(filename);
	
	circ.allIn.clear();
	circ.components.clear();
	circ.nets.clear();
	circ.luts.clear();
	circ.PIs.clear();
	circ.POs.clear();
	circ.clocks.clear();
	circ.resets.clear();
	circ.sets.clear();
	circ.ffs.clear();
	circ.vccNet = NULL;
	circ.gndNet = NULL;
	
	//skips initial comments until the entity declaration is found
	inFile.getline(buf, BUF_SIZE);
	while(buf[0] != 'e')
		inFile.getline(buf, BUF_SIZE);
	
	//gets top entity name
	sscanf(buf, "entity %s is\n", name);
	circ.name.assign(name);
	cout << circ.name << endl;
	
	nextNet = 0; //Next net
	nextComp = 0; //Next component
	nextLut = 0;
	
	//gets PIs and POs from entity description
	inFile.getline(buf, BUF_SIZE); //"port (" line
	inFile.getline(buf, BUF_SIZE); //first port
	while(buf[2] != ')'){
		if(strstr(buf, " STD_LOGIC_VECTOR ")){
			sscanf(buf, "    %s : %s %s ( %d %s %d )", name, direction, type, &upper, to_downto, &lower);
			//dealing with a vector port
			if(upper < lower){ //checks if "upper" and "lower" limits are not switched
				//"to" declaration (not "downto")
				int aux = upper;
				upper = lower;
				lower = aux;
			}
			for(i=lower; i<=upper; i++){
				if(options[OPT_NOVEC])
					snprintf(bitName, BUF_SIZE, "%s_%d_nv", name, i); //generates this bit's name
				else
					snprintf(bitName, BUF_SIZE, "%s(%d)", name, i); //generates this bit's name
				newNet = new Net(bitName, nextNet++);
				if(direction[0] == 'i'){
					newNet->isPI = true;
					circ.PIs.push_back(newNet);
					circ.allIn.push_back(newNet);
				} else {
					newNet->isPO = true;
					circ.POs.push_back(newNet);
					circ.allIn.push_back(newNet);
				}
				circ.nets.push_back(newNet);
				//the symbolTable must maintain the original name, i.e., as if using STD_LOGIC_VECTOR
				if(options[OPT_NOVEC]){
					snprintf(bitName, BUF_SIZE, "%s(%d)", name, i); //generates this bit's name
					symbolTable[string(bitName)] = newNet;
				} else
				 	symbolTable[newNet->name] = newNet;
			}
			
		} else {
			sscanf(buf, "    %s : %s %s", name, direction, type);
			//std_logic port
			newNet = new Net(name, nextNet++);
			if(direction[0] == 'i') {
				newNet->isPI = true;
				circ.PIs.push_back(newNet);
				circ.allIn.push_back(newNet);
			} else {
				newNet->isPO = true;
				circ.POs.push_back(newNet);
				circ.allIn.push_back(newNet);
			}
			circ.nets.push_back(newNet);
			symbolTable[newNet->name] = newNet;
		}
		inFile.getline(buf, BUF_SIZE); //next port
	}
	
	inFile.getline(buf, BUF_SIZE); //end entity line
	while(buf[0] != 'a') //scans until "architecture" line is found
		inFile.getline(buf, BUF_SIZE);
		
	//reads all signal declarations
	while(buf[2] != 's')
		inFile.getline(buf, BUF_SIZE); //reads first signal
	while(buf[0] != 'b'){ //scans until "begin" line is found
		if(strstr(buf, " STD_LOGIC_VECTOR ")){
			sscanf(buf, "  signal %s : %s ( %d %s %d );", name, type, &upper, to_downto, &lower);
			//dealing with a vector signal
			if(upper < lower){ //checks if "upper" and "lower" limits are not switched
				//"to" declaration (not "downto")
				int aux = upper;
				upper = lower;
				lower = aux;
			}
			for(i=lower; i<=upper; i++){
				if(options[OPT_NOVEC])
					snprintf(bitName, BUF_SIZE, "%s_%d_nv", name, i); //generates this bit's name
				else
					snprintf(bitName, BUF_SIZE, "%s(%d)", name, i); //generates this bit's name
				newNet = new Net(bitName, nextNet++);
				circ.nets.push_back(newNet);
				//the symbolTable must maintain the original name, i.e., as if using STD_LOGIC_VECTOR
				if(options[OPT_NOVEC]){
					snprintf(bitName, BUF_SIZE, "%s(%d)", name, i); //generates this bit's name
					symbolTable[string(bitName)] = newNet;
				} else
				 	symbolTable[newNet->name] = newNet;
			}
			
		} else {
			sscanf(buf, "  signal %s : %s;", name, type);
			//std_logic signal
			newNet = new Net(name, nextNet++);
			circ.nets.push_back(newNet);
			symbolTable[newNet->name] = newNet;
		}
		
		
		inFile.getline(buf, BUF_SIZE);
	}
	
	//parses all components
	inFile.getline(buf, BUF_SIZE); //gets the first component line
	while(1){
		if(strchr(buf, ':')){
			//component instance
			compType ctype;
				
			sscanf(buf, "  %s : %s\n", name, type);
			ctype = string2type(type);
			
			if(ctype == UNKNOWN)
				cout << "unknown type found: " << type << endl;
			
			switch(ctype){
				case LUT:
					newLut = new Lut(name, nextLut++);
					
					inFile.getline(buf, BUF_SIZE); //skips "generic map("
					
					inFile.getline(buf, BUF_SIZE); //"INIT" line
					sscanf(buf, "      INIT => X\"%s\"", lutInit);
					parseTable(lutInit, newLut->tableO6);
					
					inFile.getline(buf, BUF_SIZE); //skips ")"
					inFile.getline(buf, BUF_SIZE); //skips "port map("
					inFile.getline(buf, BUF_SIZE); //reads first input
					while(buf[6] == 'I') { //reads all inputs
						getNetName(buf + 12, name);
						netSrc = symbolTable[name];
						//if(netSrc->id == 17)
						//	cout << "name: " << newLut->name << endl;
						newLut->inputs.push_back(netSrc);
						netSrc->outputs.push_back(newLut);
						inFile.getline(buf, BUF_SIZE); //reads next input
					}
					//parses output
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newLut->outputs.push_back(netTarg);
					netTarg->setInput(newLut);
					circ.luts.push_back(newLut);
					circ.allIn.push_back(newLut);
					inFile.getline(buf, BUF_SIZE); //skips ");"
					break;
				
				case BUF:
					newBuf = new Buf(name, nextComp);
					inFile.getline(buf, BUF_SIZE); //skips "generic map"
					while(buf[4] != 'p')
						inFile.getline(buf, BUF_SIZE); //skips everything until the port map
					
					inFile.getline(buf, BUF_SIZE); //input line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newBuf->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newBuf);
					
					inFile.getline(buf, BUF_SIZE); //output line
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newBuf->outputs.push_back(netTarg);
					netTarg->setInput(newBuf);
					if(netTarg->isPO){
						circ.components.push_back(newBuf);
						nextComp++;
					}
					circ.allIn.push_back(newBuf);
					
					inFile.getline(buf, BUF_SIZE); //skips ");"
					break;
					
				case MUXF7:
				case MUXF8:
				case MUXCY:
					newMux = new Mux(name, nextComp++);
					
					newMux->type = ctype;
					if(ctype == MUXF8)
						cout << "WARNING!! MUXF8 found!\n\n\n" << endl;
					inFile.getline(buf, BUF_SIZE); //skips "port map("
					inFile.getline(buf, BUF_SIZE); //I0 (CI) line
					getNetName(buf + 12, name);
					netSrc = symbolTable[name];
					newMux->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newMux);
					
					inFile.getline(buf, BUF_SIZE); //I1 (DI) line
					getNetName(buf + 12, name);
					netSrc = symbolTable[name];
					newMux->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newMux);
					
					inFile.getline(buf, BUF_SIZE); //S line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newMux->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newMux);
					
					inFile.getline(buf, BUF_SIZE); //O line
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newMux->outputs.push_back(netTarg);
					netTarg->setInput(newMux);
					
					circ.components.push_back(newMux);
					circ.allIn.push_back(newMux);
					
					inFile.getline(buf, BUF_SIZE); //skips ");"				
					break;
				
				case INV: //an inverter will be mapped into a LUT by ISE, so I might do it right now
					newLut = new Lut(name, nextLut++);
					inFile.getline(buf, BUF_SIZE); //skips "generic map"
					while(buf[4] != 'p')
						inFile.getline(buf, BUF_SIZE); //skips everything until the port map
					//inverter truth table
					newLut->tableO6.resize(2);
					newLut->tableO6[0] = true;
					newLut->tableO6[1] = false;
					inFile.getline(buf, BUF_SIZE); //input line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newLut->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newLut);
					
					inFile.getline(buf, BUF_SIZE); //output line
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newLut->outputs.push_back(netTarg);
					netTarg->setInput(newLut);
					circ.luts.push_back(newLut);
					circ.allIn.push_back(newLut);
					
					inFile.getline(buf, BUF_SIZE); //skips ");"
					break;
				
				case XORCY:
					newXor = new Xor(name, nextComp++);
					
					inFile.getline(buf, BUF_SIZE); //skips "port map("
					inFile.getline(buf, BUF_SIZE); //CI line
					getNetName(buf + 12, name);
					netSrc = symbolTable[name];
					newXor->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newXor);
					
					inFile.getline(buf, BUF_SIZE); //LI line
					getNetName(buf + 12, name);
					netSrc = symbolTable[name];
					newXor->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newXor);
					
					inFile.getline(buf, BUF_SIZE); //O line
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newXor->outputs.push_back(netTarg);
					netTarg->setInput(newXor);
					
					circ.components.push_back(newXor);
					circ.allIn.push_back(newXor);
					
					inFile.getline(buf, BUF_SIZE); //skips ");"
					
					break;
					
				case DSP48E:
					while(buf[5] != ';')
						inFile.getline(buf, BUF_SIZE);
					break;
					
				case FDR:
					newFF = new FlipFlop(name, nextComp++, FDR);
					inFile.getline(buf, BUF_SIZE); //skips "port map("
					
					inFile.getline(buf, BUF_SIZE); //(C) clock line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.clocks.insert(netSrc); //This net is a clock, it may need special treatment
					
					inFile.getline(buf, BUF_SIZE); //D line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					
					inFile.getline(buf, BUF_SIZE); //R line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.resets.insert(netSrc); //This net is a reset, it may need special treatment
					if(!(strstr(name, "GLNRST") && strlen(name) == 6)){
						fprintf(stderr, "Rst named: %s\n", name);
						exit(0);
					}
					
					inFile.getline(buf, BUF_SIZE); //Q line
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newFF->outputs.push_back(netTarg);
					netTarg->setInput(newFF);
					
					circ.ffs.push_back(newFF);
					circ.allIn.push_back(newFF);
					inFile.getline(buf, BUF_SIZE); //skips ");"		
					break;
				
				case FDRS:
					newFF = new FlipFlop(name, nextComp++, FDRS);
					inFile.getline(buf, BUF_SIZE); //skips "port map("
					
					inFile.getline(buf, BUF_SIZE); //(C) clock line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.clocks.insert(netSrc); //This net is a clock, it may need special treatment
					
					inFile.getline(buf, BUF_SIZE); //D line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					
					inFile.getline(buf, BUF_SIZE); //R line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.resets.insert(netSrc); //This net is a reset, it may need special treatment
					if(!(strstr(name, "GLNRST") && strlen(name) == 6)){
						fprintf(stderr, "Rst named: %s\n", name);
						exit(0);
					}
					
					inFile.getline(buf, BUF_SIZE); //S line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.sets.insert(netSrc); //This net is a set, it may need special treatment
					if(strstr(name, "GLNRST")){
						fprintf(stderr, "Set named: %s\n", name);
						exit(0);
					}
					
					inFile.getline(buf, BUF_SIZE); //Q line
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newFF->outputs.push_back(netTarg);
					netTarg->setInput(newFF);
					
					circ.ffs.push_back(newFF);
					circ.allIn.push_back(newFF);
					inFile.getline(buf, BUF_SIZE); //skips ");"		
					break;
				
				case FDRSE:
					newFF = new FlipFlop(name, nextComp++, FDRSE);
					inFile.getline(buf, BUF_SIZE); //skips "port map("
					
					inFile.getline(buf, BUF_SIZE); //(C) clock line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.clocks.insert(netSrc); //This net is a clock, it may need special treatment
					
					inFile.getline(buf, BUF_SIZE); //CE line
					getNetName(buf + 12, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					
					inFile.getline(buf, BUF_SIZE); //D line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					
					inFile.getline(buf, BUF_SIZE); //R line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.resets.insert(netSrc); //This net is a reset, it may need special treatment
					if(!(strstr(name, "GLNRST") && strlen(name) == 6)){
						fprintf(stderr, "Rst named: %s\n", name);
						exit(0);
					}
					
					inFile.getline(buf, BUF_SIZE); //S line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.sets.insert(netSrc); //This net is a set, it may need special treatment
					if(strstr(name, "GLNRST")){
						fprintf(stderr, "Set named: %s\n", name);
						exit(0);
					}
					
					inFile.getline(buf, BUF_SIZE); //Q line
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newFF->outputs.push_back(netTarg);
					netTarg->setInput(newFF);
					
					circ.ffs.push_back(newFF);
					circ.allIn.push_back(newFF);
					inFile.getline(buf, BUF_SIZE); //skips ");"		
					break;
					
				case FDRE:
					newFF = new FlipFlop(name, nextComp++, FDRE);
					inFile.getline(buf, BUF_SIZE); //skips "port map("
					
					inFile.getline(buf, BUF_SIZE); //(C) clock line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.clocks.insert(netSrc); //This net is a clock, it may need special treatment
					
					inFile.getline(buf, BUF_SIZE); //CE line
					getNetName(buf + 12, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					
					inFile.getline(buf, BUF_SIZE); //D line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					
					inFile.getline(buf, BUF_SIZE); //R line
					getNetName(buf + 11, name);
					netSrc = symbolTable[name];
					newFF->inputs.push_back(netSrc);
					netSrc->outputs.push_back(newFF);
					circ.resets.insert(netSrc); //This net is a reset, it may need special treatment
					if(!(strstr(name, "GLNRST") && strlen(name) == 6)){
						fprintf(stderr, "Rst named: %s\n", name);
						exit(0);
					}
					
					inFile.getline(buf, BUF_SIZE); //Q line
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					newFF->outputs.push_back(netTarg);
					netTarg->setInput(newFF);
					
					circ.ffs.push_back(newFF);
					circ.allIn.push_back(newFF);
					inFile.getline(buf, BUF_SIZE); //skips ");"		
					break;
				
				case ZERO:
					inFile.getline(buf, BUF_SIZE); //skips "generic map"
					while(buf[4] != 'p') //skips everything until the port map
						inFile.getline(buf, BUF_SIZE); //some X_ZEROs have generic map..
					
					inFile.getline(buf, BUF_SIZE); //reads output
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					netTarg->setInput(NULL, GND);
					circ.gndNet = netTarg;
					
					inFile.getline(buf, BUF_SIZE); //skips ");"
					break;				
				
				case ONE:
					inFile.getline(buf, BUF_SIZE); //skips "generic map"
					while(buf[4] != 'p') //skips everything until the port map
						inFile.getline(buf, BUF_SIZE);
					
					inFile.getline(buf, BUF_SIZE); //reads output
					getNetName(buf + 11, name);
					netTarg = symbolTable[name];
					netTarg->setInput(NULL, VCC);
					circ.vccNet = netTarg;
					
					inFile.getline(buf, BUF_SIZE); //skips ");"
					break;
				
				default:
					end = true;
					break;
			}
			if(end) //if an unknown component was found, then it's the end (usually an X_)
				break;
		} else {
			if(!strstr(buf, "=")){
				cout << "Parsing error at line: " << buf << endl;
				exit(0);
			}
			
			//short circuit
			newShort = new Short(nextComp++);
			
			getNetName(buf+2, name);
			netTarg = symbolTable[name];
			newShort->outputs.push_back(netTarg);
			
			getNetName(strchr(buf, '=')+2, name);
			netSrc = symbolTable[name];
			newShort->inputs.push_back(netSrc);
			
			netTarg->setInput(newShort);
			
			netSrc->outputs.push_back(newShort);
			
			//if(netTarg->isPO){
				circ.components.push_back(newShort);
				circ.allIn.push_back(newShort);
			//	nextComp++;
			//}
		}
		inFile.getline(buf, BUF_SIZE); //gets next line
		while(buf[0] == 0) //skips empty lines
			inFile.getline(buf, BUF_SIZE);
		if(buf[0] == 'e') //end reached
			break;
	}

#if 0
	circg  = &circ;
	//buffer and short circuit clean-up phase
	for(i=circ.nets.size()-1; i>=0; i--){
		Net *n;
		n = circ.nets[i];
		cout << "net " << i << " = "<< n->getName() << endl;
		if(!n->isPO) {
			if(!n->isPI && n->input == NULL && n->value == VARSIG){
				cout << n->getName() << " not PI with NULL input! name: \"" << n->name << "\"" << "i = " << i << endl;
				exit(0);
			}
			if((n->isPI || n->value != VARSIG) || (n->input->type != SHORT && n->input->type != BUF))
				bufCleanup(n, n);
			else
				circ.nets.erase(circ.nets.begin() + i);
		}
	}
#endif

	//counts flip-flops with constant input
	circ.constInputFFs=0;
	for(i=0; i<circ.ffs.size(); i++) {
		if(circ.ffs[i]->type == FDR || circ.ffs[i]->type == FDRS){
			if(circ.ffs[i]->inputs[1]->value != VARSIG)
				circ.constInputFFs++;
		} else if(circ.ffs[i]->type == FDRE || circ.ffs[i]->type == FDRSE) {
			if(circ.ffs[i]->inputs[2]->value != VARSIG)
				circ.constInputFFs++;
		} else {
			cout << "Unknown FF type " << circ.ffs[i]->type << " at parser" << endl;
		}
	}
	
	for(i=0; i<circ.luts.size(); i++)
		if(i != circ.luts[i]->id)
			cout << circ.luts[i]->name << " i = " << i << "; id = " << circ.luts[i]->id << endl;
	
	for(i=0; i<circ.nets.size(); i++)
		circ.nets[i]->id = i;
	
	for(i=0; i<circ.components.size(); i++)
		circ.components[i]->id = i;

	for(i=0; i<circ.allIn.size(); i++) {
		circ.allIn[i]->key = i;
		cout << circ.allIn[i]->name << ".key = " << circ.allIn[i]->key << endl;
	}
	cout << endl << endl;
}
/*************************************************************************/
void XSynthParser::getNetName(char *src, char *dst){
	int i;
	
	for(i=0; src[i] != ' ' && src[i] != 0 && src[i] != ',' && src[i] != ';'; i++)
		dst[i] = src[i];
	
	dst[i] = 0;
}

/*************************************************************************/
compType XSynthParser::string2type(char *src){
	if(strstr(src, "LUT"))
		return LUT;
		
	/*if(strstr(src, "BUFG"))
		return BUF;*/

	if(strstr(src, "IBUF"))
		return BUF;

	if(strstr(src, "OBUF"))
		return BUF;

	if(strstr(src, "MUXF7"))
		return MUXF7;
		
	if(strstr(src, "MUXF8"))
		return MUXF8;
	
	if(strstr(src, "MUXCY"))
		return MUXCY;
	
	if(strstr(src, "XORCY"))
		return XORCY;
	
	if(strstr(src, "INV"))
		return INV;
	
	if(strstr(src, "FDR") && strlen(src) == 3)
		return FDR;
		
	if(strstr(src, "FDRS") && strlen(src) == 4)
		return FDRS;
	
	if(strstr(src, "FDRSE") && strlen(src) == 5)
		return FDRSE;
	
	if(strstr(src, "FDRE") && strlen(src) == 4)
		return FDRE;
		
	//if(strstr(src, "DSP48E"))
	//	return DSP48E;
		
	if(strstr(src, "GND"))
		return ZERO;
		
	if(strstr(src, "VCC"))
		return ONE;
	
	return UNKNOWN;
}

/*************************************************************************/
void XSynthParser::parseTable(char *src, vector<bool> &table){
	int i, j;
	char aux[2];
	int val;
	bool swapvar;
	
	aux[1] = 0;
	
	for(i=0; src[i] != 0 && src[i] != '"'; i++){
		aux[0] = src[i];
		sscanf(aux, "%X", &val);
		for(j=0; j<4; j++)
			table.push_back((val & (8 >> j)) != 0);
	}
	//mirrored table
	for(i=0; i < table.size()/2; i++){
		swapvar = table[i];
		table[i] = table[table.size() - (i+1)];
		table[table.size() - (i+1)] = swapvar;
	}
}
/*************************************************************************/
void XSynthParser::bufCleanup(Net* driver, Net* load){
	int i, j, k;
	
	if(!load->isPO){
		BOOST_FOREACH(Component* comp2, load->outputs)
			if(comp2->type == BUF || comp2->type == SHORT)
				bufCleanup(driver, comp2->outputs[0]);
		
		for(i=0; i<load->outputs.size(); i++){
			Component* comp = load->outputs[i];
			if((comp->type == BUF || comp->type == SHORT) && !comp->outputs[0]->isPO){
				for(j=0; j<comp->outputs[0]->outputs.size(); j++) //components driven by the buffer output
					for(k=0; k<comp->outputs[0]->outputs[j]->inputs.size(); k++) //inputs of those components
						if(comp->outputs[0]->outputs[j]->inputs[k] == comp->outputs[0]){
							comp->outputs[0]->outputs[j]->inputs[k] = driver;
							driver->outputs.push_back(comp->outputs[0]->outputs[j]);
						}
				cout << "removed buffer! out = " << comp->outputs[0]->name << endl;
				
				circg->components.erase(circg->components.begin()+comp->id);
				
				for(j=comp->id; j<circg->components.size(); j++)
					circg->components[j]->id = j;
					
				delete comp->outputs[0];
				delete comp;
				
				load->outputs.erase(load->outputs.begin()+i);
				i--;
			}
		}
	}
}
