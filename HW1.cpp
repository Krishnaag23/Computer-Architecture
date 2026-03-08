/*
 * Copyright (C) 2007-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <set>
#include <map>
using std::cerr;
using std::endl;
using std::string;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount    = 0; //number of dynamically executed instructions
UINT64 bblCount    = 0; //number of dynamically executed basic blocks
UINT64 threadCount = 0; //total number of threads, including main thread
UINT64 fastForwardCount = 0; //total number of instructions  in billion, to skip
UINT64 icount = 0;

UINT64 singleChunkIns = 0;
UINT64 multipleChunkIns = 0;
UINT64 singleChunkData = 0;
UINT64 multipleChunkData = 0;

set<UINT32> instructionAddress;
set<UINT32> dataAddress;
map<UINT32, UINT64> instructionSize; 
map<UINT32, UINT64> dataReadSize;

std::ostream* out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for MyPinTool output");
KNOB< UINT64 > KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "specify fast forward amount in billions");
KNOB< BOOL > KnobCount(KNOB_MODE_WRITEONCE, "pintool", "count", "1",
                       "count instructions, basic blocks and threads in the application");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the number of dynamically executed " << endl
         << "instructions, basic blocks and threads in the application." << endl
         << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/*!
 * Increase counter of the executed basic blocks and instructions.
 * This function is called for every basic block when it is about to be executed.
 * @param[in]   numInstInBbl    number of instructions in the basic block
 * @note use atomic operations for multi-threaded applications
 */
VOID CountBbl(UINT32 numInstInBbl)
{
    bblCount++;
    insCount += numInstInBbl;
}

/*!
 * Add each unique instruction access to a set. 
 */
VOID ins_footprint(UINT32 startAddr, UINT32 endAddr){
	UINT32 nextAddr = startAddr + 32;
	if (endAddr < nextAddr) singleChunkIns++;
	else multipleChunkIns++;
	while(startAddr <= endAddr){
		instructionAddress.insert(startAddr);
		startAddr += 32;
	}
}

/*!
 * Add each Data access to a set. 
 */
VOID data_footprint(ADDRINT memAddr, UINT32 memSize){
    UINT32 mAddr = (UINT32) memAddr;
    // Start address with granularity of 32 bytes.. 
    UINT32 startAddr = (mAddr/32)*32;
    UINT32 nextAddr = startAddr + 32;
    UINT32 endAddr = ((mAddr + memSize)/32)*32;
    if (endAddr < nextAddr) singleChunkData++;
    else multipleChunkData++;
    while(startAddr <= endAddr){
        dataAddress.insert(startAddr);
        startAddr += 32;
    }
}
/*!
 * Increase counter for the number of instructions.
 * */
VOID InsCount(){
icount++;
}

ADDRINT Terminate(void)
{
        return (icount >= fastForwardCount + 1,000,000,000);
}

// Analysis routine to check fast-forward condition
ADDRINT FastForward(void) {
	return (icount >= fastForwardCount && icount);
}

// Analysis routine to exit the application
void MyExitRoutine(){
	// Do an exit system call to exit the application.
	// As we are calling the exit system call PIN would not be able to instrument application end.
	// Because of this, even if you are instrumenting the application end, the Fini function would not
	// be called. Thus you should report the statistics here, before doing the exit system call.

	// Results etc
	*out << "Part C Analysis Results: " << endl;
	*out << "------------------------------------------------------------" << endl;

	*out << "Instruction Footprint Blocks     			= " << instrAddr.size() << endl;
    	*out << "Instruction Footprint (in bytes) 			= " << 32*instrAddr.size() << endl;
    	*out << "Data Footprint Blocks            			= " << dataAddr.size() << endl;
    	*out << "Data Footprint (in bytes)        			= " << 32*dataAddr.size() << endl;
	*out << "Number of Single Memory Chunk Instruction access 	=" << singleChunkIns << endl;
	*out << "Number of Multiple Memory Chunk Instruction access 	=" << multipleChunkIns << endl;
	*out << "Number of Single Memory Chunk data access 		=" << singleChunkData << endl;
	*out << "Number of Multiple Memory Chunk data access 		=" << multipleChunkData << endl;
    	*out << "============================================================" << endl;
	exit(0);
}

// Predicated analysis routine
void MyPredicatedAnalysis(...) {
	// analysis code
}
/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

/*!
 * Insert call to the CountBbl() analysis routine before every basic block 
 * of the trace.
 * This function is called every time a new trace is encountered.
 * @param[in]   trace    trace to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
VOID Trace(TRACE trace, VOID* v)
{
    // Visit every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to CountBbl() before every basic bloc, passing the number of instructions
	BBL_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
	BBL_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)MyExitRoutine, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBbl, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
	for(INS ins=BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)){
		UINT32 startAddr, endAddr;
		UINT32 insAddr = INS_Address(ins);
		UINT32 insSize = INS_Size(ins);
		startAddr = (insAddr/32)*32;
		endAddr = ((insAddr + insSize)/32)*32;
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_footprint, IARG_UINT32, startAddr, IARG_UINT32, endAddr, IARG_END);

		UINT32 memOperands = INS_MemoryOperandCount(ins);
		 for (UINT32 memOper = 0; memOper < memOperands; memOper++){
			UINT32 memSize = INS_MemoryOperandSize(ins, memOper);
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)data_footprint, IARG_MEMORYOP_EA, memOper, IARG_UINT32, memSize, IARG_END);
                totalMemSize += memSize;
		 }
	}
	INS_InsertIfCall(ins, IPOINT_BEFORE, FastForward, IARG_END);
	INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, MyPredicatedAnalysis, ..., IARG_END); // for instructions with true predicates

	INS_InsertIfCall(ins, IPOINT_BEFORE, FastForward, IARG_END);
	INS_InsertThenCall(ins, IPOINT_BEFORE, MyAnalysis, ..., IARG_END);  // for all instructions

	INS_InsertCall(ins, IPOINT_BEFORE, InsCount, IARG_END);


    }
}

/*!
 * Increase counter of threads in the application.
 * This function is called for every thread created by the application when it is
 * about to start running (including the root thread).
 * @param[in]   threadIndex     ID assigned by PIN to the new thread
 * @param[in]   ctxt            initial register state for the new thread
 * @param[in]   flags           thread creation flags (OS specific)
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddThreadStartFunction function call
 */
VOID ThreadStart(THREADID threadIndex, CONTEXT* ctxt, INT32 flags, VOID* v) { threadCount++; }

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v)
{
    *out << "===============================================" << endl;
    *out << "MyPinTool analysis results: " << endl;
    *out << "Number of instructions: " << insCount << endl;
    *out << "Number of basic blocks: " << bblCount << endl;
    *out << "Number of threads: " << threadCount << endl;
    *out << "===============================================" << endl;
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    string fileName = KnobOutputFile.Value();
    fastForwardCount = KnobFastForward.Value() * 1,000,000,000;

    if (!fileName.empty())
    {
        out = new std::ofstream(fileName.c_str());
    }

    if (KnobCount)
    {
        // Register function to be called to instrument traces
        TRACE_AddInstrumentFunction(Trace, 0);

        // Register function to be called for every thread before it starts running
	PIN_AddThreadStartFunction(ThreadStart, 0);

        // Register function to be called when the application exits
        PIN_AddFiniFunction(Fini, 0);
    }

    cerr << "===============================================" << endl;
    cerr << "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty())
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr << "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
