#include "Simulator.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <algorithm>

using namespace std;

Simulator::instructs Simulator::hashIt (string const& inString) {
	if (inString.compare("ADD")==0) return Simulator::instructs::ADD;
	if (inString.compare("SUB")==0) return Simulator::instructs::SUB;
	if (inString.compare("MOVC")==0) return Simulator::instructs::MOVC;
	if (inString.compare("MOV")==0) return Simulator::instructs::MOV;
	if (inString.compare("MUL")==0) return Simulator::instructs::MUL;
	if (inString.compare("AND")==0) return Simulator::instructs::AND;
	if (inString.compare("OR")==0) return Simulator::instructs::OR;
	if (inString.compare("EX-OR")==0) return Simulator::instructs::EXOR;
	if (inString.compare("LOAD")==0) return Simulator::instructs::LOAD;
	if (inString.compare("STORE")==0) return Simulator::instructs::STORE;
	if (inString.compare("BZ")==0) return Simulator::instructs::BZ;
	if (inString.compare("BNZ")==0) return Simulator::instructs::BNZ;
	if (inString.compare("JUMP")==0) return Simulator::instructs::JUMP;
	if (inString.compare("BAL")==0) return Simulator::instructs::BAL;
	if (inString.compare("HALT")==0) return Simulator::instructs::HALT;

	return Simulator::instructs::NOTFOUND;
}

//read the file with the instructions and store it in the list
//bool to indicate if successful read or not
bool Simulator::readInstrucFile(){
	string line;
	ifstream myfile (this->fileName);

	lastLinIx=-1;
	if (myfile.is_open()){
		while (getline (myfile,line)){
			this->listInstructs.push_back(line);
			lastLinIx++;
		}

		myfile.close();
	}
	else {
		cout << "Unable to open file" << endl;
		return false;
	}
	return true;
}


void Simulator::initialize(){
	ageCounter=1;
	//restart simulator
	int i=0;

	for(i=0;i<9;i++){
		regs[i]=0;
		renameTable[i]=-1;
	}

	for(i=0;i<10000;i++){
		memory[i]=0;
	}

	PC=20000;

	headIx=0;
	tailIx=0;

	for(i=0;i<16;i++){
		physRegs[i]=0;
		physRegsValid[i]=false;
		physRegsAlloc[i]=false;
		renamed[i]=false;
	}

	for(i=0;i<17;i++){
		emptyROB(arrayROB[i]);
	}

	while (!LSQ.empty()){
		LSQ.pop_front();
	}

	for(i=0;i<8;i++){
		emptyIQ(arrayIQ[i]);
	}

	predictBackupArr.clear();

	emptyINTFU(INTFUStr);
	emptyMULFU(MULFUStr);
	emptyMEMFU(MEMFUCyc1);
	emptyMEMFU(MEMFUCyc2);
	emptyMEMFU(MEMFUCyc3);

	emptyFetch1(fetch1Var);
	emptyFetch1(fetch1VarTemp);
	emptyFetch2(fetch2Var);
	emptyFetch2(fetch2VarTemp);
	emptyDecode(decodeVar);
	emptyDecode(decodeVarTemp);
	emptyDecode2(decDispatchVar);
	lastInstrucPrIx=-1;
	stoppedbyHalt=false;
}

void Simulator::fetch1Stage(){
	//see if decode stage stalled before fetching an instruction
	if(!decodeVar.stalled){
		fetch1VarTemp=fetch1Var;

		emptyFetch1(fetch1Var);

		if(!stoppedbyHalt && (PC-20000 <= lastLinIx) && (PC-20000>=0)){
			fetch1Var.instrucLine=PC;
			fetch1Var.instruction=listInstructs[PC-20000];
			PC++;
		}else{
		//	if(!stoppedbyHalt)
			//	std::cout << "ERROR FETCH 1\n";
		}
	}
}

void Simulator::fetch2Stage(){

	//pass from fetch1 to fetch2
	//see if decode stage stalled before moving from fetch1 to here

	if(!decodeVar.stalled){
		fetch2VarTemp=fetch2Var;
		emptyFetch2(fetch2Var);

		fetch2Var.instrucLine=fetch1VarTemp.instrucLine;
		fetch2Var.instruction=fetch1VarTemp.instruction;
	}
}

bool Simulator::sourceInUse(int index){
	if(index!=-1){
		if(physRegsValid[index]==false) //if the physical register's valididity is equal to false then source is in use
			return true;
	}
	return false;
}

void Simulator::createIQROBEntry(int IQIndex, int ROBIx, int arcIx, int prIndx){
	//create entry in IQ
	arrayIQ[IQIndex].iqEntryFree=false;
	arrayIQ[IQIndex].counter=false;
	arrayIQ[IQIndex].instrucType=decodeVar.typeInstruc;
	arrayIQ[IQIndex].instrucLine=decodeVar.instrucLine;
	arrayIQ[IQIndex].instruction=decodeVar.instruction;
	arrayIQ[IQIndex].src1Tag=decodeVar.PRSrc1Ix;
	arrayIQ[IQIndex].src2Tag=decodeVar.PRSrc2Ix;
	arrayIQ[IQIndex].src3Tag=decodeVar.PRSrc3Ix;
		
	arrayIQ[IQIndex].timeAdded=ageCounter;
	
	ageCounter++;

	arrayIQ[IQIndex].ROBIx=ROBIx;

	emptyROB(arrayROB[ROBIx]);
	//create entry in ROB
	arrayROB[ROBIx].whereS=3; //decode1
	arrayROB[ROBIx].archIX=arcIx;
	arrayROB[ROBIx].prIx=prIndx;
	arrayROB[ROBIx].instrucType=decodeVar.typeInstruc;
	arrayROB[ROBIx].instrucLine=decodeVar.instrucLine;
	arrayROB[ROBIx].instruction=decodeVar.instruction;
}

