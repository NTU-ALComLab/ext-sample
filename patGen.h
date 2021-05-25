#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <random>
#include <vector>

class RNG {
  public:
    RNG(int seed = 7) : mt(rd()), dis(0, SIZE_MAX) { mt.seed(seed); }
    size_t gen() { return dis(mt); }
    std::random_device rd;
    std::mt19937 mt;
    std::uniform_int_distribution<size_t> dis;
}; // end class RNG

static RNG rng;

std::vector<size_t> randGenPat(unsigned n) {
    std::vector<size_t> ret;
    ret.reserve(n);
    for (unsigned i = 0; i < n; ++i)
        ret.push_back(rng.gen());
    return ret;
}

class PatGen {
  public:
    PatGen(Abc_Ntk_t *pNtk, unsigned i, bool multiple);
    ~PatGen() {
        for (Abc_Ntk_t *pNtk : sampNtkList)
            delete pNtk;
    }
    std::vector<size_t> gen(unsigned index = 0);
    inline std::vector<size_t> randGen(unsigned num) const {
        return randGenPat(num);
    };
    inline void reset() { cnt = 0; }

  private:
    Abc_Ntk_t *pNtk;
    std::vector<Abc_Ntk_t *> sampNtkList;
    const unsigned nBit = sizeof(size_t) * 8;
    const unsigned nPi;
    unsigned cnt;
    bool multiple;
}; // end class PatGen
