#include "base/abc/abc.h"
#include <ext-sample/fasttime.h>
#include <ext-sample/SampleCircuit.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <queue>
#include <random>
#include <set>
#include <unordered_map>

using namespace std;

#define SWAP(a, b)                                                             \
  do {                                                                         \
    a ^= b;                                                                    \
    b ^= a;                                                                    \
    a ^= b;                                                                    \
  } while (0)
#define ObjId(pObj) Abc_ObjId(Abc_ObjRegular(pObj))

SampleCircuit::SampleCircuit() {
  fInit = false;
  seed = chrono::system_clock::now().time_since_epoch().count();
  srand(seed);
}

SampleCircuit::SampleCircuit(int nPI, int nPO) {
  assert(nPI > 0 && nPO > 0 && nPI < nPO);
  this->nPI = nPI;
  this->nPO = nPO;

  fInit = true;
  seed = chrono::system_clock::now().time_since_epoch().count();
  srand(seed);
}

void SampleCircuit::setIOnum(int nPI, int nPO) {
  assert(nPI > 0 && nPO > 0 && nPI < nPO);
  this->nPI = nPI;
  this->nPO = nPO;

  fInit = true;
}

Abc_Ntk_t *SampleCircuit::initCircuit(int _nPI, int _nPO, char *pCircuitName) {
  Abc_Ntk_t *pCkt;
  Abc_Obj_t *pObj;
  // initialize
  pCkt = Abc_NtkAlloc(ABC_NTK_STRASH, ABC_FUNC_AIG, 1);
  pCkt->pName = Extra_UtilStrsav(pCircuitName);

  // create PI/PO
  for (int i = 0; i < _nPI; i++) {
    char pObjName[10];
    pObj = Abc_NtkCreatePi(pCkt);
    sprintf(pObjName, "SC_i%d", i + 1);
    Abc_ObjAssignName(pObj, pObjName, NULL);
  }
  for (int i = 0; i < _nPO; i++) {
    char pObjName[10];
    pObj = Abc_NtkCreatePo(pCkt);
    sprintf(pObjName, "SC_o%d", i + 1);
    Abc_ObjAssignName(pObj, pObjName, NULL);
  }
  return pCkt;
}

Abc_Ntk_t *SampleCircuit::genRand(bool fVerbose) {
  assert(fInit);
  char *pCircuitName = "sample";
  Abc_Obj_t *pObj;
  Abc_Ntk_t *pCkt = initCircuit(nPI, nPO, pCircuitName);

  // const node
  Abc_Obj_t *pCktOne = Abc_AigConst1(pCkt);
  Abc_Obj_t *pCktZero = Abc_ObjNot(pCktOne);

  // binary code
  vector<Abc_Obj_t *> code;
  for (int i = 0; i < pow(2, nPI); i++) {
    Abc_Obj_t *pObjF = pCktOne;
    for (int j = 0; j < nPI; j++) {
      if (i & (1 << j))
        pObjF = Abc_AigAnd((Abc_Aig_t *)pCkt->pManFunc,
                           Abc_NtkPi(pCkt, nPI - 1 - j), pObjF);
      else
        pObjF = Abc_AigAnd((Abc_Aig_t *)pCkt->pManFunc,
                           Abc_ObjNot(Abc_NtkPi(pCkt, nPI - 1 - j)), pObjF);
    }
    code.push_back(pObjF);
  }

  // random matrix
  bool **mat;
  mat = new bool *[(int)pow(2, nPI)];
  for (int i = 0; i < pow(2, nPI); i++)
    mat[i] = new bool[nPO];
  set<string> seq;
  for (int i = 0; i < pow(2, nPI); i++) {
    while (true) {
      string sample;
      for (int j = 0; j < nPO; j++)
        sample.push_back((rand() % 2 ? '1' : '0'));
      if (seq.find(sample) == seq.end()) {
        for (int j = 0; j < nPO; j++)
          mat[i][j] = (sample[j] == '1' ? 1 : 0);
        seq.insert(sample);
        break;
      }
    }
  }

  if (fVerbose) {
    cout << "Random Assignments:\n";
    for (int i = 0; i < pow(2, nPI); i++) {
      for (int j = 0; j < nPO; j++)
        cout << mat[i][j];
      cout << "\n";
    }
  }

  // construct sampling circuit
  for (int j = 0; j < nPO; j++) {
    Abc_Obj_t *pObjF = pCktZero;
    for (int i = 0; i < pow(2, nPI); i++)
      if (mat[i][j])
        pObjF = Abc_AigOr((Abc_Aig_t *)pCkt->pManFunc, code[i], pObjF);
    Abc_ObjAddFanin(Abc_NtkPo(pCkt, j), pObjF);
  }

  // deconstruction
  for (int i = 0; i < pow(2, nPI); i++)
    delete[] mat[i];
  delete[] mat;

  // remove dangling nodes
  Abc_AigCleanup((Abc_Aig_t *)pCkt->pManFunc);

  // return constructed AIG
  if (!Abc_NtkCheck(pCkt)) {
    printf("The AIG construction has failed.\n");
    Abc_NtkDelete(pCkt);
    return NULL;
  }
  return pCkt;
}

