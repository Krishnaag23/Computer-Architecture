/*
 * Copyright (C) 2007-2026 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! 
 *  CS422 HW2 PIN Tool 
 *  Krishna Agrawal 230574 Group 2 
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <set>
#include <queue>

using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::set;
using std::queue;
/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 fastForwardCount = 0;
UINT64 icount = 0;
UINT64 key = 0;
vector <UINT64> branchTrace;
UINT64 prevInstrAddr;
set<UINT64> intrestingAddress;
set<UINT64> notIntrestingAddress;
queue <UINT64> history;

std::ostream* out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "",  "specify output file name");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

INT32 Usage()
{
    cerr << "PIN tool for CS422 Assignment 2 - Group 2 - Krishna Agrawal (230574)" << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/*
 * Analysis
 */

void checkTaken(ADDRINT ip, bool taken){
	if( ip != history.front() ) {
			if(intrestingAddress.count(ip) || notIntrestingAddress.count(ip)) {
				notIntrestingAddress.insert(ip);
				history.pop();
				history.push(ip);
				return;
			}
	}

	history.pop();
	history.push(ip);

	if (history.front() == history.back()) {
		notIntrestingAddress.insert(ip);
		return;	
	}

	if (notIntrestingAddress.count(ip)) return;

	if (!intrestingAddress.count(ip)){
		if (!taken && notIntrestingAddress.count(history.front())) {
			notIntrestingAddress.insert(ip); 
			return;
		}

	}
	
	intrestingAddress.insert(ip);
	branchTrace.push_back(ip);
	if (taken) branchTrace.push_back(1);
	else branchTrace.push_back(0);

}

/* ===================================================================== */
// Exit and reporting
/* ===================================================================== */

void MyExitRoutine()
{
	for (UINT64 i = 0; i < branchTrace.size(); i++){
		*out << branchTrace[i] <<  endl;
	}
    exit(0);
}

/* ===================================================================== */
// Instrumentation callback
/* ===================================================================== */

VOID Trace(TRACE trace, VOID* v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
		//check conditional branches
	if (INS_IsBranch(ins) && INS_HasFallThrough(ins)) {
		INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(checkTaken), IARG_INST_PTR, IARG_BRANCH_TAKEN ,IARG_END);
	}
	}
    }
}

/* ===================================================================== */
// Fini (fallback if application exits naturally)
/* ===================================================================== */

VOID Fini(INT32 code, VOID* v)
{
    MyExitRoutine();
}

/* ===================================================================== */
// main
/* ===================================================================== */

int main(int argc, char* argv[])
{
	history.push(0);
	history.push(0);

    if (PIN_Init(argc, argv)) return Usage();

    string fileName   = KnobOutputFile.Value();

    if (!fileName.empty()) {
        out = new std::ofstream(fileName.c_str());
    }

    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "CS422 Assignment 2 PIN Tool - Group 2 - Krishna Agrawal (230574)" << endl;
    if (!fileName.empty())
        cerr << "Output file: " << fileName << endl;
    cerr << "===============================================" << endl;

    PIN_StartProgram();
    return 0;
}
