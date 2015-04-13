/*************************************************************************/
/*
 *  Circuit description classes
 *  by Gabriel Luca Nazar <glnazar@inf.ufrgs.br>
 *
 */
/*************************************************************************/

#ifndef _INCLUDED_CIRCUIT_H_
#define _INCLUDED_CIRCUIT_H_

#include <string>
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include "common.h"

using namespace std;

//possible components:
//LUT: Lookup table (general amount of inputs)
//LUT6 and LUT5: Lookup table with 6 or 5 inputs (used for post-map parsing only)
//MUXF: Multiplexer (for larger functions implementation)
//MUXCY: Multiplexer (for carry propagation)
//XORCY: the XOR gate dedicated to adding/subtracting
//MUX: A general multiplexer
//BUF: Buffers
//INV: Inverter
//FLIPFLOP: Flipflop or latch
//ADDER: X_CARRY4 adders
//DSP48: multiplier blocks
//SHORT: direct connection between two signals
//ZERO: component that sets a signal to GND permanently
//ONE: component that sets a signal to VCC permanently
enum compType { LUT, LUT6, LUT5, MUXF7, MUXF8, MUXCY, XORCY, MUX, BUF, INV, FDR, 
		FDRE, FDRS, FDRSE, ADDER, DSP48E, SHORT, ZERO, ONE, UNKNOWN };

//possible signal values:
//GND: signal has constant value zero
//VCC: signal has constant value one
//VARSIG: regular signal with variable value
enum sigVal { GND, VCC, VARSIG };

class Net;

class Component {
	public:
		unsigned id; //component's id (internal)
		compType type; //component's type
		vector<Net*> inputs; //input Nets
		vector<Net*> outputs; //output Nets
		Component* checker;
		Component* pred;
		Component(); //constructor with no arguments (called automatically when instantiating subclasses)
		
		float depth;
	
		string name; //The LUT's name
		int locX; //The X position of the slice this LUT is in
		int locY; //The Y position of the slice this LUT is in
		virtual void print(FILE* outfile); //prints the VHDL instantiation of this component
		~Component();
};

class FlipFlop: public Component {
	public:
		FlipFlop(string newName, unsigned compId, compType ffType);
		void print(FILE* outfile); //prints the VHDL instanciation of a flip-flop
};

class Lut: public Component {
	public:
		vector<bool> tableO6; //Truth table
		vector<bool> tableO5;
		unsigned validInputs; //Amount of inputs that are different from VCC or GND
		void print(FILE* outfile); //prints the VHDL instantiation of a LUT
		void print(FILE* outfile, map<Net*, Net*> &duplicatedNet); //prints the VHDL instantiation of a LUT
									  //using the inputs copies whenever possible
		void print(FILE* outfile, map<Net*, Net*> &duplicatedNet, map<Net*, unsigned> &copyIdx, map<Net*, vector<Component*> > &rSplitLuts);
		//checker printer
		void printChecker(FILE* outfile, map<Net*, vector<Component*> > &rSplitLuts);
		void printDAdjCheck(FILE* outfile, map<Net*, Net*> &duplicatedNet, map<Net*, vector<Component*> > &rSplitLuts);
		void printTable(FILE* outfile);
		~Lut();
		Lut(string newName, unsigned compId); //LUT constructor
};

class Mux: public Component { //the third input is the selection line
	public:
		Mux(string newName, unsigned compId);
		void print(FILE* outfile); //prints the VHDL instanciation of a MUX
};

class Inv: public Component { //the third input is the selection line
	public:
		Inv(string newName, unsigned compId);
};

class Xor: public Component { //the third input is the selection line
	public:
		Xor(string newName, unsigned compId);
		void print(FILE* outfile); //prints the VHDL instanciation of a XOR
};

class Buf: public Component {
	public:
		Buf(string newName, unsigned compId);
		void print(FILE* outfile); //prints the VHDL instanciation of a BUF
};

class Adder: public Component {
	public:
		Adder(string newName, unsigned compId);
		void print(FILE* outfile); //prints the VHDL instanciation of a CARRY4
};

class Short: public Component {
	public:
		Short(unsigned compId); //class constructor
		void print(FILE* outfile); //prints a VHDL "short circuit" (a signal assignment)
};

class Net {
	public:
		unsigned id; //Net's id
		
		bool isPI; //if it is a primary input
		bool isPO; //if it is a primary output
		
		vector<Component*> outputs; //Components that read this signal
		Component* input; //The component that drives this signal
		
		string name;
		
		sigVal value; //if this net has a constant value or if it is variable
		
		Net(string netName, unsigned netId); //class constructor
		~Net(); //class destructor
		void setInput(Component* newInput, sigVal newVal=VARSIG); //assigns a new input or constant value to the net
		string getName(); //gets the net name, with the "internal_" prefix, if required
};

class Circuit {
	public:
		vector<Component*> components; //components (combinational and that are not LUTs)
		vector<Lut*> luts; //LUT-only vector
		vector<Net*> nets;
		vector<FlipFlop*> ffs;
		set<Net*> clocks; //nets that are clocks
		set<Net*> resets; //nets that are resets
		set<Net*> sets; //nets that are resets
		
		unsigned constInputFFs;
		
		Net* gndNet;
		Net* vccNet;
	
		vector<Net*> PIs;
		vector<Net*> POs;
	
		string name;

		void printPIs();
		void buildPIsGraph();
};

#endif