Abc_Ntk_t *SampleCircuit::genRandCof(bool fVerbose) {
  assert(fInit);
  char *pCircuitName = "sample";
  Abc_Obj_t *pObj;
  Abc_Ntk_t *pCkt;

  // initialize
  pCkt = initCircuit(nPI, nPO, pCircuitName);

  // const node
  Abc_Obj_t *pCktOne = Abc_AigConst1(pCkt);
  Abc_Obj_t *pCktZero = Abc_ObjNot(pCktOne);

  // random select (nPO-nPI) cofactored variables
  vector<int> seq(nPO);
  for (int i = 0; i < nPO; i++)
    seq[i] = i;
  for (int i = 0; i < nPO; i++) {
    int pos = nPO * (rand() / (RAND_MAX + 1.0));
    if (i != pos)
      SWAP(seq[i], seq[pos]);
  }
  for (int i = 0; i < nPO - nPI; i++) {
    if (rand() % 2) {
      if (fVerbose)
        cout << "Cofator input " << seq[i] << " to 1\n";
      Abc_ObjAddFanin(Abc_NtkPo(pCkt, seq[i]), pCktOne);
    } else {
      if (fVerbose)
        cout << "Cofator input " << seq[i] << " to 0\n";
      Abc_ObjAddFanin(Abc_NtkPo(pCkt, seq[i]), pCktZero);
    }
  }

  // connect the rest variables
  seq.erase(seq.begin(), seq.begin() + (nPO - nPI));
  for (int i = 0; i < nPI; i++)
    Abc_ObjAddFanin(Abc_NtkPo(pCkt, seq[i]), Abc_NtkPi(pCkt, i));

  // remove dangling nodes
  Abc_AigCleanup((Abc_Aig_t *)pCkt->pManFunc);

  // return constructed AIG
  if (!Abc_NtkCheck(pCkt)) {
    printf("The AIG construction has failed.\n");
    Abc_NtkDelete(pCkt);
    return NULL;
  }
  return pCkt;
}

