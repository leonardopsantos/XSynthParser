
#include "circuit.h"
#include "XSynthParser.h"
#include <cstdlib>

using namespace std;

bool options[OPT_CNT];

int main(int argc, char *argv[])
{
	options[OPT_NOVEC] = false;

	Circuit circ;
	XSynthParser parser;

	if( argc < 2 )
		return -1;

	parser.parse(argv[1], circ);

	circ.buildPIsGraph();
}
