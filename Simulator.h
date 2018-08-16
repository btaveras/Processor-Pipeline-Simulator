#ifndef SIMULATOR_H_
#define SIMULATOR_H_

#include "Utilities.h"
#include <vector>
#include <string>
#include <iostream>
#include <ctime>
#include <queue>

using namespace std;

class Simulator {

    public:
		typedef enum {
			ADD,
			SUB,
			MOVC,
			MOV,
			MUL,
			AND,
			OR,
			EXOR,
			LOAD,
			STORE,
			BZ,
			BNZ,
			JUMP,
			BAL,
			HALT,
			NOTFOUND
		} instructs;

		struct fetch1Str{
			int instrucLine; //line where instruct is in the file
			string instruction;
		};

		//adding this for second part
		struct fetch2Str{
			int instrucLine;
			string instruction;
		};

		struct decRenameStr{
			bool stalled; //if IQ or LSQ or ROB or physical register not available (depending on the needs of the instruction)
			Simulator::instructs typeInstruc;
			int instrucLine; //line where instruct is in the file
			string instruction; //instruction string
			int PRDestIx; //index for the destination of the physical register, will be -1 if STORE
			int IQIx; //index referring to the entry in the IQ, will be -1 if LOAD/STORE
			int ROBIx; //for any type of instruction
			int PRSrc1Ix; //physical register source 1 index
			int PRSrc2Ix; //physical register source 2 index, -1 if literal
			int PRSrc3Ix; //physical register source 3 index, -1 if literal
			int lit; //literal value if there is such thing
		};

		struct decDispatchStr{
			int instrucLine; //line where instruct is in the file
			string instruction; //instruction string
		};
		//This structure is used for both an IQ entry and a LSQ entry

		struct IQ{
			int src1Tag; //physical register index
			int src1Value;
			bool src1Valid;
			int src2Tag; //its -1 if its literal
			int src2Value; //literal or value
			bool src2Valid;
			int src3Tag; //its -1 if its literal
			int src3Value; //literal or value
			bool src3Valid;
			bool iqEntryFree; //says if IQ entry free or noter
			bool counter; //the value will be zero by default the EX stage changes to 1 (this is to avoid that the instruction gets to the FU
			//in the same cycle it gets the values) (the instruction can only be grabbed by the FU if this is equal to 1)
			//also if it receives a forwarded value its set to one
			Simulator::instructs instrucType; //indicates type of instruction

			int timeAdded; //corresponding to the ageCounter value at that moment

			int ROBIx;
			int instrucLine;
			string instruction;
		};

		struct INTFU{
			int src1Value;
			int src2Value; //literal or value
			Simulator::instructs instrucType; //indicates type of instruction
			int ROBIx; //index of the instruction in the ROB
			////// Gissella
			int result;
			int zFlag;
			int instrucLine ;
			////// end Gissella
		};

		struct MEMFU{
			int src1Value;
			int src2Value; //literal or value
			int src3Value; //literal or value; this is used if its a STORE
			Simulator::instructs instrucType; //indicates type of instruction
			int ROBIx;
			//// Gissella
			int result;
			int address;
			
			/////end Gissella
		};

		struct MULFU{
			int mulCounter;//1 if its first cycle of the MULFU stage, 2 second and so on...
			int src1Value;
			int src2Value;
			Simulator::instructs instrucType; //indicates type of instruction
			int ROBIx;
			////// Gissella
			int result;
			int zFlag;
			////// end Gissella
		};

		int tailIx; //index for the tail of the ROB
		int headIx; //index for the head of the ROB

		struct ROB{
			int whereS; //1=first stage, 2=second...
			int archIX; //to store in ARF
			int prIx; //to store in physical register file
			Simulator::instructs instrucType; //indicates type of instruction
			int instrucLine;
			string instruction;
			bool ready; //shows if an instruction completed
		};

		struct PredictionBackup{
			int currentPC;
			int branchPC;
			int renTable[9];
			bool PRValid[16]; //physical register's value is valid or not
			bool PRAlloc[16]; //allocation of physical register 0=free 1=busy
			bool renamed[16]; //copy of rename array
			int lastPixZflag; //last physical destination register that recorded the result of an arithmethic or logic operation
		};

		int physRegs[16]; //values inside the physical registers
	//	int physRegsZFlags[16]; //values of the ZFlag for each physical register
		bool physRegsValid[16]; //physical register's value is valid or not
		bool physRegsAlloc[16]; //allocation of physical register 0=free 1=busy
		int renameTable[9]; //last physical register associated to that architectural register
		bool renamed[16];
		ROB arrayROB[17];
		IQ arrayIQ[8];
		std::deque<IQ> LSQ;//the same structure of the IQ can be used for the LSQ
		int regs[9]; //GPRs, 8th register corresponds to register X
		int lastInstrucPrIx; //last physical register index
		std::deque<PredictionBackup> predictBackupArr;
		
		int ageCounter; //counter to make sure the oldest instruction is executed first

		//structures for each of the FUs
		INTFU INTFUStr;
		MULFU MULFUStr;
		MEMFU MEMFUCyc1;
		MEMFU MEMFUCyc2;
		MEMFU MEMFUCyc3;

		//structures and latches for each of the stages
		fetch1Str fetch1Var;
		fetch1Str fetch1VarTemp;
		fetch2Str fetch2Var;
		fetch2Str fetch2VarTemp;
		decRenameStr decodeVar;
		decRenameStr decodeVarTemp;
		decDispatchStr decDispatchVar;

		//helper function that returns true if all sources of an iq entry are available
		bool allSourcesAvailable(IQ iqEntry);

		//Second part project functions
		void fetch1Stage();
		void fetch2Stage();
		void decode1Stage();
		void decode2Stage();
		void executeStage();
		int writeBackStage();

		void emptyFetch1(fetch1Str &fet); //used for empty fetch1
		void emptyFetch2(fetch2Str &fet); //used for empty fetch2
		void emptyDecode(decRenameStr &decod); //used for empty decode1
		void emptyDecode2(decDispatchStr &decod); //used for emptying decode/dispatch
		void emptyINTFU(INTFU &intFU); //used for emptying the INTFU
		void emptyMULFU(MULFU &mulFU); //used for emptying the MULFU
		void emptyMEMFU(MEMFU &memFU); //used for emptying any of the MEMFUs
		void emptyIQ(IQ &iq); //used for emptying an issue queue entry
		void emptyROB(ROB &rob); //used for emptying an ROB entry
		void createIQROBEntry(int IQIndex, int ROBIx, int arcIx, int prIndx);
		void flushPipeline(int robIx);
		bool sourceInUse(int index);


		Simulator(const std::string& fileName)
			: fileName(fileName)
        { }

		bool readInstrucFile();
		std::vector<std::string> split(const std::string &text, char del); //split a string by some delimiter sep
		instructs hashIt (std::string const& inString);
		void initialize();
		int simulate(int nCycles);
		void display();
		void displayStages();

		std::string fileName;
		int memory[10000]; //memory array
		int PC; //Program counter
		bool stoppedbyHalt; //used to make the simulator stop fetching instructions
		int lastLinIx; //index of last line of file, to verify PC not bigger than this

		std::vector<std::string> listInstructs;
};


#endif
