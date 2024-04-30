/*
 * Copyright 2023  Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do
 so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "Parallelizer.hpp"

namespace arcana::gino {

static std::string getSCCTypeName(GenericSCC::SCCKind type) {
  switch (type) {
  case GenericSCC::LOOP_CARRIED:
    return "LOOP_CARRIED";
  case GenericSCC::REDUCTION:
    return "REDUCTION";
  case GenericSCC::BINARY_REDUCTION:
    return "BINARY_REDUCTION";
  case GenericSCC::LAST_REDUCTION:
    return "LAST_REDUCTION";
  case GenericSCC::RECOMPUTABLE:
    return "RECOMPUTABLE";
  case GenericSCC::SINGLE_ACCUMULATOR_RECOMPUTABLE:
    return "SINGLE_ACCUMULATOR_RECOMPUTABLE";
  case GenericSCC::INDUCTION_VARIABLE:
    return "INDUCTION_VARIABLE";
  case GenericSCC::LINEAR_INDUCTION_VARIABLE:
    return "LINEAR_INDUCTION_VARIABLE";
  case GenericSCC::LAST_INDUCTION_VARIABLE:
    return "LAST_INDUCTION_VARIABLE";
  case GenericSCC::PERIODIC_VARIABLE:
    return "PERIODIC_VARIABLE";
  case GenericSCC::LAST_SINGLE_ACCUMULATOR_RECOMPUTABLE:
    return "LAST_SINGLE_ACCUMULATOR_RECOMPUTABLE";
  case GenericSCC::UNKNOWN_CLOSED_FORM:
    return "UNKNOWN_CLOSED_FORM";
  case GenericSCC::LAST_RECOMPUTABLE:
    return "LAST_RECOMPUTABLE";
  case GenericSCC::MEMORY_CLONABLE:
    return "MEMORY_CLONABLE";
  case GenericSCC::STACK_OBJECT_CLONABLE:
    return "STACK_OBJECT_CLONABLE";
  case GenericSCC::LAST_MEMORY_CLONABLE:
    return "LAST_MEMORY_CLONABLE";
  case GenericSCC::LOOP_CARRIED_UNKNOWN:
    return "LOOP_CARRIED_UNKNOWN";
  case GenericSCC::LAST_LOOP_CARRIED:
    return "LAST_LOOP_CARRIED";
  case GenericSCC::LOOP_ITERATION:
    return "LOOP_ITERATION";
  case GenericSCC::LAST_LOOP_ITERATION:
    return "LAST_LOOP_ITERATION";
  default:
    assert(false);
  }
}

bool Parallelizer::parallelizeLoops(Noelle &noelle, Heuristics *heuristics,
                                    bool readProfile) {

  /*
   * Fetch the verbosity level.
   */
  auto verbosity = noelle.getVerbosity();

  /*
   * Collect information about C++ code we link parallelized loops with.
   */
  auto M = noelle.getProgram();
  errs() << "Parallelizer:  Analyzing the module " << M->getName() << "\n";
  if (!this->collectThreadPoolHelperFunctionsAndTypes(*M, noelle)) {
    errs() << "Parallelizer:    ERROR: I could not find the runtime within the "
              "module\n";
    return false;
  }

  /*
   * Fetch all the loops we want to parallelize.
   */
  errs() << "Parallelizer:  Fetching the program loops\n";
  auto forest = noelle.getLoopNestingForest();
  if (forest->getNumberOfLoops() == 0) {
    errs() << "Parallelizer:    There is no loop to consider\n";

    /*
     * Free the memory.
     */
    delete forest;

    errs() << "Parallelizer: Exit\n";
    return false;
  }
  errs() << "Parallelizer:    There are " << forest->getNumberOfLoops()
         << " loops in the program that are enabled from the options used\n";

  const auto isSelected = [&](int i) {
    const auto &WL = this->loopIndexesWhiteList;
    const auto &BL = this->loopIndexesBlackList;
    /*
     * If no index has been specified we will select them all
     */
    if (!WL.empty()) {
      return std::find(WL.begin(), WL.end(), i) != std::end(WL);
    }
    if (!BL.empty()) {
      return std::find(BL.begin(), BL.end(), i) == std::end(BL);
    }
    return true;
  };

  /*
   * Determine the parallelization order from the metadata.
   */
  auto mm = noelle.getMetadataManager();
  std::map<uint32_t, LoopContent *> loopParallelizationOrder;
  for (auto tree : forest->getTrees()) {
    auto selector = [&noelle, &mm, &loopParallelizationOrder,
                     &isSelected](LoopTree *n, uint32_t treeLevel) -> bool {
      auto ls = n->getLoop();
      if (!mm->doesHaveMetadata(ls, "noelle.parallelizer.looporder")) {
        return false;
      }
      auto parallelizationOrderIndex =
          std::stoi(mm->getMetadata(ls, "noelle.parallelizer.looporder"));
      if (!isSelected(parallelizationOrderIndex)) {
        return false;
      }
      auto optimizations = {LoopContentOptimization::MEMORY_CLONING_ID,
                            LoopContentOptimization::THREAD_SAFE_LIBRARY_ID};
      auto ldi = noelle.getLoopContent(ls, optimizations);
      loopParallelizationOrder[parallelizationOrderIndex] = ldi;
      return false;
    };
    tree->visitPreOrder(selector);
  }
  errs() << "Parallelizer:    Selected loops with index: ";
  for (const auto &[k, v] : loopParallelizationOrder) {
    errs() << k << " ";
  }
  errs() << "\n";

  /*
   * Parallelize the loops in order.
   */
  auto modified = false;
  std::unordered_map<BasicBlock *, bool> modifiedBBs{};
  std::unordered_set<Function *> modifiedFunctions;
  for (auto indexLoopPair : loopParallelizationOrder) {
    auto ldi = indexLoopPair.second;

    /*
     * Check if we can parallelize this loop.
     */
    auto ls = ldi->getLoopStructure();
    auto safe = true;
    for (auto bb : ls->getBasicBlocks()) {
      if (modifiedBBs[bb]) {
        safe = false;
        break;
      }
    }

    if (!readProfile) {
      errs() << "PROMPT TARGETS: " << ls->getFunction()->getName() << " "
             << ls->getHeader()->getName() << "\n";
    }

    // TODO: move to separate function?
    bool print = false; // ls->getHeader()->getName() == "for.cond1.i3" ? 1 : 0;
    auto ldg = ldi->getLoopDG();
    auto deps = ldg->getSortedDependences();
    int num_deps = 0;
    int num_lc_deps = 0;

    auto insts_in_loop = ls->getInstructions();
    for (auto dep : deps) {
      auto srcinst = dyn_cast<Instruction>(dep->getSrc());
      auto dstinst = dyn_cast<Instruction>(dep->getDst());
      if (insts_in_loop.count(srcinst) && insts_in_loop.count(dstinst)) {
        if (isa<MemoryDependence<Value, Value>>(dep)) {
          num_deps++;
          if (print) {
            errs() << "DEP FROM LOOP " << ls->getHeader()->getName() << ": \n";
            errs() << "     " << *dep->getSrc() << "\n";
            errs() << "     " << *dep->getDst() << "\n";
          }
          if (dep->isLoopCarriedDependence())
            num_lc_deps++;
        }
      }
    }
    errs() << "DEPS IN LOOP " << ls->getFunction()->getName()
           << "::" << ls->getHeader()->getName() << ": " << num_deps << " "
           << num_lc_deps << "\n";

    auto sccManager = ldi->getSCCManager();
    auto SCCNodes = sccManager->getSCCDAG()->getSCCs();
    int id = 0;
    for (auto sccNode : SCCNodes) {
      auto scc = sccManager->getSCCAttrs(sccNode);
      auto type = scc->getKind();

      errs() << "SCCID: " << id << " TypeID: " << type << " = "
             << getSCCTypeName(type) << "\n";
      for (auto *SCCI : sccNode->getInstructions()) {
        errs() << "    " << *SCCI << "\n";
      }
      errs() << "END OF SCC " << id++ << "\n";
    }

    /*
     * Get loop ID.
     */
    auto loopIDOpt = ls->getID();

    if (!safe) {
      errs() << "Parallelizer:    Loop ";
      // Parent loop has been parallelized, so basic blocks have been modified
      // and we might not have a loop ID for the child loop. If we have it we
      // print it, otherwise we don't.
      if (loopIDOpt) {
        auto loopID = loopIDOpt.value();
        errs() << loopID;
      }
      errs() << " cannot be parallelized because one of its parent has been "
                "parallelized already\n";
      continue;
    }

    /*
     * Parallelize the current loop.
     */
    auto loopIsParallelized =
        this->parallelizeLoop(ldi, noelle, heuristics, readProfile);

    /*
     * Keep track of the parallelization.
     */
    if (loopIsParallelized) {
      errs() << "Parallelizer:    Loop ";
      assert(loopIDOpt);
      auto loopID = loopIDOpt.value();
      errs() << loopID;
      errs() << " has been parallelized\n";
      errs() << "Parallelizer:      Keep track of basic blocks being modified "
                "by the parallelization\n";
      modified = true;
      for (auto bb : ls->getBasicBlocks()) {
        modifiedBBs[bb] = true;
      }
      modifiedFunctions.insert(ls->getFunction());
    }
  }

  /*
   * Free the memory.
   */
  for (auto indexLoopPair : loopParallelizationOrder) {
    delete indexLoopPair.second;
  }

  /*
   * Erase calls to intrinsics in modified functions
   */
  std::unordered_set<CallInst *> intrinsicCallsToRemove;
  for (auto F : modifiedFunctions) {
    for (auto &I : *F) {
      if (auto callInst = dyn_cast<CallInst>(&I)) {
        if (callInst->isLifetimeStartOrEnd()) {
          intrinsicCallsToRemove.insert(callInst);
        }
      }
    }
  }
  for (auto call : intrinsicCallsToRemove) {
    call->eraseFromParent();
  }

  errs() << "Parallelizer: Exit\n";
  return modified;
}

} // namespace arcana::gino
