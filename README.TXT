/********************
Author: Brenda Taveras, Gissella Bejarano, Mehrnoosh Shakarami
APEX simulator (part 2) is a project for CS520 Computer Architecture in Fall 2015
Date 12/16/2015
***********************/

/************* ARGUMENTS  ************/
The arguments for executing this simlator are:

1 - Write Initialize, Simulate X or Display
2 - Uppercase letter that represent the following commands:
	Initialize: Initialize the simulator uploading all the instructions to the Memory instructions array
	Simulate [number]: simulates the given number of cycles of the pipeline
	Display the first 100 contents of Data memory array


/************  ASSUMPTIONS *************/

For now handle register X as a normal register regs[8]
When the Initialize method is called from the command line, the list of instructions is not re-grabbed from the text file
The simulation starts with the WB (where we update ARF from ROB.head) and continues the rest of the pipeline in order (Fetch 1, Fetch 2, Decode 1, Decode 2, Execute). This is to guarantee that an instruction that finishes in EX stage is not written to ARF in the same cycle.
Also we update the availability of the sources of IQ entries every time there’s a result to forward.
All the rest of the logic can be read in the report DOCUMENTATION-APEX.pdf