Abc_Ntk_t *SampleCircuit::genCircuit(bool fVerbose, bool fFastgen,
                                     std::ostream &out) {
  vector<int> pivot;
  vector<vector<bool>> Mat;
  vector<vector<int>> XOR;
  char *pCircuitName = "sample";
  Abc_Obj_t *pObj;
  Abc_Ntk_t *pCkt;

  // allocate memory
  XOR.resize(nPO);
  Mat.resize(nPO - nPI);

  // initialize
  pCkt = initCircuit(nPI, nPO, pCircuitName);

  // const node
  Abc_Obj_t *pCktOne = Abc_AigConst1(pCkt);
  Abc_Obj_t *pCktZero = Abc_ObjNot(pCktOne);

  // generate XOR constraints
  assert(fInit);
  if (fFastgen) {
    fastXORGen(nPI, nPO, Mat, pivot);
  } else {
    do {
      rndXORGen(nPI, nPO, Mat, pivot);
    } while (!checkAvailable(nPO - nPI, nPO, Mat));
  }

  // print the sampling matrix
  if (fVerbose) {
    out << "Sampling Matrix =\n";
    for (int i = 0; i < nPO - nPI; i++) {
      out << "|";
      for (int j = 0; j < nPO; j++)
        out << " " << Mat[i][j];
      out << " | " << Mat[i][nPO];
      out << " |\n";
    }
  }

  // create mapping
  vector<int>::iterator iter = pivot.begin();
  queue<int> restPO;
  unordered_map<int, int> PImap;
  int mPO, mPI = 0;
  Abc_Obj_t *pObjF;
  std::vector<int> shuffle_index;
  for (int i = 0; i < nPI; ++i) {
    shuffle_index.push_back(i);
  }
  shuffle(shuffle_index.begin(), shuffle_index.end(),
          default_random_engine(random_seed_from_clock()));
  for (mPO = 0; mPO < nPO; mPO++) {
    if (iter == pivot.end() || mPO != *iter) {
      assert(mPI < nPI);
      PImap[mPO] = shuffle_index[mPI];
      // connect to PO
      vector<int> XOR_vec(2);
      XOR_vec[0] = 0;
      XOR_vec[1] = shuffle_index[mPI];
      XOR[mPO] = XOR_vec;
      pObjF = Abc_AigAnd((Abc_Aig_t *)pCkt->pManFunc,
                         Abc_NtkPi(pCkt, shuffle_index[mPI]), pCktOne);
      Abc_ObjAddFanin(Abc_NtkPo(pCkt, mPO), pObjF);
      mPI++;
    } else {
      restPO.push(mPO);
      iter++;
    }
  }

  // create internal nodes
  iter = pivot.begin();
  for (int i = 0; i < nPO - nPI; i++) {
    if (Mat[i][nPO])
      pObjF = pCktOne;
    else
      pObjF = pCktZero;
    vector<int> XOR_vec;
    XOR_vec.push_back(Mat[i][nPO]);
    for (int j = *iter + 1; j < nPO; j++) {
      if (Mat[i][j]) {
        pObjF = Abc_AigXor((Abc_Aig_t *)pCkt->pManFunc, pObjF,
                           Abc_NtkPi(pCkt, PImap[j]));
        XOR_vec.push_back(PImap[j]);
      }
    }
    XOR[restPO.front()] = XOR_vec;

    // connect to PO
    Abc_ObjAddFanin(Abc_NtkPo(pCkt, restPO.front()), pObjF);
    restPO.pop();

    iter++;
  }

  if (fVerbose) {
    out << "XOR constraints:\n";
    for (int i = 0; i < nPO; i++) {
      vector<int> vec = XOR[i];
      out << "\tPO" << i << " = ";
      for (vector<int>::iterator iter = vec.begin(); iter != vec.end();
           iter++) {
        if (iter == vec.begin()) {
          if (vec.size() == 1)
            out << *iter;
          else if (*iter == 1)
            out << "1+";
          continue;
        }
        out << "PI" << *iter;
        if (iter + 1 != vec.end())
          out << "+";
      }
      out << "\n";
    }
  }

  // deconstruction
  pivot.clear();
  Mat.clear();
  XOR.clear();

  // remove dangling nodes
  Abc_AigCleanup((Abc_Aig_t *)pCkt->pManFunc);

  // return constructed AIG
  if (!Abc_NtkCheck(pCkt)) {
    printf("The AIG construction has failed.\n");
    Abc_NtkDelete(pCkt);
    return NULL;
  }
  return pCkt;
}

