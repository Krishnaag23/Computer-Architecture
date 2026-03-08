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
using std::set;
using std::map;
using std::max;
using std::min;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount    = 0; //number of dynamically executed instructions
UINT64 bblCount    = 0; //number of dynamically executed basic blocks
UINT64 threadCount = 0; //total number of threads, including main thread
UINT64 fastForwardCount = 0; //total number of instructions  in billion, to skip
UINT64 icount = 0;

// Variables related to Part C 
UINT64 singleChunkIns = 0;
UINT64 multipleChunkIns = 0;
UINT64 singleChunkData = 0;
UINT64 multipleChunkData = 0;

set<UINT32> instructionAddress;
set<UINT32> dataAddress;
map<UINT32, UINT64> instructionSize; 
map<UINT32, UINT64> dataReadSize;

// Part D 
map<UINT32, UINT64> insLength; 
map<UINT32, UINT64> insOp; 
map<UINT32, UINT64> readReg; 
map<UINT32, UINT64> writeReg;
map<UINT32, UINT64> memOp;
map<UINT32, UINT64> memReadOp;
map<UINT32, UINT64> memWriteOp;
UINT32 maxMemBytes = 0;
UINT64 totalMemBytes = 0;
INT32 maxImm = INT32_MIN;
INT32 minImm = INT32_MAX;
INT32 maxDisp = INT32_MIN;
INT32 minDisp = INT32_MAX;

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

