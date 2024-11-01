#ifndef __NAMER_METADATA__
#define __NAMER_METADATA__

#include "llvm/IR/Instructions.h"

using namespace llvm;

class Namer {
private:
  int funcId;
  int blkId;
  int instrId;

public:
  Namer();
  ~Namer();

  static Value *getFuncIdValue(Instruction *I);
  static Value *getBlkIdValue(Instruction *I);
  static Value *getInstrIdValue(Instruction *I);
  static Value *getInstrIdValue(const Instruction *I);
  static int getFuncId(Function *F);
  static int getFuncId(Instruction *I);
  static int getBlkId(BasicBlock *I);
  static int getBlkId(Instruction *I);
  static int getInstrId(Instruction *I);
  static int getInstrId(const Instruction *I);
};

#endif // __NAMER_METADATA__