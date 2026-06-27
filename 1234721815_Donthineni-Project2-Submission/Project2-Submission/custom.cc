#include "cpu/pred/custom.hh"

#include "base/bitfield.hh"
#include "base/intmath.hh"

namespace gem5
{

namespace branch_prediction
{

CustomBP::CustomBP(const CustomBPParams &params)
    : BPredUnit(params),
      globalHistoryReg(params.numThreads, 0),
      globalHistoryBits(ceilLog2(params.PredictorSize)),
      globalPredictorSize(params.PredictorSize),
      globalCtrBits(params.PHTCtrBits),
      takenCounters(globalPredictorSize, SatCounter8(globalCtrBits)),
      notTakenCounters(globalPredictorSize, SatCounter8(globalCtrBits)),
      historyRegisterMask(mask(globalHistoryBits)),
      globalHistoryMask(globalPredictorSize - 1),
      takenThreshold((ULL(1) << (globalCtrBits - 1)) - 1),
      notTakenThreshold((ULL(1) << (globalCtrBits - 1)) - 1)
{
    // Verify that the global predictor size is a power of 2 to ensure
    // proper indexing. This is critical for the functioning of the branch
    // predictor and prevents unexpected behavior during lookups.
    if (!isPowerOf2(globalPredictorSize)) {
        fatal("Invalid global history predictor size.\n");
    }
}

void
CustomBP::uncondBranch(ThreadID tid, Addr pc, void * &bpHistory)
{
    // Handle unconditional branch instructions by initializing a new
    // BPHistory instance. It stores the current global history and
    // sets the prediction flags to true, indicating that the branch
    // will be taken regardless of any conditions.
    BPHistory *history = new BPHistory;
    history->globalHistoryReg = globalHistoryReg[tid];
    history->takenPred = true;
    history->notTakenPred = true;
    history->finalPred = true;
    bpHistory = static_cast<void*>(history);
    updateGlobalHistReg(tid, true); // Update the global history register to reflect the branch taken.
}

void
CustomBP::squash(ThreadID tid, void *bpHistory)
{
    // Squash the predictions and revert the global history register
    // to the state stored in bpHistory. This is used when an instruction
    // needs to be invalidated, ensuring that the state reflects the 
    // correct history before the squash.
    BPHistory *history = static_cast<BPHistory*>(bpHistory);
    globalHistoryReg[tid] = history->globalHistoryReg;
    delete history; // Free the memory allocated for BPHistory.
}

bool
CustomBP::lookup(ThreadID tid, Addr branchAddr, void * &bpHistory)
{
    // Perform a lookup in the branch predictor to make a prediction
    // based on the current global history and the branch address.
    // The index is determined using a NAND operation between the branch
    // address bits and the global history register. The prediction 
    // is based on the values of taken and not-taken counters.
    unsigned nandIndex = ~((branchAddr >> instShiftAmt) & globalHistoryReg[tid]) & globalHistoryMask;

    assert(nandIndex < globalPredictorSize);

    // Check the counter values to determine predictions for taken and not-taken
    bool takenGHBPrediction = takenCounters[nandIndex] > takenThreshold;
    bool notTakenGHBPrediction = notTakenCounters[nandIndex] > notTakenThreshold;

    // The final prediction is true if the taken prediction is true 
    // and not-taken prediction is false.
    bool finalPrediction = takenGHBPrediction && !notTakenGHBPrediction;

    BPHistory *history = new BPHistory;
    history->globalHistoryReg = globalHistoryReg[tid];
    history->takenPred = takenGHBPrediction;
    history->notTakenPred = notTakenGHBPrediction;
    history->finalPred = finalPrediction;
    bpHistory = static_cast<void*>(history);
    updateGlobalHistReg(tid, finalPrediction); // Update the history based on the prediction outcome.

    return finalPrediction; // Return the final prediction result.
}

void
CustomBP::btbUpdate(ThreadID tid, Addr branchAddr, void * &bpHistory)
{
    // Update the branch target buffer (BTB) state, clearing the least
    // significant bit of the global history register. This operation
    // ensures that the last bit of history is discarded, maintaining
    // a compact representation of the history.
    globalHistoryReg[tid] &= (historyRegisterMask & ~ULL(1));
}

void
CustomBP::update(ThreadID tid, Addr branchAddr, bool taken, void *bpHistory,
                 bool squashed, const StaticInstPtr &inst, Addr corrTarget)
{
    // Update the predictor state based on the outcome of the branch
    // instruction. If the instruction is squashed, the global history
    // register is updated to reflect the stored state in bpHistory.
    // Otherwise, the predictor counters are adjusted based on the
    // actual outcome versus the predicted outcome.
    assert(bpHistory);
    BPHistory *history = static_cast<BPHistory*>(bpHistory);

    if (squashed) {
        globalHistoryReg[tid] = (history->globalHistoryReg << 1) | taken; // Update history for squashed state.
        return;
    }

    unsigned nandIndex = ~((branchAddr >> instShiftAmt) & history->globalHistoryReg) & globalHistoryMask;

    assert(nandIndex < globalPredictorSize);

    // Adjust taken and not-taken counters based on the prediction accuracy.
    if (history->finalPred == taken) {
        if (history->takenPred) {
            // If the prediction was taken and the actual outcome was taken
            // or not taken, update the corresponding counter.
            if (taken) {
                takenCounters[nandIndex]++;
            } else {
                takenCounters[nandIndex]--;
            }
        } else {
            if (taken) {
                notTakenCounters[nandIndex]++;
            } else {
                notTakenCounters[nandIndex]--;
            }
        }
    } else {
        // If the prediction was incorrect, increment the opposite counter.
        if (taken) {
            notTakenCounters[nandIndex]++;
        } else {
            takenCounters[nandIndex]--;
        }
    }

    delete history; // Free the memory allocated for BPHistory.
}

void
CustomBP::updateGlobalHistReg(ThreadID tid, bool taken)
{
    // Update the global history register based on whether the last
    // branch was taken or not. The history is shifted left by one bit,
    // and the least significant bit is set to the outcome of the last
    // branch. The history is then masked to ensure it fits within the
    // defined history register size.
    globalHistoryReg[tid] = taken ? (globalHistoryReg[tid] << 1) | 1 :
                               (globalHistoryReg[tid] << 1);
    globalHistoryReg[tid] &= historyRegisterMask; // Ensure history stays within bounds.
}

} // namespace branch_prediction
} // namespace gem5