void Simulator::decode1Stage(){
	

	Utilities util;

	//if stalled then empty the decode info to be passed to the next stage
	if(decodeVar.stalled)
		emptyDecode(decodeVarTemp);
	else//copy before editing
		decodeVarTemp=decodeVar;

	emptyDecode(decodeVar);

	decodeVar.instrucLine=fetch2VarTemp.instrucLine;
	decodeVar.instruction=fetch2VarTemp.instruction;

	if(decodeVar.instrucLine!=-1){
		vector<string> parts=util.split(fetch2VarTemp.instruction);

		decodeVar.typeInstruc=hashIt(parts[0]);
		int ROBIx=-1;

		//if the tail+1 mod 17 different from head
		//it means ROB not full store the element in that index and increase tail in one
		if(!(((tailIx+1)%17)==headIx)){
			//if ROB is not full
			ROBIx=tailIx;

			if(decodeVar.typeInstruc== Simulator::instructs::ADD || decodeVar.typeInstruc== Simulator::instructs::SUB ||
			decodeVar.typeInstruc== Simulator::instructs::MUL || decodeVar.typeInstruc== Simulator::instructs::AND ||
			decodeVar.typeInstruc== Simulator::instructs::OR || decodeVar.typeInstruc== Simulator::instructs::EXOR){
			
				int phyIndex=-1;
				int IQIndex=-1;

				//See if there is free Physical register
				for(int i=0;i<16;i++){
					if(!physRegsAlloc[i]){ //if physical register not allocated use this as destination physical register
						phyIndex=i;
						break;
					}
				}

				//See if there is free IQ entry
				for(int i=0;i<8;i++){
					if(arrayIQ[i].iqEntryFree){ //if iq entry free use this as new IQ entry
						IQIndex=i;
						break;
					}
				}

				if(phyIndex!=-1 && IQIndex!=-1){
					decodeVar.PRSrc1Ix=renameTable[parts[2].at(1)-'0'];
					decodeVar.PRSrc2Ix=renameTable[parts[3].at(1)-'0'];

					int prevPr=renameTable[parts[1].at(1)-'0'];
					if(prevPr!=-1)
						renamed[prevPr]=true;
						//physRegsAlloc[prevPr]=false; //de-allocate physical register instance
					physRegsValid[phyIndex]=false; //make new physical register valid bit equal to false
					renamed[phyIndex]=false;
					renameTable[parts[1].at(1)-'0']=phyIndex; //parts[1].at(1)-'0' corresponds to the index of the destination register
					physRegsAlloc[phyIndex]=true;
					//makes free physical register be recorded as new instance for architectural register

					decodeVar.PRDestIx=phyIndex;

					
					decodeVar.IQIx=IQIndex;
					decodeVar.ROBIx=ROBIx;

					createIQROBEntry(IQIndex, ROBIx, (int)(parts[1].at(1)-'0'), phyIndex);

					lastInstrucPrIx=phyIndex;

				}else{
					decodeVar.stalled=true;
				}

			}else if(decodeVar.typeInstruc== Simulator::instructs::LOAD){

				int phyIndex=-1;

				//See if there is free Physical register
				for(int i=0;i<16;i++){
					if(!physRegsAlloc[i]){ //if physical register not allocated use this as destination physical register
						phyIndex=i;
						break;
					}
				}
				unsigned int intendedSize = 4;
				if(phyIndex!=-1 && (LSQ.size()<intendedSize)){
					decodeVar.PRSrc1Ix=renameTable[parts[2].at(1)-'0'];

					//if it's a register
					if(parts[3].at(0)=='R'){
						decodeVar.PRSrc2Ix=renameTable[parts[3].at(1)-'0'];
					}
					else{
						//if second source is a literal
						decodeVar.PRSrc2Ix=-1;
						decodeVar.lit=stoi(parts[3]);
					}


					int prevPr=renameTable[parts[1].at(1)-'0'];
					if(prevPr!=-1)
						renamed[prevPr]=true;
					//if(prevPr!=-1)
						//physRegsAlloc[prevPr]=false; //de-allocate physical register instance
					physRegsValid[phyIndex]=false; //make new physical register valid bit equal to false
					renamed[phyIndex]=false;
					renameTable[parts[1].at(1)-'0']=phyIndex; //parts[1].at(1)-'0' corresponds to the index of the destination register
					//makes free physical register be recorded as new instance for architectural register
					physRegsAlloc[phyIndex]=true;
					decodeVar.PRDestIx=phyIndex;
					

					decodeVar.ROBIx=ROBIx;
					decodeVar.IQIx=LSQ.size();

					//create entry in LSQ
					IQ LSQentry;
					emptyIQ(LSQentry);
					LSQentry.counter=false;
					LSQentry.iqEntryFree=false;
					LSQentry.instrucType=decodeVar.typeInstruc;
					LSQentry.instruction=decodeVar.instruction;
					LSQentry.instrucLine=decodeVar.instrucLine;
					LSQentry.src1Tag=decodeVar.PRSrc1Ix;
					LSQentry.src2Tag=decodeVar.PRSrc2Ix;
					LSQentry.src3Tag=decodeVar.PRSrc3Ix;
			
					LSQentry.ROBIx=ROBIx;
					LSQ.push_back(LSQentry);

					emptyROB(arrayROB[ROBIx]);
					//create entry in ROB
					arrayROB[ROBIx].whereS=3; //decode1
					arrayROB[ROBIx].archIX=(int)(parts[1].at(1)-'0');
					arrayROB[ROBIx].prIx=phyIndex;
					arrayROB[ROBIx].instrucType=decodeVar.typeInstruc;
					arrayROB[ROBIx].instrucLine=decodeVar.instrucLine;
					arrayROB[ROBIx].instruction=decodeVar.instruction;
				}else{
					decodeVar.stalled=true;
				}
			}else if(decodeVar.typeInstruc== Simulator::instructs::STORE){
				unsigned int intendedSize = 4;

				if(LSQ.size()<intendedSize){
					decodeVar.PRSrc1Ix=renameTable[parts[1].at(1)-'0'];
					decodeVar.PRSrc2Ix=renameTable[parts[2].at(1)-'0'];

					//if it's a register
					if(parts[3].at(0)=='R'){
						decodeVar.PRSrc3Ix=renameTable[parts[3].at(1)-'0'];
					}
					else{
						//if third source is a literal
						decodeVar.PRSrc3Ix=-1;
						decodeVar.lit=stoi(parts[3]);
					}

					decodeVar.ROBIx=ROBIx;
					decodeVar.IQIx=LSQ.size();

					//create entry in LSQ
					IQ LSQentry;
					emptyIQ(LSQentry);
					LSQentry.counter=false;
					LSQentry.iqEntryFree=false;
					LSQentry.instrucType=decodeVar.typeInstruc;
					LSQentry.instruction=decodeVar.instruction;
					LSQentry.instrucLine=decodeVar.instrucLine;
					LSQentry.src1Tag=decodeVar.PRSrc1Ix;
					LSQentry.src2Tag=decodeVar.PRSrc2Ix;
					LSQentry.src3Tag=decodeVar.PRSrc3Ix;

					LSQentry.ROBIx=ROBIx;
					LSQ.push_back(LSQentry);

					emptyROB(arrayROB[ROBIx]);
					//create entry in ROB
					arrayROB[ROBIx].whereS=3; //decode1
					arrayROB[ROBIx].archIX=-1;
					arrayROB[ROBIx].prIx=-1;
					arrayROB[ROBIx].instrucType=decodeVar.typeInstruc;
					arrayROB[ROBIx].instrucLine=decodeVar.instrucLine;
					arrayROB[ROBIx].instruction=decodeVar.instruction;
				}else{
					decodeVar.stalled=true;
				}

			}else if(decodeVar.typeInstruc== Simulator::instructs::MOVC){
				int phyIndex=-1;
				int IQIndex=-1;
				//See if there is free Physical register
				for(int i=0;i<16;i++){
					if(!physRegsAlloc[i]){ //if physical register not allocated use this as destination physical register
						phyIndex=i;
						break;
					}
				}

				//See if there is free IQ entry
				for(int i=0;i<8;i++){
					if(arrayIQ[i].iqEntryFree){ //if iq entry free use this as new IQ entry
						IQIndex=i;
						break;
					}
				}

				if(phyIndex!=-1 && IQIndex!=-1){
					decodeVar.PRSrc1Ix=-1;
					decodeVar.lit=stoi(parts[2]);
					

					int prevPr=renameTable[parts[1].at(1)-'0'];
					if(prevPr!=-1)
						renamed[prevPr]=true;
					//if(prevPr!=-1)
						//physRegsAlloc[prevPr]=false; //de-allocate physical register instance
					physRegsValid[phyIndex]=false; //make new physical register valid bit equal to false
					physRegsAlloc[phyIndex]=true;
					renamed[phyIndex]=false;
					renameTable[parts[1].at(1)-'0']=phyIndex; //parts[1].at(1)-'0' corresponds to the index of the destination register
					//makes free physical register be recorded as new instance for architectural register

					decodeVar.PRDestIx=phyIndex;
					
					decodeVar.IQIx=IQIndex;
					decodeVar.ROBIx=ROBIx;

					createIQROBEntry(IQIndex, ROBIx, (int)(parts[1].at(1)-'0'), phyIndex);

				}else{
					decodeVar.stalled=true;
				}
			}else if(decodeVar.typeInstruc== Simulator::instructs::MOV){
				int phyIndex=-1;
				int IQIndex=-1;
				//See if there is free Physical register
				for(int i=0;i<16;i++){
					if(!physRegsAlloc[i]){ //if physical register not allocated use this as destination physical register
						phyIndex=i;
						break;
					}
				}

				//See if there is free IQ entry
				for(int i=0;i<8;i++){
					if(arrayIQ[i].iqEntryFree){ //if iq entry free use this as new IQ entry
						IQIndex=i;
						break;
					}
				}

				if(phyIndex!=-1 && IQIndex!=-1){
					if(parts[2].compare("X")==0){
						decodeVar.PRSrc1Ix=renameTable[8];
					}else{
						decodeVar.PRSrc1Ix=renameTable[parts[2].at(1)-'0'];
					}

					int prevPr=renameTable[parts[1].at(1)-'0'];
					if(prevPr!=-1)
						renamed[prevPr]=true;
					//if(prevPr!=-1)
						//physRegsAlloc[prevPr]=false; //de-allocate physical register instance
					physRegsValid[phyIndex]=false; //make new physical register valid bit equal to false
					renamed[phyIndex]=false;
					renameTable[parts[1].at(1)-'0']=phyIndex; //parts[1].at(1)-'0' corresponds to the index of the destination register
					//makes free physical register be recorded as new instance for architectural register
					physRegsAlloc[phyIndex]=true;

					decodeVar.PRDestIx=phyIndex;
				
					

					decodeVar.IQIx=IQIndex;
					decodeVar.ROBIx=ROBIx;

					createIQROBEntry(IQIndex, ROBIx, (int)(parts[1].at(1)-'0'), phyIndex);

				}else{
					decodeVar.stalled=true;
				}
			}
			else if(decodeVar.typeInstruc== Simulator::instructs::BAL){
				int phyIndex=-1;
				int IQIndex=-1;
				
				//See if there is free Physical register
				for(int i=0;i<16;i++){
					if(!physRegsAlloc[i]){ //if physical register not allocated use this as destination physical register
						phyIndex=i;
						break;
					}
				}
				//See if there is free IQ entry
				for(int i=0;i<8;i++){
					if(arrayIQ[i].iqEntryFree){ //if iq entry free use this as new IQ entry
						IQIndex=i;
						break;
					}
				}

				if(IQIndex!=-1 && phyIndex!=-1){
					decodeVar.PRSrc1Ix=renameTable[parts[1].at(1)-'0'];
					decodeVar.lit= stoi(parts[2]);


					int prevPr=renameTable[8];
					if(prevPr!=-1)
						renamed[prevPr]=true;
					//if(prevPr!=-1)
						//physRegsAlloc[prevPr]=false; //de-allocate physical register instance
					physRegsValid[phyIndex]=false; //make new physical register valid bit equal to false
					renamed[phyIndex]=false;
					renameTable[8]=phyIndex; //8 is the index for the X register
					//makes free physical register be recorded as new instance for X register

					physRegsAlloc[phyIndex]=true;

					decodeVar.PRDestIx=phyIndex;
				
					
					decodeVar.IQIx=IQIndex;
					decodeVar.ROBIx=ROBIx;

					//architectural register 8 corresponds to X register
					//pass the new allocated physical register
					createIQROBEntry(IQIndex, ROBIx, 8, phyIndex);

					int k=0;
					PredictionBackup branch_backup;
					branch_backup.currentPC = PC;
					branch_backup.branchPC= decodeVar.lit + decodeVar.instrucLine;
					for (k=0; k<9; k++){
						branch_backup.renTable[k] = renameTable[k];
					}
					for (k=0; k<16; k++){
						branch_backup.PRValid [k] = physRegsValid[k];
					}
					for (k=0; k<16; k++){
						branch_backup.PRAlloc [k] = physRegsAlloc[k];
					}
					for (k=0; k<16; k++){
						branch_backup.renamed[k] = renamed[k];
					}
					//copy last instruction physical destination register that modified the zflag
					branch_backup.lastPixZflag=lastInstrucPrIx;
					predictBackupArr.push_back(branch_backup);
					
				}else{//stall if no physical register available or iq entry available
					decodeVar.stalled=true;
				}

			}else if(decodeVar.typeInstruc== Simulator::instructs::JUMP){
				int IQIndex=-1;

				//See if there is free IQ entry
				for(int i=0;i<8;i++){
					if(arrayIQ[i].iqEntryFree){ //if iq entry free use this as new IQ entry
						IQIndex=i;
						break;
					}
				}

				if(IQIndex!=-1){
					if(parts[1].compare("X")==0){
						decodeVar.PRSrc1Ix=renameTable[8];
					}else{
						decodeVar.PRSrc1Ix=renameTable[parts[1].at(1)-'0'];
					}

					decodeVar.lit= stoi(parts[2]);

					decodeVar.IQIx=IQIndex;
					decodeVar.ROBIx=ROBIx;

					createIQROBEntry(IQIndex, ROBIx, -1, -1);

					
					int k=0;
					PredictionBackup branch_backup;
					branch_backup.currentPC = PC;
					branch_backup.branchPC= decodeVar.lit + decodeVar.instrucLine;
					for (k=0; k<9; k++){
						branch_backup.renTable[k] = renameTable[k];
					}
					for (k=0; k<16; k++){
						branch_backup.PRValid [k] = physRegsValid[k];
					}
					for (k=0; k<16; k++){
						branch_backup.PRAlloc [k] = physRegsAlloc[k];
					}
					for (k=0; k<16; k++){
						branch_backup.renamed[k] = renamed[k];
					}
					//copy last instruction physical destination register that modified the zflag
					branch_backup.lastPixZflag=lastInstrucPrIx;
					predictBackupArr.push_back(branch_backup);
					
				}else{
					decodeVar.stalled=true;
				}
			}
			else if(decodeVar.typeInstruc== Simulator::instructs::BZ ||
				decodeVar.typeInstruc== Simulator::instructs::BNZ){
				int IQIndex=-1;

				//See if there is free IQ entry
				for(int i=0;i<8;i++){
					if(arrayIQ[i].iqEntryFree){ //if iq entry free use this as new IQ entry
						IQIndex=i;
						break;
					}
				}

				if(IQIndex!=-1){
					decodeVar.PRSrc1Ix=lastInstrucPrIx;

					decodeVar.lit= stoi(parts[1]);

					decodeVar.IQIx=IQIndex;
					decodeVar.ROBIx=ROBIx;

					createIQROBEntry(IQIndex, ROBIx, -1, -1);


					int k=0;
					PredictionBackup branch_backup;
					branch_backup.currentPC = PC;
					branch_backup.branchPC= decodeVar.lit + decodeVar.instrucLine;
					for (k=0; k<9; k++){
						branch_backup.renTable[k] = renameTable[k];
					}
					for (k=0; k<16; k++){
						branch_backup.PRValid [k] = physRegsValid[k];
					}
					for (k=0; k<16; k++){
						branch_backup.PRAlloc [k] = physRegsAlloc[k];
					}
					for (k=0; k<16; k++){
						branch_backup.renamed[k] = renamed[k];
					}
					//copy last instruction physical destination register that modified the zflag
					branch_backup.lastPixZflag=lastInstrucPrIx;
					predictBackupArr.push_back(branch_backup);

					//flush only if prediction is taken
					if (decodeVar.lit < 0){

						//flush only fetch1 and fetch2
						PC = decodeVar.lit + decodeVar.instrucLine;
						emptyFetch1(fetch1Var);
						emptyFetch1(fetch1VarTemp);
						emptyFetch2(fetch2Var);
						emptyFetch2(fetch2VarTemp);

						//no necessary to put stopped by halt =false since this is done in this same stage
					}

				}else{
					decodeVar.stalled=true;
				}
			}
			else if(decodeVar.typeInstruc== Simulator::instructs::HALT){
				int IQIndex=-1;

				//See if there is free IQ entry
				for(int i=0;i<8;i++){
					if(arrayIQ[i].iqEntryFree){ //if iq entry free use this as new IQ entry
						IQIndex=i;
						break;
					}
				}

				if(IQIndex!=-1){
					decodeVar.IQIx=IQIndex;
					decodeVar.ROBIx=ROBIx;

					createIQROBEntry(IQIndex, ROBIx, -1, -1);

				}else{
					decodeVar.stalled=true;
				}

				stoppedbyHalt=true; //stops fetching instructions
			}

			//only increment the tail if the decode is not going to be stalled there because in such a case
			//the instruction should get the same rop index not a next one
			if(!decodeVar.stalled)
				tailIx=(tailIx+1)%17;
		}else{ //if there is no ROB entry available it is not necessary to do anything else
			decodeVar.stalled=true;
		}
	}
	

	//check if physical register can be deallocated
	for(int i=0;i<16;i++){
		if(physRegsValid[i] && renamed[i] && decodeVar.PRSrc1Ix!=i && decodeVar.PRSrc2Ix!=i && decodeVar.PRSrc3Ix!=i)
			physRegsAlloc[i]=false;
	}
	
}