Abc_Ntk_t **SampleCircuit::genParCircuit(int nPar, bool fVerbose, bool fFastgen,
                                         std::ostream &out) {
  int nCount = pow(2, nPar);
  Abc_Ntk_t **vSamp = new Abc_Ntk_t *[nCount];
  vector<vector<bool>> Mat;
  vector<vector<bool>> initMat;
  vector<vector<int>> XOR;
  vector<int> pivot;
  Abc_Obj_t *pObj;
  // generate nPar constraint
  std::default_random_engine engine;
  engine.seed(random_seed_from_clock());
  std::mt19937 gen(engine());
  std::uniform_int_distribution<> bool_gen(0, 1);
  int nParPI = nPI - nPar;
  initMat.resize(nPar);
  Mat.resize(nPO - nParPI);
  XOR.resize(nPO);
  do {
    for (int i = 0; i < nPar; i++) {
      initMat[i].resize(nPO + 1);
      for (int j = 0; j < nPO; ++j) {
        initMat[i][j] = bool_gen(gen);
      }
      initMat[i][nPO] = bool_gen(gen);
    }
    gaussianEliminate(nPar, nPO, initMat, pivot);
  } while (!checkAvailable(nPar, nPO, initMat));
  if (fVerbose) {
    out << "Initial Sampling Matrix =\n";
    for (int i = 0; i < nPar; i++) {
      out << "|";
      for (int j = 0; j < nPO; j++)
        out << " " << initMat[i][j];
      out << " | " << initMat[i][nPO];
      out << " |\n";
    }
  }

  for (int c = 0; c < nCount; ++c) {
    // assign value for the partition constraint
    XOR.clear();
    for (int i = 0; i < nPar; ++i) {
      Mat[i].resize(nPO + 1);
      for (int j = 0; j < nPO; ++j) {
        Mat[i][j] = initMat[i][j];
      }
      Mat[i][nPO] = (c & (1 << i)) >> i;
    }
    if (fVerbose) {
      out << "Initial Sampling Matrix =\n";
      for (int i = 0; i < nPar; i++) {
        out << "|";
        for (int j = 0; j < nPO; j++)
          out << " " << Mat[i][j];
        out << " | " << Mat[i][nPO];
        out << " |\n";
      }
    }

    char pCircuitName[10];
    sprintf(pCircuitName, "sample_%d", c);
    Abc_Ntk_t *pCkt = initCircuit(nParPI, nPO, pCircuitName);

    // const node
    Abc_Obj_t *pCktOne = Abc_AigConst1(pCkt);
    Abc_Obj_t *pCktZero = Abc_ObjNot(pCktOne);

    // generate the rest of the constraint
    do {
      for (int i = nPar; i < nPO - nParPI; i++) {
        Mat[i].resize(nPO + 1, 0);
        for (int j = 0; j < nPO; ++j) {
          Mat[i][j] = bool_gen(gen);
        }
        Mat[i][nPO] = bool_gen(gen);
      }
      gaussianEliminate(nPO - nParPI, nPO, Mat, pivot);
    } while (!checkAvailable(nPO - nParPI, nPO, Mat));
    // print the sampling matrix
    if (fVerbose) {
      out << "Sampling Matrix =\n";
      for (int i = 0; i < nPO - nParPI; i++) {
        out << "|";
        for (int j = 0; j < nPO; j++)
          out << " " << Mat[i][j];
        out << " | " << Mat[i][nPO];
        out << " |\n";
      }
    }
    // create mapping
    vector<int>::iterator iter = pivot.begin();
    queue<int> restPO;
    unordered_map<int, int> PImap;
    int mPO, mPI = 0;
    Abc_Obj_t *pObjF;
    // std::vector<int> shuffle_index;
    // for (int i = 0; i < nPO; ++i) {
    //   shuffle_index.push_back(i);
    // }
    // std::random_shuffle ( shuffle_index.begin(), shuffle_index.end() );
    for (mPO = 0; mPO < nPO; mPO++) {
      if (iter == pivot.end() || mPO != *iter) {
        assert(mPI < nParPI);
        PImap[mPO] = mPI;
        // connect to PO
        vector<int> XOR_vec(2);
        XOR_vec[0] = 0;
        XOR_vec[1] = mPI;
        XOR[mPO] = XOR_vec;
        pObjF = Abc_AigAnd((Abc_Aig_t *)pCkt->pManFunc, Abc_NtkPi(pCkt, mPI++),
                           pCktOne);
        Abc_ObjAddFanin(Abc_NtkPo(pCkt, mPO), pObjF);
      } else {
        restPO.push(mPO);
        iter++;
      }
    }

    // create internal nodes
    iter = pivot.begin();
    for (int i = 0; i < nPO - nParPI; i++) {
      if (Mat[i][nPO])
        pObjF = pCktOne;
      else
        pObjF = pCktZero;
      vector<int> XOR_vec;
      XOR_vec.push_back(Mat[i][nPO]);
      for (int j = *iter + 1; j < nPO; j++) {
        if (Mat[i][j]) {
          pObjF = Abc_AigXor((Abc_Aig_t *)pCkt->pManFunc, pObjF,
                             Abc_NtkPi(pCkt, PImap[j]));
          XOR_vec.push_back(PImap[j]);
        }
      }
      XOR[restPO.front()] = XOR_vec;

      // connect to PO
      Abc_ObjAddFanin(Abc_NtkPo(pCkt, restPO.front()), pObjF);
      restPO.pop();

      iter++;
    }

    if (fVerbose) {
      out << "XOR constraints:\n";
      for (int i = 0; i < nPO; i++) {
        vector<int> vec = XOR[i];
        out << "\tPO" << i << " = ";
        for (vector<int>::iterator iter = vec.begin(); iter != vec.end();
             iter++) {
          if (iter == vec.begin()) {
            if (vec.size() == 1)
              out << *iter;
            else if (*iter == 1)
              out << "1+";
            continue;
          }
          out << "PI" << *iter;
          if (iter + 1 != vec.end())
            out << "+";
        }
        out << "\n";
      }
    }

    // deconstruction
    pivot.clear();
    XOR.clear();

    // remove dangling nodes
    Abc_AigCleanup((Abc_Aig_t *)pCkt->pManFunc);
    vSamp[c] = pCkt;
  }
  return vSamp;
}

