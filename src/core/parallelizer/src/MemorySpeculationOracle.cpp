#include "MemSpec.hpp"
#include "Namer.hpp"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <sstream>

#define MEMSPEC_FILE "result.slamp.profile"

using namespace arcana::noelle;

namespace arcana::gino {

template <class T> static T string_to(std::string s) {
  T ret;
  std::stringstream ss(s);
  ss >> ret;

  if (!ss) {
    assert(false && "Failed to convert string to given type\n");
  }

  return ret;
}

static size_t split(const std::string s, std::vector<std::string> &tokens,
                    char delim) {
  tokens.clear();

  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, delim)) {
    tokens.push_back(token);
  }
  return tokens.size();
}

static bool isExternalCall(const Instruction *I) {
  auto callInst = dyn_cast<CallInst>(I);
  if (!callInst)
    return false;

  auto calledFunction = callInst->getCalledFunction();

  return calledFunction->isDeclaration();
}

MemorySpeculationOracle::MemorySpeculationOracle(Noelle &noelle, Module &M)
    : DependenceAnalysis("MemorySpeculationOracle"), noelle(noelle) {

  createNamerMap(M);
  openSpecFile();
}

MemorySpeculationOracle::~MemorySpeculationOracle() {}

bool MemorySpeculationOracle::canThereBeAMemoryDataDependence(
    Instruction *src, Instruction *dst, LoopStructure &loop) {
  if (!fileOpened)
    return true;

  auto bb = loop.getHeader();
  auto loopid = Namer::getBlkId(bb);
  // return false;
  // loop has been profiled
  if (this->edges.count(loopid)) {
    // TODO: once PROMPT is able to handle external calls, don't have to check
    if (isExternalCall(src) || isExternalCall(dst))
      return true;

    auto srcid = Namer::getInstrId(src);
    auto dstid = Namer::getInstrId(dst);

    // Check if both instructions are part of the loop
    auto insts_in_loop = loop.getInstructions();
    if (!insts_in_loop.count(src) || !insts_in_loop.count(dst))
      return true;

    errs() << "YEBIN: asking for profiled loop " << loopid << ": " << srcid
           << " -> " << dstid << "\n";
    if (this->edges[loopid].count(DepEdge(srcid, dstid, 0)) ||
        this->edges[loopid].count(DepEdge(srcid, dstid, 1))) {
      errs() << "   YEBIN: found profiled edge\n";
      return true;
    }
    errs() << "   YEBIN: nonexistent edge in profiling\n";
    return false;
  }
  return true;
}

bool MemorySpeculationOracle::canThisDependenceBeLoopCarried(
    noelle::DGEdge<Value, Value> *dep, noelle::LoopStructure &loop) {
  if (!fileOpened)
    return true;

  if (!isa<MemoryDependence<Value, Value>>(dep))
    return true;

  auto bb = loop.getHeader();
  auto loopid = Namer::getBlkId(bb);
  // loop has been profiled
  if (this->edges.count(loopid)) {
    auto *src = dep->getSrc();
    auto *dst = dep->getDst();

    auto *srcInst = dyn_cast<Instruction>(src);
    auto *dstInst = dyn_cast<Instruction>(dst);
    // FIXME: PROMPT can only handle deps between instructions
    //        can iterate through insts in an SCC
    if (!srcInst || !dstInst) {
      return true;
    }
    // Check if both instructions are part of the loop
    auto insts_in_loop = loop.getInstructions();
    if (!insts_in_loop.count(srcInst) || !insts_in_loop.count(dstInst))
      return true;

    auto srcid = Namer::getInstrId(srcInst);
    auto dstid = Namer::getInstrId(dstInst);

    // TODO: once PROMPT is able to handle external calls, don't have to check
    if (isExternalCall(srcInst) || isExternalCall(dstInst))
      return true;

    errs() << "YEBIN: asking for profiled loop " << loopid << ": " << srcid
           << " -> " << dstid << "\n";
    if (this->edges[loopid].count(DepEdge(srcid, dstid, 1))) {
      errs() << "   YEBIN: found loop-carried edge\n";
      return true;
    }
    errs() << "   YEBIN: nonexistent loop-carried edge in profiling\n";
    return false;
  }
  return true;
  // return false;
}

void MemorySpeculationOracle::createNamerMap(Module &M) {
  for (auto &F : M) {
    int id = Namer::getFuncId(&F);
    errs() << "YEBIN: function " << id << ": " << F.getName() << "\n";
    if (id != -1)
      functionMap[id] = &F;
    for (auto &BB : F) {
      int id = Namer::getBlkId(&BB);
      errs() << "YEBIN: bb " << id << ": " << BB.getName() << "\n";
      if (id != -1)
        bbMap[id] = &BB;
      for (auto &I : BB) {
        int id = Namer::getInstrId(&I);
        errs() << "YEBIN: inst " << id << ": " << I << "\n";
        if (id != -1)
          instMap[id] = &I;
      }
    }
  }
}

void MemorySpeculationOracle::openSpecFile() {
  std::string memspec_file = MEMSPEC_FILE;
  std::ifstream ifs(memspec_file.c_str());

  if (!ifs.is_open()) {
    errs() << "Error: cannot open file " << MEMSPEC_FILE << "\n";
    fileOpened = false;
  } else {
    fileOpened = true;
  }

  std::string line;
  while (std::getline(ifs, line)) {
    std::vector<std::string> tokens;
    split(line, tokens, ' ');

    auto loopid = string_to<uint32_t>(tokens[0]);
    auto src = string_to<uint32_t>(tokens[1]);
    auto baredst = string_to<uint32_t>(tokens[2]);
    auto dst = string_to<uint32_t>(tokens[3]);
    auto islc = string_to<uint32_t>(tokens[4]);
    // auto count = string_to<uint32_t>(tokens[5]);

    if (src == 0 && baredst == 0) {
      errs() << "Counting loop " << loopid << "\n";
    } else {
      // Make sure we are tracking the instruction in the loop
      if (dst == 0)
        dst = baredst;
      errs() << "Dep from " << src << " to " << dst << "\n";
      BasicBlock *header = getBBWithID(loopid);
      assert(header);
      // TODO: insert check to see if the loop is in the function
    }
    DepEdge edge(src, dst, islc);
    edges[loopid][edge] = true;
  }
  errs() << "PROFILE RESULT:\n";
  for (auto &loop : edges) {
    errs() << "Loop " << loop.first << ":\n";
    for (auto &edge : loop.second) {
      errs() << "  " << edge.first.src << " -> " << edge.first.dst
             << " (lc: " << edge.first.cross << ")\n";
    }
  }
}

} // namespace arcana::gino
