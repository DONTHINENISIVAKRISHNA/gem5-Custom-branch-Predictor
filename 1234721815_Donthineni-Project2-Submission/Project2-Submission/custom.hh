#ifndef __CPU_PRED_CUSTOM_HH__
#define __CPU_PRED_CUSTOM_HH__

#include "base/types.hh"
#include "base/sat_counter.hh"
#include "cpu/pred/bpred_unit.hh"
#include "params/CustomBP.hh"

namespace gem5
{

namespace branch_prediction
{

class CustomBP : public BPredUnit
{
  public:
    CustomBP(const CustomBPParams &params);
    ~CustomBP() = default;

    void uncondBranch(ThreadID tid, Addr pc, void * &bpHistory) override;
    void squash(ThreadID tid, void *bpHistory) override;
    bool lookup(ThreadID tid, Addr branchAddr, void * &bpHistory) override;
    void btbUpdate(ThreadID tid, Addr branchAddr, void * &bpHistory) override;
    void update(ThreadID tid, Addr branchAddr, bool taken, void *bpHistory,
                bool squashed, const StaticInstPtr &inst, Addr corrTarget) override;

  private:
    struct BPHistory {
        unsigned globalHistoryReg;
        bool takenPred;
        bool notTakenPred;
        bool finalPred;

        BPHistory()
            : globalHistoryReg(0), takenPred(false), notTakenPred(false), finalPred(false)
        {}
    };

    inline void updateGlobalHistReg(ThreadID tid, bool taken);

    std::vector<unsigned> globalHistoryReg;
    const unsigned globalHistoryBits;
    const unsigned globalPredictorSize;
    const unsigned globalCtrBits;

    std::vector<SatCounter8> takenCounters;
    std::vector<SatCounter8> notTakenCounters;

    const unsigned historyRegisterMask;
    const unsigned globalHistoryMask;
    const unsigned takenThreshold;
    const unsigned notTakenThreshold;
};

} // namespace branch_prediction
} // namespace gem5

#endif // __CPU_PRED_CUSTOM_HH__