bool cmp(vector<int> i, vector<int> j) { return (i.size() > j.size()); }
Abc_Ntk_t *SampleCircuit::genCircuit(Abc_Ntk_t *pNtk, bool fVerbose,
                                     bool fFastgen, std::ostream &out) {
  int i, j;
  vector<int> pivot;
  vector<vector<bool>> Mat;
  vector<vector<int>> XOR;
  char *pName = "sample";
  Abc_Obj_t *pCo, *pObj, *pObjF;
  Abc_Ntk_t *pCkt;
  Vec_Ptr_t *vSupp;
  vector<vector<int>> sup_vec;
  vector<int> vSuppRef;
  unordered_map<string, int> sup_map;

  // get support information
  sup_vec.resize(Abc_NtkPoNum(pNtk));
  vSuppRef.resize(Abc_NtkPiNum(pNtk), 0);
  Abc_NtkForEachPi(pNtk, pObj, i) sup_map[Abc_ObjName(pObj)] = i;
  Abc_NtkForEachCo(pNtk, pCo, i) {
    // cout << Abc_ObjName(pCo) << "\n";
    vSupp = Abc_NtkNodeSupport(pNtk, &pCo, 1);
    Vec_PtrForEachEntry(Abc_Obj_t *, vSupp, pObj, j) {
      sup_vec[i].push_back(sup_map[Abc_ObjName(pObj)]);
      vSuppRef[sup_map[Abc_ObjName(pObj)]]++;
    }
    // for (vector<int>::iterator iter = sup_vec[i].begin(); iter !=
    // sup_vec[i].end(); iter++)
    //    cout << *iter << "(" << Abc_ObjName(Abc_NtkPi(pNtk, *iter)) << ") ";
    // cout << "\n";
    Vec_PtrFree(vSupp);
  }

  // sort by support reference
  for (i = 0; i < sup_vec.size(); i++)
    sort(sup_vec[i].begin(), sup_vec[i].end(),
         [&vSuppRef](size_t i1, size_t i2) {
           return vSuppRef[i1] < vSuppRef[i2];
         });
  // sort by support number
  sort(sup_vec.begin(), sup_vec.end(), cmp);

  // initialize
  pCkt = initCircuit(nPI, nPO, pName);

  // const node
  Abc_Obj_t *pCktOne = Abc_AigConst1(pCkt);
  Abc_Obj_t *pCktZero = Abc_ObjNot(pCktOne);

  // generate sampling circuit
  assert(fInit);
  vector<bool> vCheck(nPO, false);
  XOR.resize(nPO);
  while (!sup_vec.empty()) {
    // sampling the cone with the most supports
    vector<int> supp = sup_vec.front();
    if (supp.size() <= 0) {
      sup_vec.erase(sup_vec.begin());
      continue;
    }
    int nPOc = supp.size();
    sup_vec.erase(sup_vec.begin());
    shuffle(supp.begin(), supp.end(),
            default_random_engine(random_seed_from_clock()));

    if (fVerbose) {
      out << "Support :" << std::endl;
      for (int sup : supp) {
        out << Abc_ObjName(Abc_NtkPi(pNtk, sup)) << " ";
      }
      out << std::endl;
    }
    // HACK generate random linear constraint
    // deal the case nPOc <= nPI
    if (nPOc <= nPI) {
      // shuffle PI variable
      Abc_Obj_t *pCktOne = Abc_AigConst1(pCkt);
      Abc_Obj_t *pCktZero = Abc_ObjNot(pCktOne);
      Abc_Obj_t *pObjF;
      vector<int> var(nPI);
      for (i = 0; i < nPI; i++)
        var[i] = i;
      shuffle(var.begin(), var.end(),
              default_random_engine(random_seed_from_clock()));
      std::default_random_engine engine;
      engine.seed(random_seed_from_clock());
      std::mt19937 gen(engine());
      for (i = 0; i < nPOc; i++) {
        vector<int> rand_xor;
        vector<int> XOR_vec;
        rand_xor.resize(nPI + 1);
        std::uniform_int_distribution<> bool_gen(0, 1);
        for (int i = 0; i < nPI + 1; ++i) {
          rand_xor[i] = bool_gen(gen);
        }
        if (rand_xor[nPI])
          pObjF = pCktOne;
        else
          pObjF = pCktZero;
        for (int j = 0; j < nPI; j++) {
          if (!rand_xor[j])
            continue;
          pObjF = Abc_AigXor((Abc_Aig_t *)pCkt->pManFunc, pObjF,
                             Abc_NtkPi(pCkt, var[j]));
          XOR_vec.push_back(var[j]);
        }
        XOR[supp[i]] = XOR_vec;
        Abc_ObjAddFanin(Abc_NtkPo(pCkt, supp[i]), pObjF);
      }
    } else {
      // allocate memory
      Mat.resize(nPOc - nPI);

      // generate XOR constraints
      if (fFastgen) {
        fastXORGen(nPI, nPOc, Mat, pivot);
      } else {
        do {
          rndXORGen(nPI, nPOc, Mat, pivot);
        } while (!checkAvailable(nPOc - nPI, nPOc, Mat));
      }

      // print the sampling matrix
      if (fVerbose) {
        out << "Sampling Matrix =\n";
        for (i = 0; i < nPOc - nPI; i++) {
          out << "|";
          for (j = 0; j < nPOc; j++)
            out << " " << Mat[i][j];
          out << " | " << Mat[i][nPOc];
          out << " |\n";
        }
      }

      // create mapping
      vector<int>::iterator iter = pivot.begin();
      queue<int> restPO;
      unordered_map<int, int> PImap;
      Abc_Obj_t *pObjF;
      std::vector<int> shuffle_index;
      for (int i = 0; i < nPI; ++i) {
        shuffle_index.push_back(i);
      }
      int mPO, mPI = 0;
      for (mPO = 0; mPO < nPOc; mPO++) {
        if (iter == pivot.end() || mPO != *iter) {
          assert(mPI < nPI);
          PImap[mPO] = shuffle_index[mPI];
          // connect to PO
          vector<int> XOR_vec(2);
          XOR_vec[0] = 0;
          XOR_vec[1] = shuffle_index[mPI];
          XOR[supp[mPO]] = XOR_vec;
          pObjF = Abc_AigAnd((Abc_Aig_t *)pCkt->pManFunc,
                             Abc_NtkPi(pCkt, shuffle_index[mPI]), pCktOne);
          mPI++;
          Abc_ObjAddFanin(Abc_NtkPo(pCkt, supp[mPO]), pObjF);
        } else {
          restPO.push(supp[mPO]);
          iter++;
        }
      }

      // create internal nodes
      iter = pivot.begin();
      for (i = 0; i < nPOc - nPI; i++) {
        if (Mat[i][nPOc])
          pObjF = pCktOne;
        else
          pObjF = pCktZero;
        vector<int> XOR_vec;
        XOR_vec.push_back(Mat[i][nPOc]);
        for (j = *iter + 1; j < nPOc; j++) {
          if (Mat[i][j]) {
            pObjF = Abc_AigXor((Abc_Aig_t *)pCkt->pManFunc, pObjF,
                               Abc_NtkPi(pCkt, PImap[j]));
            XOR_vec.push_back(PImap[j]);
          }
        }
        XOR[restPO.front()] = XOR_vec;

        // connect to PO
        Abc_ObjAddFanin(Abc_NtkPo(pCkt, restPO.front()), pObjF);
        restPO.pop();

        iter++;
      }

      pivot.clear();
      Mat.clear();
    }

    // update check vector
    for (i = 0; i < supp.size(); i++)
      vCheck[supp[i]] = true;

    // remove the connected variables
    for (i = 0; i < sup_vec.size(); i++) {
      int erase = 0;
      for (j = 0; j < sup_vec[i].size(); j++) {
        if (vCheck[sup_vec[i][j]]) {
          if (erase != j)
            SWAP(sup_vec[i][j], sup_vec[i][erase]);
          erase++;
        }
      }
      sup_vec[i].erase(sup_vec[i].begin(), sup_vec[i].begin() + erase);
    }
    // sort the rest cone
    sort(sup_vec.begin(), sup_vec.end(), cmp);
  }

  if (fVerbose) {
    out << "XOR constraints:\n";
    for (int i = 0; i < nPO; i++) {
      vector<int> vec = XOR[i];
      out << "\tPO" << i << " = ";
      for (vector<int>::iterator iter = vec.begin(); iter != vec.end();
           iter++) {
        if (iter == vec.begin()) {
          if (vec.size() == 1)
            out << *iter;
          else if (*iter == 1)
            out << "1+";
          continue;
        }
        out << "PI" << *iter;
        if (iter + 1 != vec.end())
          out << "+";
      }
      out << "\n";
    }
  }

  // deconstruction
  sup_vec.clear();
  vSuppRef.clear();
  pivot.clear();
  Mat.clear();
  XOR.clear();
  Abc_NtkForEachPo(pCkt, pObj, i) {
    if (Abc_ObjFaninNum(pObj) == 0) {
      Abc_ObjAddFanin(pObj, Abc_AigConst1(pCkt));
    }
  }

  // remove dangling nodes
  Abc_AigCleanup((Abc_Aig_t *)pCkt->pManFunc);

  // return constructed AIG
  if (!Abc_NtkCheck(pCkt)) {
    printf("The AIG construction has failed.\n");
    Abc_NtkDelete(pCkt);
    return NULL;
  }
  return pCkt;
}

