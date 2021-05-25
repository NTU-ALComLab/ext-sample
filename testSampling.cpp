#include "ext-sample/testSampling.h"

#include <cassert>
#include <iostream>

#include "base/abc/abc.h"
#include "ext-sample/SampleCircuit.h"

PatGen::PatGen(Abc_Ntk_t *n, unsigned i) : pNtk(n), nPi(i), cnt(0) {
  if (i < 1) return;
  Abc_Obj_t *pPo;
  int j;

  if (i > nBit) {
    std::cout << "do not support sampling circuit over " << nBit << " PIs"
              << std::endl;
    assert(false);
  }

  Abc_NtkForEachPo(pNtk, pPo, j) {
    Abc_Ntk_t *pPoCone =
        Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0);
    if (Abc_ObjFaninC0(pPo)) {
      Abc_ObjSetFaninC(Abc_NtkPo(pPoCone, 0), 0);
    }
    if (Abc_NtkPiNum(pPoCone) < nPi) {
    }
    SampleCircuit sc(nPi, Abc_NtkPiNum(pPoCone));
    std::cout << Abc_NtkPiNum(pPoCone) << std::endl;
    if (Abc_NtkPiNum(pPoCone) < nPi) {
      sampNtkList.push_back(NULL);
    } else {
      sampNtkList.push_back(sc.genCircuit(pPoCone));
    }
    coneList.push_back(pPoCone);
  }

  // generate sampling circuit
  // sc.setRndSeed(7);
}

PatVec PatGen::gen(unsigned index) {
  Abc_Obj_t *pObj;
  int i;
  if (sampNtkList[index] == NULL) {
    return randGenPat(Abc_NtkPiNum(pNtk));
  }
  PatVec ret;
  ret.reserve(Abc_NtkPiNum(pNtk));

  // generate input patterns
  if ((cnt * nBit) >= (1 << nPi)) return randGen(Abc_NtkPiNum(pNtk));

  PatVec pv(Abc_NtkPiNum(sampNtkList[index]), 0);
  for (unsigned j = 0; j < nBit; ++j) {
    size_t a = nBit * cnt + j;
    for (unsigned i = 0; i < pv.size(); ++i) {
      size_t b = (a >> i) & (size_t)1;
      pv[i] |= (b << j);
    }
  }
  cnt += 1;

  // for(auto p: pv)
  //    cout << CeNode::getDXPatStr(make_pair((size_t)0, p)) << endl;

  // propagate
  simPat(sampNtkList[index], pv);

  ret.clear();
  unsigned j = 0;

  assert(Abc_NtkPoNum(sampNtkList[index]) == Abc_NtkPiNum(coneList[index]));
  Abc_NtkForEachPi(pNtk, pObj, i) {
    if (strcmp(Abc_ObjName(pObj), Abc_ObjName(Abc_NtkPi(coneList[index], j))) != 0) {
      ret.push_back(rng.gen());
      continue;
    }
    ret.push_back(getObjPat(Abc_ObjChild0(Abc_NtkPo(sampNtkList[index], j))));
    ++j;
  }
  return ret;
}

SimTest::SimTest(Abc_Ntk_t *ntk, bool x) : pNtk(ntk), xOrSim(x) {
  if (xOrSim) {
    pGen = new PatGen(pNtk, 10);
    cout << "generator done" << endl;
  }
}

void SimTest::sim(unsigned iter, unsigned index) {
  if (xOrSim) {
    PatVec pat = pGen->gen(index);
    simPat(pNtk, pat, iter);
  } else {
    simPat(pNtk, randGenPat(Abc_NtkPiNum(pNtk)), iter);
  }
}

void SimTest::SimTestExec() {
  Abc_Obj_t *pPo, *pObj;
  initPatHistory(pNtk, 100);
  int i = 0, j = 0;
  pattern_map.clear();
  if (!xOrSim) {
    for (int i = 0; i < 100; ++i) {
      sim(i);
    }
  } else {
    Abc_NtkForEachPo(pNtk, pPo, j) {
      sim(i, j);
      ++i;
      if (i == 100) break;
    }
  }
  Abc_AigForEachAnd(pNtk, pObj, j) {
    // auto equal_range = pattern_map.equal_range(pObj);
    pattern_map.insert(pObj);
  }
  int total_pairs = 0;
  for (int i = 0; i < pattern_map.bucket_count(); ++i) {
    if (pattern_map.bucket_size(i) != 0) {
      std::cout << pattern_map.bucket_size(i) << std::endl;
    }
  }
  cout << "Total equivalent pair: " << total_pairs << endl;
  freePatHistory(pNtk);
}
