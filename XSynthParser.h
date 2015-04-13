/*************************************************************************/
/*
 *  Parser for Xilinx post-synthesis netlist
 *  by Gabriel Luca Nazar <glnazar@inf.ufrgs.br>
 *
 */
/*************************************************************************/

#ifndef _INCLUDED_XSYNTHPARSER_H_
#define _INCLUDED_XSYNTHPARSER_H_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common.h"
#include "circuit.h"

#define BUF_SIZE 4096

using namespace std;

class XSynthParser{
	private:
		map<string, Net*> symbolTable;
		unsigned nextNet;
		unsigned nextComp;
		unsigned nextLut;
		void getNetName(char *src, char *dst);
		compType string2type(char *src);
		void parseTable(char *src, vector<bool> &table);
		void bufCleanup(Net* driver, Net* load);
	public:
		void parse(char *filename, Circuit &circ);
};

#endif