void Simulator::decode2Stage(){
	emptyDecode2(decDispatchVar);


	decDispatchVar.instrucLine=decodeVarTemp.instrucLine;
	decDispatchVar.instruction=decodeVarTemp.instruction;

	if(decodeVarTemp.instrucLine!=-1){
		if(decodeVarTemp.typeInstruc== Simulator::instructs::ADD || decodeVarTemp.typeInstruc== Simulator::instructs::SUB ||
			decodeVarTemp.typeInstruc== Simulator::instructs::MUL || decodeVarTemp.typeInstruc== Simulator::instructs::AND ||
			decodeVarTemp.typeInstruc== Simulator::instructs::OR || decodeVarTemp.typeInstruc== Simulator::instructs::EXOR){
				
				if (!sourceInUse(decodeVarTemp.PRSrc1Ix)){
					arrayIQ[decodeVarTemp.IQIx].src1Value = physRegs[decodeVarTemp.PRSrc1Ix];
					arrayIQ[decodeVarTemp.IQIx].src1Valid = true;
				}else{
					arrayIQ[decodeVarTemp.IQIx].src1Tag = decodeVarTemp.PRSrc1Ix;
					arrayIQ[decodeVarTemp.IQIx].src1Valid = false;
				}

				if (!sourceInUse(decodeVarTemp.PRSrc2Ix)){
					arrayIQ[decodeVarTemp.IQIx].src2Value = physRegs[decodeVarTemp.PRSrc2Ix];
					arrayIQ[decodeVarTemp.IQIx].src2Valid = true;
				}else
				{
					arrayIQ[decodeVarTemp.IQIx].src2Tag = decodeVarTemp.PRSrc2Ix;
					arrayIQ[decodeVarTemp.IQIx].src2Valid = false;
				}


		}//end of INT two operands


		else if(decodeVarTemp.typeInstruc== Simulator::instructs::LOAD){
			
			int lsqIndex=-1;

			for(int i=0;i<(int)LSQ.size();i++){
				if(LSQ[i].ROBIx==decodeVarTemp.ROBIx){
					lsqIndex=i;
				}
			}

			if (!sourceInUse(decodeVarTemp.PRSrc1Ix)){
				LSQ[lsqIndex].src1Value = physRegs[decodeVarTemp.PRSrc1Ix];
				LSQ[lsqIndex].src1Valid = true;
			}else
			{
				LSQ[lsqIndex].src1Tag = decodeVarTemp.PRSrc1Ix;
				LSQ[lsqIndex].src1Valid = false;
			}

			if(decodeVarTemp.PRSrc2Ix != -1){
				if (!sourceInUse(decodeVarTemp.PRSrc2Ix)){
					LSQ[lsqIndex].src2Value = physRegs[decodeVarTemp.PRSrc2Ix];
					LSQ[lsqIndex].src2Valid = true;
				}else{
					LSQ[lsqIndex].src2Tag = decodeVarTemp.PRSrc2Ix;
					LSQ[lsqIndex].src2Valid = false;
				}
			}else{
				LSQ[lsqIndex].src2Value = decodeVarTemp.lit;
				LSQ[lsqIndex].src2Valid = true;
			}

		} //end of LOAD
		else if(decodeVarTemp.typeInstruc== Simulator::instructs::STORE){
			int lsqIndex=-1;

			for(int i=0;i<(int)LSQ.size();i++){
				if(LSQ[i].ROBIx==decodeVarTemp.ROBIx){
					lsqIndex=i;
				}
			}
			
			if (!sourceInUse(decodeVarTemp.PRSrc1Ix)){
				LSQ[lsqIndex].src1Value = physRegs[decodeVarTemp.PRSrc1Ix];
				LSQ[lsqIndex].src1Valid = true;
			}
			else
			{
				LSQ[lsqIndex].src1Tag = decodeVarTemp.PRSrc1Ix;
				LSQ[lsqIndex].src1Valid = false;
			}
			if (!sourceInUse(decodeVarTemp.PRSrc2Ix))
			{
				LSQ[lsqIndex].src2Value = physRegs[decodeVarTemp.PRSrc2Ix];
				LSQ[lsqIndex].src2Valid = true;
			}
			else
			{
				LSQ[lsqIndex].src2Tag = decodeVarTemp.PRSrc2Ix;
				LSQ[lsqIndex].src2Valid = false;
			}
			if(LSQ[lsqIndex].src3Tag != -1){
				if (!sourceInUse(decodeVarTemp.PRSrc3Ix)){
					LSQ[lsqIndex].src3Value = physRegs[decodeVarTemp.PRSrc3Ix];
					LSQ[lsqIndex].src3Valid = true;
				}
				else{
					LSQ[lsqIndex].src3Tag = decodeVarTemp.PRSrc3Ix;
					LSQ[lsqIndex].src3Valid = false;
				}
			}
			else{
				LSQ[lsqIndex].src3Value = decodeVarTemp.lit;
				LSQ[lsqIndex].src3Valid = true;
			}
		}//end of STORE

		else if(decodeVarTemp.typeInstruc== Simulator::instructs::MOVC){

			arrayIQ[decodeVarTemp.IQIx].src1Value = decodeVarTemp.lit;
			arrayIQ[decodeVarTemp.IQIx].src1Valid = true;
		}//end of MOVC

		else if(decodeVarTemp.typeInstruc== Simulator::instructs::MOV){
			if (!sourceInUse(decodeVarTemp.PRSrc1Ix))
			{
				arrayIQ[decodeVarTemp.IQIx].src1Value = physRegs[decodeVarTemp.PRSrc1Ix];
				arrayIQ[decodeVarTemp.IQIx].src1Valid = true;
			}else{
				arrayIQ[decodeVarTemp.IQIx].src1Tag = decodeVarTemp.PRSrc1Ix;
				arrayIQ[decodeVarTemp.IQIx].src1Valid = false;
			}

		}//end of MOV
		else if(decodeVarTemp.typeInstruc== Simulator::instructs::BAL)
		{

			//see if source in use
			if(sourceInUse(decodeVarTemp.PRSrc1Ix)){
				arrayIQ[decodeVarTemp.IQIx].src1Tag = decodeVarTemp.PRSrc1Ix;
				arrayIQ[decodeVarTemp.IQIx].src1Valid = false;
			}
			else
			{
				arrayIQ[decodeVarTemp.IQIx].src1Value = physRegs[decodeVarTemp.PRSrc1Ix];
				arrayIQ[decodeVarTemp.IQIx].src1Valid = true;
			}

			arrayIQ[decodeVarTemp.IQIx].src2Value = decodeVarTemp.lit;
			arrayIQ[decodeVarTemp.IQIx].src2Valid = true;

		}//end of BALL
		else if(decodeVarTemp.typeInstruc== Simulator::instructs::JUMP)
		{
			//see if source in use
			if(sourceInUse(decodeVarTemp.PRSrc1Ix)){
				arrayIQ[decodeVarTemp.IQIx].src1Tag = decodeVarTemp.PRSrc1Ix;
				arrayIQ[decodeVarTemp.IQIx].src1Valid = false;
			}
			else{
				arrayIQ[decodeVarTemp.IQIx].src1Value = physRegs[decodeVarTemp.PRSrc1Ix];
				arrayIQ[decodeVarTemp.IQIx].src1Valid = true;
			}

			arrayIQ[decodeVarTemp.IQIx].src2Value = decodeVarTemp.lit;
			arrayIQ[decodeVarTemp.IQIx].src2Valid = true;


		}//end of JUMP
		else if((decodeVarTemp.typeInstruc== Simulator::instructs::BZ || decodeVarTemp.typeInstruc== Simulator::instructs::BNZ)){


			//see if source in use
			if(sourceInUse(decodeVarTemp.PRSrc1Ix)){
				arrayIQ[decodeVarTemp.IQIx].src1Tag = decodeVarTemp.PRSrc1Ix;
				arrayIQ[decodeVarTemp.IQIx].src1Valid = false;
			}else{
				arrayIQ[decodeVarTemp.IQIx].src1Value = physRegs[decodeVarTemp.PRSrc1Ix]; 
				//or 1 otherwise
				arrayIQ[decodeVarTemp.IQIx].src1Valid = true;
			}

			arrayIQ[decodeVarTemp.IQIx].src2Value = decodeVarTemp.lit;
			arrayIQ[decodeVarTemp.IQIx].src2Valid = true;
		}
	}
}//end if instructline!=-1

