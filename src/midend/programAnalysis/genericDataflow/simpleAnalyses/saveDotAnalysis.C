#include "saveDotAnalysis.h"

namespace dataflow {
/***********************
 *** SaveDotAnalysis ***
 ***********************/

void SaveDotAnalysis::runAnalysis(const Function& func, NodeState* state)
{
        if(func.get_definition())
        {
                // output the CFG to a file
                std::ofstream fileCFG;
                fileCFG.open((func.get_name().getString()+"_cfg.dot").c_str());
                cout << "    writing to file "<<(func.get_name().getString()+"_cfg.dot")<<"\n";
                cfgToDot(fileCFG, func.get_name(), func.get_definition()->cfgForBeginning());
                fileCFG.close();
        }
}

// Saves the CFGs of all the functions into their own files
// Precondition: initAnalysis() has been called
void saveCFGsToDots()
{
        SaveDotAnalysis sda;
        UnstructuredPassInterAnalysis upia_sda(sda);
        upia_sda.runAnalysis();
}
}; // namespace dataflow
