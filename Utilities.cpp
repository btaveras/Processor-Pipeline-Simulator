#include "Utilities.h"

//Code grabbed from http://stackoverflow.com/questions/236129/split-a-string-in-c


std::vector<std::string> Utilities::split(const std::string &text){
    std::vector<std::string> tokens; // Create vector to hold our words	
    std::string buf; // Have a buffer string
    std::stringstream ss(text); // Insert the string into a stream

    while (ss >> buf)
        tokens.push_back(buf);
  
    return tokens;
}
