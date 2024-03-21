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

MemorySpeculationOracle::MemorySpeculationOracle(Noelle &noelle, Module &M)
    : DependenceAnalysis("MemorySpeculationOracle"), noelle(noelle) {

  createNamerMap(M);
  openSpecFile();
}

MemorySpeculationOracle::~MemorySpeculationOracle() {}

bool MemorySpeculationOracle::canThereBeAMemoryDataDependence(
    Instruction *src, Instruction *dst, LoopStructure &loop) {

  auto bb = loop.getHeader();
  auto loopid = Namer::getBlkId(bb);
  // loop has been profiled
  if (this->edges.count(loopid)) {
    // FIXME: hack to avoid PROMPT bug where call instruction ids are incorrect
    if (isa<CallInst>(*src) || isa<CallInst>(*dst)) {
      errs() << "   YEBIN: call instruction. Skipping\n";
      return false;
    }
    auto srcid = Namer::getInstrId(src);
    auto dstid = Namer::getInstrId(dst);
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
  auto bb = loop.getHeader();
  auto loopid = Namer::getBlkId(bb);
  // loop has been profiled
  if (this->edges.count(loopid)) {
    auto *src = dep->getSrc();
    auto *dst = dep->getDst();
    // FIXME: hack to avoid PROMPT bug where call instruction ids are incorrect
    if (isa<CallInst>(*src) || isa<CallInst>(*dst)) {
      errs() << "   YEBIN: call instruction. Skipping\n";
      return false;
    }
    auto srcid = Namer::getInstrId(dyn_cast<Instruction>(src));
    auto dstid = Namer::getInstrId(dyn_cast<Instruction>(dst));
    if (this->edges[loopid].count(DepEdge(srcid, dstid, 1))) {
      errs() << "   YEBIN: found loop-carried edge\n";
      return true;
    }
    errs() << "   YEBIN: nonexistent loop-carried edge in profiling\n";
    return false;
  }
  return true;
}

void MemorySpeculationOracle::createNamerMap(Module &M) {
  for (auto &F : M) {
    int id = Namer::getFuncId(&F);
    errs() << "YEBIN: function " << F.getName() << ": " << id << "\n";
    if (id != -1)
      functionMap[id] = &F;
    for (auto &BB : F) {
      int id = Namer::getBlkId(&BB);
      errs() << "YEBIN: bb " << BB.getName() << ": " << id << "\n";
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
    // auto baredst = string_to<uint32_t>(tokens[2]);
    auto dst = string_to<uint32_t>(tokens[3]);
    auto islc = string_to<uint32_t>(tokens[4]);
    // auto count = string_to<uint32_t>(tokens[5]);

    if (src == 0 && dst == 0) {
      errs() << "Counting loop " << loopid << "\n";
    } else {
      errs() << "Dep from " << src << " to " << dst << "\n";
      BasicBlock *header = getBBWithID(loopid);
      assert(header);
      // TODO: insert check to see if the loop is in the function
    }
    DepEdge edge(src, dst, islc);
    edges[loopid][edge] = true;
  }
}

} // namespace arcana::gino