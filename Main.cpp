#include <string>
#include "Simulator.h"
#include "Utilities.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>


int main(int argc, char *argv[]){
	
	string command="";

	Utilities util;
	
	Simulator simulator(argv[0]);

	simulator.readInstrucFile();

	while(true){
		std::getline (std::cin,command);
		if(command.compare("Initialize")==0){
			simulator.initialize();
		}else if(command.compare("Display")==0){
			simulator.display();
		}else{
			vector<string> vect;
			vect = util.split(command);
			if(vect.at(0).compare("Simulate")==0){
				int n=std::stoi(vect.at(1));				
				if(simulator.simulate(n)==1){ //if halt stopped execution end program
					simulator.display();
					break;
					
				}
			}else{
				break;
			}
		}
	}
	return 0; 
}
