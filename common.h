
#ifndef _INCLUDED_COMMON_H_
#define _INCLUDED_COMMON_H_

#include <boost/foreach.hpp>
using namespace boost;
#define foreach BOOST_FOREACH
#define MAX(a, b) ((a > b) ? a : b)
#define dbprint(a) cout << #a << " = " << a << endl

#define BUFSIZE 512

enum lineOpts {OPT_USE_7, OPT_D_ADDERS, OPT_AGGRESSIVE, OPT_E_AGGREG, OPT_DWCF, OPT_CMUX, OPT_RSPLIT, OPT_MERGE_IO,
		OPT_NOVEC, OPT_DWCC, OPT_USE4ADJ, OPT_MMATCH, OPT_2RAIL, OPT_SPLIT_CHECK, OPT_STACK_CHECK, OPT_FF_INPUT_CMP,
		OPT_DUPLICATE_MUXES, OPT_NOCHECK, OPT_NODUPLNET, OPT_DUPLICATE_PI, OPT_NO_CMP_MODULE, OPT_CCHAIN};

#define OPT_CNT (((int) OPT_CCHAIN) + 1)

#define DEFAULT_MAXCHECK_CC 4 //Default value for maximum checker size
#define DEFAULT_MAXGROUP_CC 5 //Default value for maximum group size

extern bool options[OPT_CNT];

#endif
