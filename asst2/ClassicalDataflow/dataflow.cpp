// 15-745 S14 Assignment 2: dataflow.cpp
// Group: bhumbers, psuresh
////////////////////////////////////////////////////////////////////////////////

#include "dataflow.h"

#include "llvm/Support/raw_ostream.h"

#include "llvm/Support/CFG.h"

namespace llvm {

std::string bitVectorToString(const BitVector& bv) {
  std::string str(bv.size(), '0');
  for (int i = 0; i < bv.size(); i++)
    str[i] = bv[i] ? '1' : '0';
  return str;
}

DenseMap<BasicBlock*, DataFlowResultForBlock> DataFlow::run(Function& F,
                                                            std::vector<Value*> domain,
                                                            Direction direction,
                                                            BitVector boundaryCond,
                                                            BitVector initInteriorCond) {
  DenseMap<BasicBlock*, DataFlowResultForBlock> results;

  //TODO: Would it make sense to use a linear vector of blocks and use "basic block indices" rather
  //than a map of BasicBlock pointers in order to do block references for dataflow?
  //Might be cleaner and/or faster... depends on how efficient DenseMap is.

  bool analysisConverged = false;

  //Create mapping from domain entries to linear indices
  //(simplifies updating bitvector entries given a particular domain element)
  DenseMap<Value*, int> domainEntryToValueIdx;
  for (int i = 0; i < domain.size(); i++)
    domainEntryToValueIdx[domain[i]] = i;

  //Set initial val for boundary block (depends on direction of analysis)
  DataFlowResultForBlock boundaryResult = DataFlowResultForBlock();
  BasicBlock* boundaryBlock = (direction == FORWARD) ? &F.front() : &F.back();                //fwd boundary = entry, bwd boundary = exit
  BitVector* boundaryVal = (direction == FORWARD) ? &boundaryResult.out : &boundaryResult.in; //set either the "OUT" of entry or the "IN" of exit
  *boundaryVal = boundaryCond;
  boundaryResult.currTransferResult.baseValue = boundaryCond;
  results[boundaryBlock] = boundaryResult;

  //Set initial vals for interior blocks (either OUTs for fwd analysis or INs for bwd analysis)
  for (Function::iterator basicBlock = F.begin(); basicBlock != F.end(); ++basicBlock) {
    if ((BasicBlock*)basicBlock != boundaryBlock) {
      DataFlowResultForBlock interiorInitResult = DataFlowResultForBlock();
      BitVector* interiorInitVal = (direction == FORWARD) ? &interiorInitResult.out : &interiorInitResult.in;
      *interiorInitVal = initInteriorCond;
      interiorInitResult.currTransferResult.baseValue = initInteriorCond;
      results[basicBlock] = interiorInitResult;
    }
  }

  //Generate analysis "predecessor" list for each block (depending on direction of analysis)
  //Will be used to drive the meet inputs.
  DenseMap<BasicBlock*, std::vector<BasicBlock*> > analysisPredsByBlock;
  for (Function::iterator basicBlock = F.begin(); basicBlock != F.end(); ++basicBlock) {
      std::vector<BasicBlock*> analysisPreds;
      switch (direction) {
        case FORWARD:
          for (pred_iterator predBlock = pred_begin(basicBlock), E = pred_end(basicBlock); predBlock != E; ++predBlock)
            analysisPreds.push_back(*predBlock);
          break;
        case BACKWARD:
          for (succ_iterator succBlock = succ_begin(basicBlock), E = succ_end(basicBlock); succBlock != E; ++succBlock)
            analysisPreds.push_back(*succBlock);
          break;
      }

      analysisPredsByBlock[basicBlock] = analysisPreds;
  }

  //Iterate over blocks in function until convergence of output sets for all blocks
  while (!analysisConverged) {
    analysisConverged = true; //assume converged until proven otherwise during this iteration

    //TODO: if analysis is backwards, may want to iterate from back-to-front of blocks list
    for (Function::iterator basicBlock = F.begin(); basicBlock != F.end(); ++basicBlock) {
      DataFlowResultForBlock& blockVals = results[basicBlock];

      //Store old output before applying this analysis pass to the block (depends on analysis dir)
      BitVector* oldPassOut = (direction == FORWARD) ? &blockVals.out : &blockVals.in;

      //If any analysis predecessors have outputs ready, apply meet operator to generate updated input set for this block
      BitVector* passIn = (direction == FORWARD) ? &blockVals.in : &blockVals.out;
      std::vector<BasicBlock*> analysisPreds = analysisPredsByBlock[basicBlock];
      std::vector<BitVector> meetInputs;
      //Iterate over analysis predecessors in order to generate meet inputs for this block
      for (std::vector<BasicBlock*>::iterator analysisPred = analysisPreds.begin(); analysisPred < analysisPreds.end(); ++analysisPred) {
        DataFlowResultForBlock& predVals = results[*analysisPred];
        BitVector meetInput = predVals.currTransferResult.baseValue;
        for (std::vector<std::pair<BasicBlock*, BitVector> >::iterator predSpecificValue = predVals.currTransferResult.predSpecificValues.begin();
             predSpecificValue < predVals.currTransferResult.predSpecificValues.end();
             ++predSpecificValue)
        {
          if (predSpecificValue->first == basicBlock)
            meetInput &= predSpecificValue->second;
        }
        meetInputs.push_back(meetInput);
      }
      if (!meetInputs.empty())
        *passIn = applyMeet(meetInputs);

      //Apply transfer function to input set in order to get output set for this iteration
      blockVals.currTransferResult = applyTransfer(*passIn, domainEntryToValueIdx, basicBlock);
      BitVector* passOut = (direction == FORWARD) ? &blockVals.out : &blockVals.in;
      *passOut = blockVals.currTransferResult.baseValue;

      //DEBUGGING
      errs() << "Old passOut: " << bitVectorToString(*oldPassOut) << "\n";
      errs() << "New passOut: " << bitVectorToString(*passOut) << "\n";

      //Update convergence: if the output set for this block has changed, then we've not converged for this iteration
      if (analysisConverged && passOut != oldPassOut)
        analysisConverged = false;
    }
  }

  return results;
}

void DataFlow::PrintInstructionOps(raw_ostream& O, const Instruction* I) {
  O << "\nOps: {";
  if (I != NULL) {
    for (Instruction::const_op_iterator OI = I->op_begin(), OE = I->op_end();
        OI != OE; ++OI) {
      const Value* v = OI->get();
      v->print(O);
      O << ";";
    }
  }
  O << "}\n";
}

void DataFlow::ExampleFunctionPrinter(raw_ostream& O, const Function& F) {
  for (Function::const_iterator FI = F.begin(), FE = F.end(); FI != FE; ++FI) {
    const BasicBlock* block = FI;
    O << block->getName() << ":\n";
    const Value* blockValue = block;
    PrintInstructionOps(O, NULL);
    for (BasicBlock::const_iterator BI = block->begin(), BE = block->end();
        BI != BE; ++BI) {
      BI->print(O);
      PrintInstructionOps(O, &(*BI));
    }
  }
}

}
