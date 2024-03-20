#include "Namer.hpp"
#include <string>

static std::string NamerMeta = "namer";

enum NamerMetaID {
  FCN_ID = 0,
  BB_ID = 1,
  INST_ID = 2,
};

Namer::Namer() {}

Namer::~Namer() {}

Value *getIdValue(const Instruction *I, NamerMetaID id) {
  if (I == nullptr)
    return nullptr;

  MDNode *md = I->getMetadata(NamerMeta);
  if (md == nullptr)
    return nullptr;
  ValueAsMetadata *vsm = dyn_cast<ValueAsMetadata>(md->getOperand(id));
  auto *vFB = cast<ConstantInt>(vsm->getValue());
  const int f_v = vFB->getSExtValue();
  return ConstantInt::get(vFB->getType(), f_v);
}

Value *Namer::getFuncIdValue(Instruction *I) { return getIdValue(I, FCN_ID); }

Value *Namer::getBlkIdValue(Instruction *I) { return getIdValue(I, BB_ID); }

Value *Namer::getInstrIdValue(Instruction *I) {
  return getInstrIdValue((const Instruction *)I);
}

Value *Namer::getInstrIdValue(const Instruction *I) {
  return getIdValue(I, INST_ID);
}

int Namer::getFuncId(Function *F) {
  if (!F || F->isDeclaration()) {
    return -1;
  }

  if (F->getInstructionCount() == 0) {
    return -1;
  }

  // FIXME: the entry block could also be added
  auto &bb = F->getEntryBlock();

  for (auto &I : bb) {
    auto id = getFuncId(&I);
    if (id != -1) {
      return id;
    }
  }

  return -1;
}

int Namer::getFuncId(Instruction *I) {
  Value *v = getFuncIdValue(I);
  if (v == NULL)
    return -1;
  ConstantInt *cv = (ConstantInt *)v;
  return (int)cv->getSExtValue();
}

int Namer::getBlkId(BasicBlock *B) {
  if (!B) {
    return -1;
  }

  for (auto &I : *B) {
    auto id = getBlkId(&I);
    if (id != -1) {
      return id;
    }
  }
  return -1;
}

int Namer::getBlkId(Instruction *I) {
  Value *v = getBlkIdValue(I);
  if (v == NULL) {
    return -1;
  }
  ConstantInt *cv = (ConstantInt *)v;
  auto blkId = cv->getSExtValue();
  auto blkIdInt = (int)blkId;
  return blkIdInt;
}

int Namer::getInstrId(Instruction *I) {
  Value *v = getInstrIdValue(I);
  if (v == NULL)
    return -1;
  ConstantInt *cv = (ConstantInt *)v;
  return (int)cv->getSExtValue();
}

int Namer::getInstrId(const Instruction *I) {
  Value *v = getInstrIdValue(I);
  if (v == NULL)
    return -1;
  ConstantInt *cv = (ConstantInt *)v;
  return (int)cv->getSExtValue();
}