void PrintResults(const string &title, const map<UINT32, UINT64> &data) {
    *out << title << " Results: " << endl;
    for (const auto &entry : data) {
        *out << entry.first << " : " << entry.second << endl;
    }
    *out << endl;
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

VOID ins_distribution(
    UINT32 insSize, 
    UINT32 opCount, 
    UINT32 insReadReg, 
    UINT32 insWriteReg
){
    insLength[insSize]++;
    insOp[opCount]++;
    readReg[insReadReg]++; 
    writeReg[insWriteReg]++;
    
}

VOID mem_distribution(
    UINT32 numRead, 
    UINT32 numWrite,
    UINT32 totalMemSize
){
    memOp[numRead+numWrite]++; 
    memReadOp[numRead]++; 
    memWriteOp[numWrite]++;
    maxMemBytes = max(maxMemBytes, totalMemSize);
    totalMemBytes += totalMemSize;
}

VOID imm_distribution(ADDRINT immValue){
    INT32 imm = (INT32) immValue;
    maxImm = max(maxImm, imm);
    minImm = min(minImm, imm);
}

VOID disp_distribution(ADDRINT dispValue){
// using Int32 as only analysing 32 bit binaries, so ADDRDELTA 
// would just be INT_32.
    INT32 disp = (INT32) dispValue;
    maxDisp = max(maxDisp, disp);
    minDisp = min(minDisp, disp);
}

/*!
 * Increase counter for the number of instructions.
 * */
VOID InsCount(){
icount++;
}

ADDRINT Terminate(void)
{
        return (icount >= fastForwardCount + 1e9);
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

	UINT64 memInsCount = 0;
	for(const auto &entry : memOp){
		if(entry.first > 0) memInsCount += entry.second;
	}
	double avgMemBytes = (memInsCount == 0) ? 0.0 : ((1.0*totalMemBytes) / memInsCount);

	// Remove no memory operand instruction from being counted.
	memReadOp[0] = memReadOp[0]-memOp[0];
    	memWriteOp[0] = memWriteOp[0]-memOp[0];

	*out << "Part C Analysis Results: " << endl;
	*out << "------------------------------------------------------------" << endl;

	*out << "Instruction Footprint Blocks     			= " << instructionAddress.size() << endl;
    	*out << "Instruction Footprint (in bytes) 			= " << 32*instructionAddress.size() << endl;
    	*out << "Data Footprint Blocks            			= " << dataAddress.size() << endl;
    	*out << "Data Footprint (in bytes)        			= " << 32*dataAddress.size() << endl;
	*out << "Number of Single Memory Chunk Instruction access 	=" << singleChunkIns << endl;
	*out << "Number of Multiple Memory Chunk Instruction access 	=" << multipleChunkIns << endl;
	*out << "Number of Single Memory Chunk data access 		=" << singleChunkData << endl;
	*out << "Number of Multiple Memory Chunk data access 		=" << multipleChunkData << endl;
    	*out << "============================================================" << endl;

	*out << "Part D Analysis Results: " << endl;
	*out << "------------------------------------------------------------" << endl;
	PrintResults("Distribution of instruction length", insLength);
	PrintResults("Distribution of the number of operands in an instruction", insOp);
	*out << "Number of register read operands in an instruction(with 2 register read operands):	" << readReg[2] << endl;
	*out << "Number of register write operands in an instruction(with 1 register write operand):	" <<  writeReg[1] << endl;
	*out << "Number of instruction with 3 memory operands:						" <<  memOp[3] << endl;
	*out << "Memory Instruction Read Operand(with 1 memory read operand)				" << memReadOp[1] << endl;
	*out << "Memory Instruction write Operand(with 2 memory write operand)				" << memWriteOp[2] << endl;
	*out << "Maximum number of bytes touched by an instruction : " << maxMemBytes << endl;
	*out << "Average number of bytes touched by an instruction : " << std::fixed << std::setprecision(6) << avgMemBytes << endl;
	*out << "Maximum value of immediate : " << maxImm << endl;
	*out << "Minimum value of immediate : " << minImm << endl;
	*out << "Maximum value of displacement used in memory addressing : " << maxDisp << endl;
	*out << "Minimum value of displacement used in memory addressing : " << minDisp << endl;
	exit(0);
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
	BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
	BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)MyExitRoutine, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBbl, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
	for(INS ins=BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)){
		UINT32 startAddr, endAddr;
		UINT32 insAddr = INS_Address(ins);
		UINT32 insSize = INS_Size(ins);
		startAddr = (insAddr/32)*32;
		endAddr = ((insAddr + insSize)/32)*32;

		UINT32 opCount = INS_OperandCount(ins);
		UINT32 insReadReg = INS_MaxNumRRegs(ins);
		UINT32 insWriteReg = INS_MaxNumWRegs(ins);

		UINT32 memOperands = INS_MemoryOperandCount(ins);
		UINT32 numRead = 0, numWrite = 0;
		UINT32 totalMemSize = 0;

		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_footprint, IARG_UINT32, startAddr, IARG_UINT32, endAddr, IARG_END);

		for (UINT32 memOper = 0; memOper < memOperands; memOper++){
			UINT32 memSize = INS_MemoryOperandSize(ins, memOper);
			
			if (INS_OperandIsMemory(ins, memOper)) {
			    ADDRDELTA displacement = INS_OperandMemoryDisplacement(ins, memOper);
			    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
			    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)disp_distribution, IARG_ADDRINT, displacement, IARG_END);
                }
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                	INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)data_footprint, IARG_MEMORYOP_EA, memOper, IARG_UINT32, memSize, IARG_END);
		 }

		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            	INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_distribution,
				IARG_UINT32, insSize, 
                            	IARG_UINT32, opCount, 
                            	IARG_UINT32, insReadReg, 
                            	IARG_UINT32, insWriteReg, 
                            	IARG_END);

		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
		INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_distribution,
                                IARG_UINT32, numRead, 
                                IARG_UINT32, numWrite,
                                IARG_UINT32, totalMemSize,
                                IARG_END);

		UINT32 operandCount = INS_OperandCount(ins);
            	for (UINT32 opIdx = 0; opIdx < operandCount; opIdx++) {
                	if (INS_OperandIsImmediate(ins, opIdx)) {
                    		INT32 immValue = INS_OperandImmediate(ins, opIdx);
                    		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                    		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)imm_distribution, IARG_ADDRINT, immValue,IARG_END);
                	}
            	}
	}
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
    MyExitRoutine();
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
    fastForwardCount = KnobFastForward.Value() * 1e9;

    if (!fileName.empty())
    {
        out = new std::ofstream(fileName.c_str());
    }

    if (KnobCount)
    {
        // Register function to be called to instrument traces
        TRACE_AddInstrumentFunction(Trace, 0);

        // Register function to be called for every thread before it starts running
	//PIN_AddThreadStartFunction(ThreadStart, 0);

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