vector<size_t> argsort(const vector<int> &v) {
  // initialize original index locations
  vector<size_t> idx(v.size());
  for (size_t i = 0; i < idx.size(); ++i)
    idx[i] = i;

  // sort indexes based on comparing values in v
  sort(idx.begin(), idx.end(),
       [&v](size_t i1, size_t i2) { return v[i1] < v[i2]; });

  return idx;
}

Abc_Ntk_t *SampleCircuit::connect(Abc_Ntk_t *pCkt, Abc_Ntk_t *pNtk,
                                  char *pName) {
  assert(pCkt && Abc_NtkHasAig(pCkt) && Abc_NtkIsStrash(pCkt));
  assert(pNtk && Abc_NtkHasAig(pNtk) && Abc_NtkIsStrash(pNtk));
  assert(Abc_NtkPoNum(pCkt) == Abc_NtkPiNum(pNtk));

  int i;
  Abc_Ntk_t *pNtkRes, *pNtkDup;
  Abc_Obj_t *pObj, *pPo;
  Abc_Obj_t *pChild0;
  Abc_Obj_t *pChild1;
  unordered_map<int, int> IDmap;

  // create new network
  pNtkRes = Abc_NtkAlloc(ABC_NTK_STRASH, ABC_FUNC_AIG, 1);
  pNtkRes->pName = Extra_UtilStrsav(pName);
  pNtkDup = Abc_NtkDup(pNtk);

  // map constant nodes
  Abc_AigConst1(pCkt)->pCopy = Abc_AigConst1(pNtkRes);
  Abc_AigConst1(pNtkDup)->pCopy = Abc_AigConst1(pNtkRes);

  // clone PIs & POs
  Abc_NtkForEachPi(pCkt, pObj, i) Abc_NtkDupObj(pNtkRes, pObj, 1);
  Abc_NtkForEachPo(pNtkDup, pObj, i) Abc_NtkDupObj(pNtkRes, pObj, 1);

  // map IDs to numbers
  Abc_NtkForEachPi(pNtkDup, pObj, i) IDmap[ObjId(pObj)] = i;

  // copy the AND gates
  Abc_AigForEachAnd(pCkt, pObj, i) pObj->pCopy =
      Abc_AigAnd((Abc_Aig_t *)pNtkRes->pManFunc, Abc_ObjChild0Copy(pObj),
                 Abc_ObjChild1Copy(pObj));
  Abc_AigForEachAnd(pNtkDup, pObj, i) {
    if (Abc_ObjIsPi(Abc_ObjFanin0(pObj))) {
      pPo = Abc_NtkPo(pCkt, IDmap[ObjId(Abc_ObjFanin0(pObj))]);
      pChild0 = Abc_ObjNotCond(Abc_ObjFanin0(pPo)->pCopy,
                               Abc_ObjFaninC0(pPo) ^ Abc_ObjFaninC0(pObj));
    } else
      pChild0 = Abc_ObjChild0Copy(pObj);
    if (Abc_ObjIsPi(Abc_ObjFanin1(pObj))) {
      pPo = Abc_NtkPo(pCkt, IDmap[ObjId(Abc_ObjFanin1(pObj))]);
      pChild1 = Abc_ObjNotCond(Abc_ObjFanin0(pPo)->pCopy,
                               Abc_ObjFaninC0(pPo) ^ Abc_ObjFaninC1(pObj));
    } else
      pChild1 = Abc_ObjChild1Copy(pObj);
    pObj->pCopy = Abc_AigAnd((Abc_Aig_t *)pNtkRes->pManFunc, pChild0, pChild1);
  }
  // relink CO
  Abc_NtkForEachCo(pNtkDup, pObj, i) {
    if (Abc_ObjIsPi(Abc_ObjFanin0(pObj))) {
      pPo = Abc_NtkPo(pCkt, IDmap[ObjId(Abc_ObjFanin0(pObj))]);
      pChild0 = Abc_ObjNotCond(Abc_ObjFanin0(pPo)->pCopy,
                               Abc_ObjFaninC0(pPo) ^ Abc_ObjFaninC0(pObj));
    }
    // else if (Abc_AigNodeIsConst(Abc_ObjFanin0(pObj)))
    //    pChild0 = Abc_ObjNotCond( Abc_AigConst1(pNtkRes), Abc_ObjFaninC0(pObj)
    //    );
    else
      pChild0 = Abc_ObjChild0Copy(pObj);
    Abc_ObjAddFanin(pObj->pCopy, pChild0);
  }

  // remove dangling nodes
  Abc_AigCleanup((Abc_Aig_t *)pNtkRes->pManFunc);

  // return constructed AIG
  Abc_NtkDelete(pNtkDup);
  if (!Abc_NtkCheck(pNtkRes)) {
    printf("The AIG construction has failed.\n");
    Abc_NtkDelete(pNtkRes);
    return NULL;
  }
  return pNtkRes;
}