void Simulator::flushPipeline(int robIx){
	int robTemp=(robIx+1)%17;

	emptyFetch1(fetch1Var);
	emptyFetch1(fetch1VarTemp);
	emptyFetch2(fetch2Var);
	emptyFetch2(fetch2VarTemp);
	emptyDecode(decodeVar);
	emptyDecode(decodeVarTemp);
	emptyDecode2(decDispatchVar);

	std::vector<int> listPrsToInvalidate;


	for(int i=robTemp;i<=tailIx;i=(i+1)%17){
		for(int j=0;j<8;j++){
			if(arrayIQ[j].ROBIx==robTemp){
				emptyIQ(arrayIQ[j]);
				break;
			}
		}

		for (int j=0;j<(int)LSQ.size();j++){
			if(LSQ[j].ROBIx==robTemp){
				LSQ.erase (LSQ.begin()+j);
				break;
			}
		}

		listPrsToInvalidate.push_back(arrayROB[robTemp].prIx);
		for(int j=0;j<8;j++){
			if(arrayIQ[j].ROBIx==i){
				emptyIQ(arrayIQ[j]);
			}
		}
	}

	if(predictBackupArr.size()>0){
		for(int i=0;i<9;i++){
			renameTable[i]=predictBackupArr.at(0).renTable[i];
		}

		for(int i=0;i<16;i++){
			renamed[i]=predictBackupArr.at(0).renamed[i];
			physRegsAlloc[i]=predictBackupArr.at(0).PRAlloc[i];
		}

		for(int i=0;i<(int)listPrsToInvalidate.size();i++){
			physRegsValid[listPrsToInvalidate.at(i)]=predictBackupArr.at(0).PRValid[listPrsToInvalidate.at(i)];
		}
		lastInstrucPrIx=predictBackupArr.at(0).lastPixZflag;
	}

	stoppedbyHalt=false;

	tailIx=(robIx+1)%17; //make the tail equal to the rob of the branch +1 %17
}

