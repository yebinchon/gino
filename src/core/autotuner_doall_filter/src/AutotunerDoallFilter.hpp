/*
 * Copyright 2016 - 2021  Angelo Matni, Simone Campanoni
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
#ifndef NOELLE_SRC_TOOLS_AUTOTUNER_DOALL_FILTER_AUTOTUNERDOALLFILTER_H_
#define NOELLE_SRC_TOOLS_AUTOTUNER_DOALL_FILTER_AUTOTUNERDOALLFILTER_H_

#include "noelle/core/Noelle.hpp"
#include "arcana/gino/core/DOALL.hpp"

namespace arcana::gino {

class AutotunerDoallFilter : public ModulePass {
public:
  AutotunerDoallFilter();

  bool doInitialization(Module &M) override;

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  /*
   * Class fields
   */
  static char ID;
};

} // namespace arcana::gino

#endif // NOELLE_SRC_TOOLS_AUTOTUNER_DOALL_FILTER_AUTOTUNERDOALLFILTER_H_