void SampleCircuit::fastXORGen(int nPI, int nPO, vector<vector<bool>> &Mat,
                               vector<int> &pivot) {
  std::default_random_engine engine;
  engine.seed(random_seed_from_clock());
  auto bool_gen = std::bind(std::uniform_int_distribution<>(0, 1), engine);
  int pivot_index = 0;

  // matrix generation
  Mat.resize(nPO - nPI);
  for (int i = 0; i < nPO - nPI; ++i) {
    Mat[i].resize(nPO + 1, 0);
    for (int j = 0; j < nPO; ++j) {
      if (j == pivot_index) {
        pivot.push_back(pivot_index);
        Mat[i][j] = 1;
      } else if (j >= (nPO - nPI)) {
        Mat[i][j] = bool_gen();
      } else {
        Mat[i][j] = 0;
      }
    }
    Mat[i][nPO] = bool_gen();
    pivot_index++;
  }
  return;
}

void SampleCircuit::rndXORGen(int nPI, int nPO, vector<vector<bool>> &Mat,
                              vector<int> &pivot) {
  int pos, nXOR;
  std::default_random_engine engine;
  engine.seed(random_seed_from_clock());
  std::mt19937 gen(engine());
  std::uniform_int_distribution<> bool_gen(0, 1);
  std::uniform_int_distribution<> count_gen(0, nPO - 1);

  // for shuffle
  vector<int> seq(nPO);
  for (int i = 0; i < nPO; i++)
    seq[i] = i;

  // generate XOR constraints
  for (int i = 0; i < nPO - nPI; i++) {
    Mat[i].resize(nPO + 1, 0);
    for (int j = 0; j < nPO; ++j) {
      Mat[i][j] = bool_gen(gen);
    }
    Mat[i][nPO] = bool_gen(gen);
    // NOTE: Old Matrix Generator
    // nXOR = count_gen(gen) + 1;
    // Mat[i].resize(nPO + 1, 0);
    // for (int j = 0; j < nPO; j++) {
    //   pos = count_gen(gen);
    //   if (j != pos)
    //     SWAP(seq[j], seq[pos]);
    // }
    // for (int j = 0; j < nXOR; j++)
    //   Mat[i][seq[j]] = 1;
  }

  // Gaussian Elimination
  pivot.clear();
  int row = 0;
  for (int i = 0; i < nPO && row < nPO - nPI; i++) {
    // find pivot
    if (!Mat[row][i]) {
      for (int j = row + 1; j < nPO - nPI; j++) {
        // swap
        if (Mat[j][i]) {
          Mat[row].swap(Mat[j]);
          break;
        }
      }
      if (!Mat[row][i])
        continue;
    }

    // reduction
    for (int j = 0; j < nPO - nPI; j++) {
      if (Mat[j][i] && j != row) {
        for (int k = i; k <= nPO; k++)
          Mat[j][k] = Mat[j][k] ^ Mat[row][k];
      }
    }
    row += 1;
    pivot.push_back(i);
  }

  return;
}