bool Simulator::allSourcesAvailable(IQ iqEntry){
	if(iqEntry.src1Valid && iqEntry.src2Valid && iqEntry.src3Valid){
		return true;
	}
	return false;
}

void Simulator::executeStage(){
	
	////// GISSELLA begin 11-12-15
	bool firstTime=true;
	int typeFUToForward = -1;
	int phyDestIxInt = -1;
	int phyDestIxMul = -1;
	int phyDestIxMem = -1;
	int iqEntry = 0;

	///// ****** INT FU -  As it is one cycle-long I can always pick a new IQ entry ********/////
	// select IQ entry
	int olderIndex=-1;
	int olderTime=0;
	Simulator::instructs oldestInsType;

	//empty the INT FU can do so right away because its one cycle long
	emptyINTFU(INTFUStr);
	for (iqEntry = 0; iqEntry < 8 ; iqEntry++){

		Simulator::instructs instrucTypeIq = arrayIQ[iqEntry].instrucType;
		if  (instrucTypeIq== Simulator::instructs::ADD || instrucTypeIq== Simulator::instructs::SUB  ||
			instrucTypeIq== Simulator::instructs::AND || instrucTypeIq== Simulator::instructs::BZ ||
			instrucTypeIq== Simulator::instructs::BNZ || instrucTypeIq== Simulator::instructs::JUMP ||
			instrucTypeIq== Simulator::instructs::BAL ||
			instrucTypeIq== Simulator::instructs::OR   || instrucTypeIq== Simulator::instructs::EXOR ||
			instrucTypeIq== Simulator::instructs::MOV || instrucTypeIq== Simulator::instructs::MOVC ||
			instrucTypeIq== Simulator::instructs::HALT) {

			// if all the sources are valid (by default an unused source is valid AND the counter is 1
			if (arrayIQ[iqEntry].src1Valid && arrayIQ[iqEntry].src2Valid && arrayIQ[iqEntry].src3Valid) {

				if (arrayIQ[iqEntry].counter){
					if(firstTime){
						firstTime=false;
						olderIndex = iqEntry;
						olderTime = arrayIQ[iqEntry].timeAdded;
					}else{
						if (arrayIQ[iqEntry].timeAdded < olderTime){ // Trying to find the oldest INT instruction
							olderIndex = iqEntry;
							olderTime = arrayIQ[iqEntry].timeAdded;
						}
					}
				} else {
					arrayIQ[iqEntry].counter=true;
				}
			}
		}
	}

	if (olderIndex!=-1) { // There was a ready instruction, so execute it according to its type
			oldestInsType = arrayIQ[olderIndex].instrucType;
			INTFUStr.ROBIx = arrayIQ[olderIndex].ROBIx;
			INTFUStr.instrucLine=arrayROB[arrayIQ[olderIndex].ROBIx].instrucLine;
			switch(oldestInsType){
				case Simulator::instructs::ADD:
					INTFUStr.result = arrayIQ[olderIndex].src1Value + arrayIQ[olderIndex].src2Value;
					if (INTFUStr.result == 0)
						INTFUStr.zFlag = 0;
					else 
						INTFUStr.zFlag = 1;
					break;
				case Simulator::instructs::SUB:
					INTFUStr.result = arrayIQ[olderIndex].src1Value - arrayIQ[olderIndex].src2Value;
					if (INTFUStr.result == 0)
						INTFUStr.zFlag = 0;
					else INTFUStr.zFlag = 1;
					break;
				case Simulator::instructs::MOVC:
					INTFUStr.result = arrayIQ[olderIndex].src1Value; //literal
					break;
				case Simulator::instructs::MOV:
					INTFUStr.result = arrayIQ[olderIndex].src1Value;
					break;
				case Simulator::instructs::AND:
					INTFUStr.result = arrayIQ[olderIndex].src1Value & arrayIQ[olderIndex].src2Value;
					if (INTFUStr.result == 0)
						INTFUStr.zFlag = 0;
					else INTFUStr.zFlag = 1;
					break;
				case Simulator::instructs::OR:
					INTFUStr.result = arrayIQ[olderIndex].src1Value | arrayIQ[olderIndex].src2Value;
					if (INTFUStr.result == 0)
						INTFUStr.zFlag = 0;
					else INTFUStr.zFlag = 1;
					break;
				case Simulator::instructs::EXOR:
					INTFUStr.result = arrayIQ[olderIndex].src1Value ^ arrayIQ[olderIndex].src2Value;
					if (INTFUStr.result == 0)
						INTFUStr.zFlag = 0;
					else INTFUStr.zFlag = 1;
					break;
				case Simulator::instructs::BZ:
					if(arrayIQ[olderIndex].src1Value==0){ //ZFLAG
					// If the branche must be taken (src ==0 ) but the prediction was to be not taken
						if (arrayIQ[olderIndex].src2Value >= 0) {
							PC=INTFUStr.instrucLine + arrayIQ[olderIndex].src2Value; //literal
							flushPipeline(INTFUStr.ROBIx); // review
							predictBackupArr.clear();
						}else{
							//if prediction was right then just get this branch outta the predict vector
							predictBackupArr.pop_front();
						}
					}
					else {
						if (arrayIQ[olderIndex].src2Value < 0) {
							PC=INTFUStr.instrucLine + 1; //change the pc address to the instruction following the branch
							flushPipeline(INTFUStr.ROBIx); // review
							predictBackupArr.clear();
						}else{
							//if prediction was right then just get this branch outta the predict vector
							predictBackupArr.pop_front();
						}
					}
					break;
				case Simulator::instructs::BNZ:
					if(arrayIQ[olderIndex].src1Value!=0){ //ZFLAG
						//bnz means branch if not zero, so if the prediction was not to branch
						//flush pipeline
						if (arrayIQ[olderIndex].src2Value >= 0) {
							PC=INTFUStr.instrucLine + arrayIQ[olderIndex].src2Value; //literal
							flushPipeline(INTFUStr.ROBIx);
							predictBackupArr.clear();
						}else{
							//if prediction was right then just get this branch outta the predict vector
							predictBackupArr.pop_front();
						}
					} else {//if the branch is not taken but the prediction was taken
						//flush pipeline and make the new instruction address equal to the branch address +1
						if (arrayIQ[olderIndex].src2Value < 0) {
							//go back to whatever is after the branch
							PC=INTFUStr.instrucLine + 1; //literal
							flushPipeline(INTFUStr.ROBIx);
							predictBackupArr.clear();
						}else{
							//if prediction was right then just get this branch outta the predict vector
							predictBackupArr.pop_front();
						}
					}
					break;
				case Simulator::instructs::JUMP:
					PC=arrayIQ[olderIndex].src1Value + arrayIQ[olderIndex].src2Value; //literal
					flushPipeline(INTFUStr.ROBIx);
					predictBackupArr.clear();
					break;
				case Simulator::instructs::BAL:
					INTFUStr.result=INTFUStr.instrucLine+1; // Register R8 or X register
					PC=arrayIQ[olderIndex].src1Value + arrayIQ[olderIndex].src2Value;
					flushPipeline(INTFUStr.ROBIx);
					predictBackupArr.clear();
					break;
				case Simulator::instructs::HALT:
					stoppedbyHalt=true;
					break;
				default:
					break;
			}
			
			// Sending the result to ROB
			phyDestIxInt = arrayROB[INTFUStr.ROBIx].prIx; //Before, assignt to ROBIx the robIx of the FU
			if(phyDestIxInt!=-1){
				physRegs[phyDestIxInt] = INTFUStr.result; //grap from Int FU
				physRegsValid[phyDestIxInt] = true;
				typeFUToForward = 0;
			}
			arrayROB[INTFUStr.ROBIx].whereS = 5;
			emptyIQ(arrayIQ[olderIndex]);

			arrayROB[INTFUStr.ROBIx].ready=true;
	} else {// if there was not any ready instrution
		INTFUStr.result= -1;
		INTFUStr.zFlag = -1;
		INTFUStr.instrucType = Simulator::instructs::NOTFOUND;
	}
	////// ****** MUL FU ******///////////
	olderIndex = -1;
	olderTime = 0;
	firstTime=true;
	if (MULFUStr.mulCounter ==0) { // MUL FU is ready to receive another instruction
		//if counter = 0 can delete what was previously in the mul fu
		emptyMULFU(MULFUStr);
		for (iqEntry = 0; iqEntry < 8 ; iqEntry++){
			if(!arrayIQ[iqEntry].iqEntryFree){
				Simulator::instructs instrucTypeIq = arrayIQ[iqEntry].instrucType;
				// if all the sources are valid (by default an unused source is valid
				if   ( instrucTypeIq== Simulator::instructs::MUL ) {
					if (arrayIQ[iqEntry].src1Valid && arrayIQ[iqEntry].src2Valid && arrayIQ[iqEntry].src3Valid) {
						if (arrayIQ[iqEntry].counter){
							if(firstTime){
								firstTime=false;
								olderIndex = iqEntry;
								olderTime = arrayIQ[iqEntry].timeAdded;
							}else{
								cout << "Time Added: " << arrayIQ[iqEntry].timeAdded << " olderTime: " << olderTime;
								if (arrayIQ[iqEntry].timeAdded < olderTime){ // Trying to find the oldest INT instruction
									olderIndex = iqEntry;
									olderTime = arrayIQ[iqEntry].timeAdded;
								}
							}
						}//end if counter equal to true
						else{
							arrayIQ[iqEntry].counter=true;
						}
					}
				}
			}
		}
		if (olderIndex !=-1){
			
			//oldestInsType = arrayIQ[olderIndex].instrucType;
			MULFUStr.result = arrayIQ[olderIndex].src1Value * arrayIQ[olderIndex].src2Value;
			MULFUStr.ROBIx = arrayIQ[olderIndex].ROBIx;
			arrayROB[MULFUStr.ROBIx].whereS = 5;
			emptyIQ(arrayIQ[olderIndex]);
			if (MULFUStr.result == 0){
				MULFUStr.zFlag = 0;
			}
			else {
				MULFUStr.zFlag = 1;
			}

			MULFUStr.mulCounter = MULFUStr.mulCounter +1;
		}
	} else { // MUL FU already has to go thoguh 4 cycles (0-3) //end if Mulcounter=0
		switch (MULFUStr.mulCounter){
			case 1:
				MULFUStr.mulCounter+=1;
				break;
			case 2:
				MULFUStr.mulCounter+=1;
				break;
			case 3:
				
				MULFUStr.mulCounter =0;
				typeFUToForward = 1;
				phyDestIxMul = arrayROB[MULFUStr.ROBIx].prIx;
				physRegs[phyDestIxMul] = MULFUStr.result; //grap from MUL FU
				physRegsValid[phyDestIxMul] = true;
				arrayROB[MULFUStr.ROBIx].ready=true;

				break;
			default:
				break;
		}
	}

	//empty whatever was in the third cycle of the mem
	emptyMEMFU(MEMFUCyc3);

	////// ******  MEM FU ******///////////
	// Pass values from second MEM stage to third MEM stage update Physical Registers
	// Move from two to three
	switch (MEMFUCyc2.instrucType){
			case Simulator::instructs::NOTFOUND:
				break;
			case Simulator::instructs::LOAD:
				MEMFUCyc3=MEMFUCyc2; //move from two to three
				MEMFUCyc3.result = memory[MEMFUCyc2.address];
				phyDestIxMem = arrayROB[MEMFUCyc2.ROBIx].prIx;
				physRegs[phyDestIxMem] = memory[MEMFUCyc2.address];
				physRegsValid[phyDestIxMem] = true;
				typeFUToForward = 2;
				arrayROB[MEMFUCyc2.ROBIx].ready=true;
				emptyMEMFU(MEMFUCyc2); //empty second cycle of mem fu after moving to three
				break;
			case Simulator::instructs::STORE:
				MEMFUCyc3=MEMFUCyc2; //move from two to three
				memory[MEMFUCyc2.address] = MEMFUCyc2.result;
				
				arrayROB[MEMFUCyc2.ROBIx].ready=true;
				emptyMEMFU(MEMFUCyc2); //empty second cycle of mem fu
				break;
			default: break;
	}
	// Pass values from first MEM stage to second MEM stage
	if (MEMFUCyc1.instrucType!=NOTFOUND){
		emptyMEMFU(MEMFUCyc2); //move from one to two
		MEMFUCyc2.address = MEMFUCyc1.address;
		MEMFUCyc2.result = MEMFUCyc1.result;
		MEMFUCyc2.instrucType = MEMFUCyc1.instrucType;
		MEMFUCyc2.ROBIx = MEMFUCyc1.ROBIx;
		emptyMEMFU(MEMFUCyc1);
	}
	//empty whats in one that should have been moved to two
	emptyMEMFU(MEMFUCyc1);

	// Select new LSQ entry for MEMFUCyc1
	if ( !LSQ.empty() ){
		IQ newMemIns = LSQ.front();
		if (newMemIns.src1Valid && newMemIns.src2Valid && newMemIns.src3Valid){
			if (newMemIns.counter){
				switch (newMemIns.instrucType){
						case Simulator::instructs::LOAD:
							MEMFUCyc1.address=newMemIns.src1Value+newMemIns.src2Value;
							MEMFUCyc1.ROBIx = newMemIns.ROBIx;
							MEMFUCyc1.instrucType=LOAD;
							LSQ.pop_front();
							break;
						case Simulator::instructs::STORE:
							//if the index of the head corresponds to the rob index of the store
							if (headIx == newMemIns.ROBIx){
								MEMFUCyc1.result = newMemIns.src1Value;
								MEMFUCyc1.address=newMemIns.src2Value+newMemIns.src3Value;
								MEMFUCyc1.ROBIx = newMemIns.ROBIx;
								arrayROB[MEMFUCyc1.ROBIx].whereS = 5;
								MEMFUCyc1.instrucType=STORE;
								LSQ.pop_front();
							} else {
								//if instruction is store and at top of LSQ but not at head of ROB
								MEMFUCyc1.instrucType = NOTFOUND;
							}
							break;
						 default: break;
				}//end switch
			} else {
				LSQ[0].counter=true;
				MEMFUCyc1.instrucType = NOTFOUND; //if counter not true
			}
		} else {
			MEMFUCyc1.instrucType = NOTFOUND; //if all sources are not valid
		}
	} else {
		MEMFUCyc1.instrucType = NOTFOUND; //if LSQ not empty
	}
	// Forward, Free iqentry / lsqentry and set counter to 1 to the ones that were not forwarded
	int finalResult=-1;
	int phyDestIx=-1;

	switch (typeFUToForward){
		case 0:
			finalResult = INTFUStr.result;
			phyDestIx = phyDestIxInt;
			break;
		case 1:
			finalResult = MULFUStr.result;
			phyDestIx = phyDestIxMul;
			break;
		case 2:
			finalResult = MEMFUCyc3.result;
			phyDestIx = phyDestIxMem;
			break;
		default:
			break;
	}

	if(typeFUToForward!=-1){
		for (iqEntry = 0; iqEntry < 8 ; iqEntry++){
			//only forward to the iq entries that have something inside
			if(!arrayIQ[iqEntry].iqEntryFree){
				//update instructionForward for the complete EX instructions
				if (arrayIQ[iqEntry].src1Tag == phyDestIxInt && arrayIQ[iqEntry].src1Tag!=-1){
					arrayIQ[iqEntry].src1Value = INTFUStr.result;
					arrayIQ[iqEntry].src1Valid = true;
					arrayIQ[iqEntry].src1Tag=-1; //change the tag so that the value is not forwarded again
					//only put the counter as true if this value is the last source needed
					if (typeFUToForward == 0 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
				if (arrayIQ[iqEntry].src1Tag == phyDestIxMul && arrayIQ[iqEntry].src1Tag!=-1){
					arrayIQ[iqEntry].src1Value = MULFUStr.result;
					arrayIQ[iqEntry].src1Valid = true;
					arrayIQ[iqEntry].src1Tag=-1; //change the tag so that the value is not forwarded again
					if (typeFUToForward == 1 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
				if (arrayIQ[iqEntry].src1Tag == phyDestIxMem && arrayIQ[iqEntry].src1Tag!=-1){
					arrayIQ[iqEntry].src1Value = MEMFUCyc3.result;
					arrayIQ[iqEntry].src1Valid = true;
					arrayIQ[iqEntry].src1Tag=-1; //change the tag so that the value is not forwarded again
					if (typeFUToForward == 2 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
				if (arrayIQ[iqEntry].src2Tag == phyDestIxInt && arrayIQ[iqEntry].src2Tag!=-1){
					arrayIQ[iqEntry].src2Value = INTFUStr.result;
					arrayIQ[iqEntry].src2Valid = true;
					arrayIQ[iqEntry].src2Tag=-1; //change the tag so that the value is not forwarded again
					if (typeFUToForward == 0 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
				if (arrayIQ[iqEntry].src2Tag == phyDestIxMul && arrayIQ[iqEntry].src2Tag!=-1){
					arrayIQ[iqEntry].src2Value = MULFUStr.result;
					arrayIQ[iqEntry].src2Valid = true;
					arrayIQ[iqEntry].src2Tag=-1; //change the tag so that the value is not forwarded again
					if (typeFUToForward == 1 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
				if (arrayIQ[iqEntry].src2Tag == phyDestIxMem && arrayIQ[iqEntry].src2Tag!=-1){
					arrayIQ[iqEntry].src2Value = MEMFUCyc3.result;
					arrayIQ[iqEntry].src2Valid = true;
					arrayIQ[iqEntry].src2Tag=-1; //change the tag so that the value is not forwarded again
					if (typeFUToForward == 2 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
				if (arrayIQ[iqEntry].src3Tag == phyDestIxInt && arrayIQ[iqEntry].src3Tag!=-1){
					arrayIQ[iqEntry].src3Value = INTFUStr.result;
					arrayIQ[iqEntry].src3Valid = true;
					arrayIQ[iqEntry].src3Tag=-1; //change the tag so that the value is not forwarded again
					if (typeFUToForward == 0 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
				if (arrayIQ[iqEntry].src3Tag == phyDestIxMul && arrayIQ[iqEntry].src3Tag!=-1){
					arrayIQ[iqEntry].src3Value = MULFUStr.result;
					arrayIQ[iqEntry].src3Valid = true;
					arrayIQ[iqEntry].src3Tag=-1; //change the tag so that the value is not forwarded again
					if (typeFUToForward == 1 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
				if (arrayIQ[iqEntry].src3Tag == phyDestIxMem && arrayIQ[iqEntry].src3Tag!=-1){
					arrayIQ[iqEntry].src3Value = MEMFUCyc3.result;
					arrayIQ[iqEntry].src3Valid = true;
					arrayIQ[iqEntry].src3Tag=-1; //change the tag so that the value is not forwarded again
					if (typeFUToForward == 2 && allSourcesAvailable(arrayIQ[iqEntry]))
						arrayIQ[iqEntry].counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
				}
			}
    	};

		////FORWARD TO THE LSQ
		deque<IQ> newLSQ;
		while (!LSQ.empty())
		{
			IQ nextEntry = LSQ.front();
			LSQ.pop_front();
			if (nextEntry.src1Tag == phyDestIxInt && nextEntry.src1Tag!=-1){
				nextEntry.src1Value = INTFUStr.result;
				nextEntry.src1Valid = true;
				nextEntry.src1Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 0 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			if (nextEntry.src1Tag == phyDestIxMul && nextEntry.src1Tag!=-1){
				nextEntry.src1Value = MULFUStr.result;
				nextEntry.src1Valid = true;
				nextEntry.src1Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 1 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			if (nextEntry.src1Tag == phyDestIxMem && nextEntry.src1Tag!=-1){
				nextEntry.src1Value = MEMFUCyc3.result;
				nextEntry.src1Valid = true;
				nextEntry.src1Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 2 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			if (nextEntry.src2Tag == phyDestIxInt && nextEntry.src2Tag!=-1){
				nextEntry.src2Value = INTFUStr.result;
				nextEntry.src2Valid = true;
				nextEntry.src2Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 0 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			if (nextEntry.src2Tag == phyDestIxMul && nextEntry.src2Tag!=-1){
				nextEntry.src2Value = MULFUStr.result;
				nextEntry.src2Valid = true;
				nextEntry.src2Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 1 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			if (nextEntry.src2Tag == phyDestIxMem && nextEntry.src2Tag!=-1){
				nextEntry.src2Value = MEMFUCyc3.result;
				nextEntry.src2Valid = true;
				nextEntry.src2Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 2 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			if (nextEntry.src3Tag == phyDestIxInt && nextEntry.src3Tag!=-1){
				nextEntry.src3Value = INTFUStr.result;
				nextEntry.src3Valid = true;
				nextEntry.src3Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 0 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			if (nextEntry.src3Tag == phyDestIxMul && nextEntry.src3Tag!=-1){
				nextEntry.src3Value = MULFUStr.result;
				nextEntry.src3Valid = true;
				nextEntry.src3Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 1 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			if (nextEntry.src3Tag == phyDestIxMem && nextEntry.src3Tag!=-1){
				nextEntry.src3Value = MEMFUCyc3.result;
				nextEntry.src3Valid = true;
				nextEntry.src3Tag =-1; //change the tag to -1 so the value is not forwarded again
				if (typeFUToForward == 2 && allSourcesAvailable(nextEntry))
					nextEntry.counter = true; // When EX picks from IQ it also review all the sources are valid besides that the counter = 1
			}
			newLSQ.push_back(nextEntry);
		}
		
		//pass from temporary queue to the normal one
		while(!newLSQ.empty()){
			IQ nextEntry = newLSQ.front();
			LSQ.push_back(nextEntry);
			newLSQ.pop_front();
		}		
	}
}

//////////// GISSELLA begin 11-12-15
//writes to the registers
int Simulator::writeBackStage(){
	if(arrayROB[headIx].instrucType==Simulator::instructs::HALT && arrayROB[headIx].ready){
		return 1;
	}

	int prIxResult = arrayROB[headIx].prIx;
	
	if(arrayROB[headIx].ready){
		if(prIxResult!=-1){
			if (physRegsValid[prIxResult]){
				regs[arrayROB[headIx].archIX] = physRegs[prIxResult];
			}
		}
		emptyROB(arrayROB[headIx]); // Clean the whole entry
		headIx = ((headIx + 1)%17);
	}
	return 0;
}

/////// GISSELLA end 11-12-15

void Simulator::emptyROB(ROB &rob){
	rob.whereS=-1;
	rob.archIX=-1;
	rob.prIx=-1;
	rob.instrucType=Simulator::instructs::NOTFOUND;
	rob.instrucLine=-1;
	rob.instruction="";
	rob.ready=false;
}

void Simulator::emptyIQ(IQ &iq){
	iq.src1Tag=-1;
	iq.src1Value=-1;
	iq.src1Valid=true;
	iq.src2Tag=-1;
	iq.src2Value=-1;
	iq.src2Valid=true;
	iq.src3Tag=-1;
	iq.src3Value=-1;
	iq.src3Valid=true;
	iq.iqEntryFree=true;
	iq.counter=false;
	iq.instrucType=Simulator::instructs::NOTFOUND;
	iq.ROBIx=-1;
	iq.instrucLine=-1;
	iq.instruction="";
	iq.timeAdded=-1;
}

void Simulator::emptyINTFU(INTFU &inFU){
	inFU.src1Value=-1;
	inFU.src2Value=-1;
	inFU.instrucType=Simulator::instructs::NOTFOUND;
	inFU.ROBIx=-1;
	inFU.result=-1;
	inFU.zFlag=-1;
	inFU.instrucLine=-1;
}

void Simulator::emptyMEMFU(MEMFU &memFU){
	memFU.src1Value=-1;
	memFU.src2Value=-1;
	memFU.src3Value=-1;
	memFU.instrucType=Simulator::instructs::NOTFOUND;
	memFU.ROBIx=-1;
	memFU.result=-1;
	memFU.address=-1;
}

void Simulator::emptyMULFU(MULFU &mulFU){
	mulFU.mulCounter=0;
	mulFU.src1Value=-1;
	mulFU.src2Value=-1;
	mulFU.instrucType=Simulator::instructs::NOTFOUND;;
	mulFU.ROBIx=-1;
	mulFU.result=-1;
	mulFU.zFlag=-1;
}

void Simulator::emptyFetch1(fetch1Str &fet){
	fet.instrucLine=-1;
	fet.instruction="";
}

void Simulator::emptyFetch2(fetch2Str &fet){
	fet.instrucLine=-1;
	fet.instruction="";
}

void Simulator::emptyDecode(decRenameStr &decod){
	decod.stalled=false;
	decod.typeInstruc=Simulator::instructs::NOTFOUND;
	decod.instruction="";
	decod.instrucLine=-1;
	decod.PRDestIx=-1;
	decod.IQIx=-1;
	decod.ROBIx=-1;
	decod.PRSrc1Ix=-1;
	decod.PRSrc2Ix=-1;
	decod.PRSrc3Ix=-1;
}

void Simulator::emptyDecode2(decDispatchStr &decod){
	decod.instrucLine=-1;
	decod.instruction="";
}

int Simulator::simulate(int nCycles){

	bool retur=false;
	//simulate til N cycles or halt stops execution (==1)
	for(int i=0;i<nCycles;i++){
		if(writeBackStage()==1){
			cout << "Last Cycle: "<<i+1 <<" \n";
			retur= true;
		}
		fetch1Stage();
		fetch2Stage();
		decode1Stage();
		decode2Stage();
		executeStage();

		if(retur)
			return 1;
	}

	return 0;
}

void Simulator::displayStages(){

	cout << "\nFetch1 Stage:" << endl;
	cout << "Address: "<<fetch1Var.instrucLine << " Instruction: "<< fetch1Var.instruction << endl;
	cout << "\nFetch2 Stage:" << endl;
	cout << "Address: "<<fetch2Var.instrucLine << " Instruction: "<< fetch2Var.instruction << endl;
	cout << "Decode/Rename Stage:" << endl;
	cout << "Address: "<<decodeVar.instrucLine << " Instruction: "<< decodeVar.instruction << endl;
	cout << "Rename/Dispatch Stage:" << endl;
	cout << "Address: "<<decDispatchVar.instrucLine << " Instruction: "<< decDispatchVar.instruction << endl;
}

void Simulator::display(){

	//display instruction in the stages and the first 100 memory locations
	displayStages();
	cout << "\nRegisters:\n";
	for(int i=0;i<9;i++)
		cout << i <<": "<< regs[i] << endl;

	cout << "Memory Locations: " <<endl;

	for(int i=0;i<100;i++){
		cout << "Index: "<< i << " Value: "<< memory[i] <<endl;
	}

	//show artifacts

	cout << "ROB entries from head to tail: " <<endl;

	for(int i=headIx;i<=tailIx;i=((i+1)%17)){
		cout << "instruction: " << arrayROB[i].instruction << " index: " << i << endl;
	}
	std::deque<IQ> tempQueue(LSQ);

	std::cout << "LSQ from top to bottom: " <<endl;
	while (!tempQueue.empty())
	{
		std::cout << ' ' << tempQueue.front().instruction;
		tempQueue.pop_front();
	}
	std::cout << '\n';

	std::cout << endl << "IQ: " <<endl;

	for(int i=0;i<8;i++){
		cout << "Instruction: " << arrayIQ[i].instruction << " IQ IX: " << i << endl;
	}
	if(INTFUStr.ROBIx!=-1)
		cout << endl << "INTFU: " << arrayROB[INTFUStr.ROBIx].instruction << endl;
	if(MULFUStr.ROBIx!=-1)
		cout << endl << "MULFU: " << arrayROB[MULFUStr.ROBIx].instruction << "Cycle: " << MULFUStr.mulCounter+1 << endl;
	if(MEMFUCyc1.ROBIx!=-1)
		cout << endl << "MEMFU First Stage: " << arrayROB[MEMFUCyc1.ROBIx].instruction << endl;
	if(MEMFUCyc2.ROBIx!=-1)
		cout << endl << "MEMFU Second Stage: " << arrayROB[MEMFUCyc2.ROBIx].instruction << endl;
	if(MEMFUCyc3.ROBIx!=-1)
		cout << endl << "MEMFU Third Stage: " << arrayROB[MEMFUCyc3.ROBIx].instruction << endl;

	std::cout << endl << "Physical Registers: " <<endl;

	for(int i=0;i<16;i++){
		cout << endl << "Index: "<< i<<" Value: "<<physRegs[i] <<endl;
	}

	std::cout << endl << "Valid Array: " <<endl;

	for(int i=0;i<16;i++){
		cout << endl << "Index: "<< i<<" Value: "<<physRegsValid[i] <<endl;
	}
	std::cout << endl << "Alloc Array: " <<endl;

	for(int i=0;i<16;i++){
		cout << endl << "Index: "<< i<<" Value: "<<physRegsAlloc[i] <<endl;
	}
	std::cout << endl << "Rename Table: " <<endl;

	for(int i=0;i<9;i++){
		cout << endl << "Index: "<< i<<" Physical register: "<<renameTable[i] <<endl;
	}

	std::cout << endl << "Renamed array: " <<endl;

	for(int i=0;i<16;i++){
		cout << endl << "Index: "<< i<<" Value: "<<renamed[i] <<endl;
	}
}
