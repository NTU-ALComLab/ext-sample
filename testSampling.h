#ifndef TESTSAMPLING_H

#define TESTSAMPLING_H

#include <random>
#include <unordered_set>
#include <vector>

#include "base/abc/abc.h"

typedef std::vector<size_t> PatVec;

class RNG {
 public:
  RNG(int seed = 7) : mt(rd()), dis(0, SIZE_MAX) { mt.seed(seed); }
  size_t gen() { return dis(mt); }
  std::random_device rd;
  std::mt19937 mt;
  std::uniform_int_distribution<size_t> dis;
};  // end class RNG

static RNG rng;

inline void simPat(Abc_Ntk_t *pNtk, const PatVec &pv, const int nIter = 0) {
  assert(Abc_NtkPiNum(pNtk) == pv.size());
  // const 1 pattern
  Abc_Obj_t *pConst1 = Abc_AigConst1(pNtk);
  pConst1->pCopy = (Abc_Obj_t *)((size_t)SIZE_MAX);
  // set up PIs
  unsigned i = 0;
  Abc_Obj_t *pObj;
  Abc_NtkForEachPi(pNtk, pObj, i) {
    pObj->pCopy = (Abc_Obj_t *)pv[i];
    ((size_t *)(pObj->pData))[nIter] = pv[i];
  }
  // value propagation
  Abc_AigForEachAnd(pNtk, pObj, i) {
    size_t v0 = (size_t)(Abc_ObjFanin0(pObj)->pCopy);
    size_t v1 = (size_t)(Abc_ObjFanin1(pObj)->pCopy);
    if (Abc_ObjFaninC0(pObj)) v0 = ~v0;
    if (Abc_ObjFaninC1(pObj)) v1 = ~v1;
    const size_t p = v0 & v1;
    pObj->pCopy = (Abc_Obj_t *)p;
    ((size_t *)(pObj->pData))[nIter] = p;
  }
}

inline void initPatHistory(Abc_Ntk_t *pNtk, const unsigned &iterMax) {
  unsigned i;
  Abc_Obj_t *pObj;
  Abc_NtkForEachPi(pNtk, pObj, i) pObj->pData = (void *)(new size_t[iterMax]);
  Abc_AigForEachAnd(pNtk, pObj, i) pObj->pData = (void *)(new size_t[iterMax]);
}

inline void freePatHistory(Abc_Ntk_t *pNtk) {
  unsigned i;
  Abc_Obj_t *pObj;
  Abc_NtkForEachPi(pNtk, pObj, i) delete[](size_t *)(pObj->pData);
  Abc_AigForEachAnd(pNtk, pObj, i) delete[](size_t *)(pObj->pData);
}

inline size_t getObjPat(Abc_Obj_t *pObj) {
  size_t p = (size_t)(Abc_ObjRegular(pObj)->pCopy);
  return Abc_ObjIsComplement(pObj) ? ~p : p;
}

inline PatVec randGenPat(unsigned n) {
  PatVec ret;
  ret.reserve(n);
  for (unsigned i = 0; i < n; ++i) ret.push_back(rng.gen());
  return ret;
}

class PatGen {
 public:
  PatGen(Abc_Ntk_t *n, unsigned i);
  ~PatGen() {
    for (Abc_Ntk_t *n : sampNtkList) {
      Abc_NtkDelete(n);
    }
    for (Abc_Ntk_t *n : coneList) {
      Abc_NtkDelete(n);
    }
  }
  PatVec gen(unsigned index = 0);
  inline PatVec randGen(unsigned num) const { return randGenPat(num); };
  inline void reset() { cnt = 0; }

 private:
  Abc_Ntk_t *pNtk;
  std::vector<Abc_Ntk_t *> sampNtkList;
  std::vector<Abc_Ntk_t *> coneList;
  unsigned nPi;
  unsigned cnt;
  const unsigned nBit = sizeof(size_t) * 8;
};

struct PatHasher {
  bool operator()(Abc_Obj_t *a) const {
    std::hash<size_t> hasher;
    size_t res = 0;
    for (int i = 0; i < 100; ++i) {
      res += res * 31 + hasher(((size_t *)a->pData)[i]);
    }
    return res;
  }
};

struct PatComparator {
  bool operator()(Abc_Obj_t *a, Abc_Obj_t *b) const {
    for (int i = 0; i < 100; ++i) {
      if (((size_t *)a->pData)[i] != ((size_t *)b->pData)[i]) {
        return true;
      }
    }
    return false;
  }
};

class SimTest {
 public:
  SimTest(Abc_Ntk_t *, bool);
  ~SimTest(){};

  void SimTestExec();
  void sim(unsigned iter, unsigned index = 0);

 private:
  std::unordered_multiset<Abc_Obj_t *, PatHasher, PatComparator> pattern_map;
  unsigned nIter = 100;
  PatGen *pGen;
  Abc_Ntk_t *pNtk;
  bool xOrSim;
  bool pairWise;
  int candidateCount = 0;
};

#endif /* end of include guard: TESTSAMPLING_H */