void SampleCircuit::gaussianEliminate(int nRow, int nCol,
                                      vector<vector<bool>> &Mat,
                                      vector<int> &pivot) {
  pivot.clear();
  int row = 0;
  for (int i = 0; i < nCol && row < nRow; i++) {
    // find pivot
    if (!Mat[row][i]) {
      for (int j = row + 1; j < nRow; j++) {
        // swap
        if (Mat[j][i]) {
          Mat[row].swap(Mat[j]);
          break;
        }
      }
      if (!Mat[row][i])
        continue;
    }

    // reduction
    for (int j = 0; j < nRow; j++) {
      if (Mat[j][i] && j != row) {
        for (int k = i; k <= nCol; k++)
          Mat[j][k] = Mat[j][k] ^ Mat[row][k];
      }
    }
    row += 1;
    pivot.push_back(i);
  }
}

void SampleCircuit::printXOR(const vector<vector<int>> XOR) {
  cout << "XOR constraints:\n";
  for (int i = 0; i < nPO; i++) {
    vector<int> vec = XOR[i];
    cout << "\tPO" << i << " = ";
    for (vector<int>::iterator iter = vec.begin(); iter != vec.end(); iter++) {
      if (iter == vec.begin()) {
        if (vec.size() == 1)
          cout << *iter;
        else if (*iter == 1)
          cout << "1+";
        continue;
      }
      cout << "PI" << *iter;
      if (iter + 1 != vec.end())
        cout << "+";
    }
    cout << "\n";
  }

  return;
}

bool SampleCircuit::checkAvailable(int nRow, int nCol,
                                   vector<vector<bool>> &Mat) {
  // check last row
  for (int i = 0; i < nCol; i++)
    if (Mat[nRow - 1][i])
      return true;
  // cout << "unavailable constraint!!!\n" << endl;
  return false;
}
