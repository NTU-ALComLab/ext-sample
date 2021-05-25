#include <iostream>
#include <vector>

#include "base/main/main.h"
#include "base/main/mainInt.h"

using namespace std;

class SampleCircuit{

public:
    SampleCircuit();
    SampleCircuit(int nPI, int nPO);

    void setIOnum(int nPI, int nPO);
    void setRndSeed(unsigned seed) {srand(seed);}
    Abc_Ntk_t* genRand(bool fVerbose=false);
    Abc_Ntk_t* genRandCof(bool fVerbose=false);
    Abc_Ntk_t* genCircuit(bool fVerbose=false, bool fFastgen=false, std::ostream& out=std::cout);
    Abc_Ntk_t** genParCircuit(int nPar, bool fVerbose=false, bool fFastgen=false, std::ostream& out=std::cout);
    Abc_Ntk_t* genCircuit(Abc_Ntk_t* pNtk, bool fVerbose=false, bool fFastgen=false, std::ostream& out=std::cout);
    Abc_Ntk_t* connect(Abc_Ntk_t* pCkt, Abc_Ntk_t* pNtk, char* pName="sampled");

private:
    bool fInit;
    int nPI;
    int nPO;
    unsigned seed;

    Abc_Ntk_t* initCircuit(int nPI, int nPO, char*);
    void rndXORGen(int nPI, int nPO, vector< vector<bool> > &Mat, vector<int> &pivot);
    void gaussianEliminate(int nPI, int nPO, vector< vector<bool> > &Mat, vector<int> &pivot);
    void fastXORGen(int nPI, int nPO, vector< vector<bool> > &Mat, vector<int> &pivot);
    bool checkAvailable(int nPI, int nPO, vector< vector<bool> > &Mat);
    void printXOR(const vector< vector<int> >);
};
