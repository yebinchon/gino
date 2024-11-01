#ifndef __MEMSPEC_HPP__
#define __MEMSPEC_HPP__

#include "arcana/noelle/core/Noelle.hpp"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "Namer.hpp"
#include "arcana/noelle/core/DGBase.hpp"
#include "arcana/noelle/core/LoopStructure.hpp"

namespace arcana::gino {

struct MemorySpeculationOracle : public noelle::DependenceAnalysis {
  static char ID;

  using Dependence = noelle::DGEdge<llvm::Value, llvm::Value>;

  MemorySpeculationOracle(noelle::Noelle &noelle, Module &M);
  ~MemorySpeculationOracle();

  void openSpecFile();
  void createNamerMap(Module &M);

  static bool profileExists() {
    return fileOpened;
  }

  bool canThereBeAMemoryDataDependence(Instruction *src,
                                       Instruction *dst,
                                       noelle::LoopStructure &loop);

  bool canThisDependenceBeLoopCarried(noelle::DGEdge<Value, Value> *dep,
                                      noelle::LoopStructure &loop);

  template <class T>
  T *getTwithId(unsigned int id, std::map<int, T *> m) {
    if (m.count(id) == 0) {
      return nullptr;
    } else {
      return m[id];
    }
  }

  Instruction *getInstructionWithID(unsigned int id) {
    return getTwithId(id, instMap);
  }

  Function *getFuncWithID(unsigned int id) {
    return getTwithId(id, functionMap);
  }

  BasicBlock *getBBWithID(unsigned int id) {
    return getTwithId(id, bbMap);
  }

private:
  noelle::Noelle &noelle;

  std::map<int, Function *> functionMap;
  std::map<int, BasicBlock *> bbMap;
  std::map<int, Instruction *> instMap;

  inline static bool fileOpened = false;

  class DepEdge {
  public:
    DepEdge(uint32_t s, uint32_t d, uint32_t c) : src(s), dst(d), cross(c) {}

    uint32_t src;
    uint32_t dst;
    uint32_t cross;
  };

  struct DepEdgeComp {
    bool operator()(const DepEdge &e1, const DepEdge &e2) const {
      if (e1.src < e2.src)
        return true;
      else if (e1.src > e2.src)
        return false;

      if (e1.dst < e2.dst)
        return true;
      else if (e1.dst > e2.dst)
        return false;

      return e1.cross < e2.cross;
    }
  };
  // TODO: if DepEdgeComp can be used outside this context, delete DepEdgeMap
  using DepEdgeMap = std::map<DepEdge, bool, DepEdgeComp>;
  std::map<uint32_t, DepEdgeMap> edges;
};

} // namespace arcana::gino
#endif // __MEMSPEC_HPP__
