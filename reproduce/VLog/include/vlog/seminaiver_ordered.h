#ifndef _SEMINAIVER_ORDERED_H
#define _SEMINAIVER_ORDERED_H

#include "vlog/reliances/reliances.h"
#include <vlog/seminaiver.h>

#include <vector>
#include <stack>
#include <unordered_set>
#include <deque>
#include <algorithm>
#include <numeric>

enum SemiNaiverOrderedType : int32_t
{
    Default = 0,
    PieceDecomposed = 1,
    Dynamic = 2,
    UnrestrainedFirst = 4
};

class SemiNaiverOrdered: public SemiNaiver {
public:
    struct PositiveGroup;

    struct RelianceRuleInfo
    {
        unsigned id;

        RuleExecutionDetails *ruleDetails;

        bool active = true;
        bool triggered = false;
        unsigned numRestrains = 0;

        PositiveGroup *positiveGroup;
        std::vector<RelianceRuleInfo*> positiveSuccessors, restraintSuccessors;

        bool existentialRule; // contains at least one existential variable in heads
        bool complexRule; // contains at least two body atoms

        RelianceRuleInfo(unsigned id, RuleExecutionDetails *details)
            : id(id), ruleDetails(details)
        {
            for (RelianceRuleInfo *successor : restraintSuccessors)
            {
                ++successor->numRestrains;
            }
        }

        void initialize(PositiveGroup *group,
            std::vector<RelianceRuleInfo*> positiveSuccessors, std::vector<RelianceRuleInfo*> restraintSuccessors)
        {
            this->positiveGroup = group;
            this->positiveSuccessors = positiveSuccessors;
            this->restraintSuccessors = restraintSuccessors;

            for (RelianceRuleInfo *successor : restraintSuccessors)
            {
                ++successor->numRestrains;
            }
        }

        void setTriggered(bool triggered)
        {
            if (this->triggered && !triggered)
            {
                --positiveGroup->numTriggeredRules;
            }
            else if(!this->triggered && triggered)
            {
                ++positiveGroup->numTriggeredRules;
            }

            this->triggered = triggered;
        }

        void setInactive()
        {
            active = false;

            for (RelianceRuleInfo *memberSuccessor : restraintSuccessors)
            {
                --memberSuccessor->numRestrains;
            }
        }
    };

    struct PositiveGroup
    {
        unsigned id;

        bool removed = false;
        unsigned numActivePredecessors = 0;
        unsigned numTriggeredRules = 0;
    
        std::vector<RelianceRuleInfo*> members;
        std::vector<PositiveGroup*> successors;

        bool isActive()
        {
            return numActivePredecessors > 0 || numTriggeredRules > 0;
        }

        void setInactive()
        {
            if (isActive())
            {
                std::cerr << "setInactive on active group?" << std::endl;
                return;
            }

            for (RelianceRuleInfo *memberInfo : members)
            {
                memberInfo->setInactive();
            }
        }
    };

    struct RestrainedGroup
    {
        std::vector<PositiveGroup*> positiveGroups; 
        std::vector<RelianceRuleInfo*> restrainedMembers, unrestrainedMembers;
    };

    VLIBEXP SemiNaiverOrdered(EDBLayer &layer,
        Program *program, 
        bool opt_intersect,
        bool opt_filtering, 
        bool multithreaded,
        TypeChase chase, 
        int nthreads, 
        bool shuffleRules,
        bool ignoreExistentialRule,
        Program *RMFC_check = NULL,
        SemiNaiverOrderedType strategy = SemiNaiverOrderedType::Default);

    VLIBEXP void run(size_t lastIteration,
        size_t iteration,
        unsigned long *timeout = NULL,
        bool checkCyclicTerms = false,
        int singleRule = -1,
        PredId_t predIgnoreBlock = -1);
        
private:
    SemiNaiverOrderedType strategy;

    void setActive(PositiveGroup *currentGroup);
    void sortRuleVectorById(std::vector<RelianceRuleInfo *> &infos);
    std::vector<PositiveGroup> computePositiveGroups(std::vector<RelianceRuleInfo> &allRules, SimpleGraph &positiveGraph, RelianceGroupResult &groupsResult);

    void orderGroupExistentialLast(PositiveGroup *group);
    void orderGroupAverageRuntime(PositiveGroup *group);
    void orderGroupPredicateCount(PositiveGroup *group);
    void orderGroupManually(PositiveGroup *group);

    // void prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules, const SimpleGraph &positiveGraph, const SimpleGraph &positiveGraphTransposed, const SimpleGraph &blockingGraphTransposed, const RelianceGroupResult &groupsResult, std::vector<PositiveGroup> &positiveGroups);
    void prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules, std::vector<RelianceRuleInfo> &outInfo, std::vector<RuleExecutionDetails> &outRuleDetails);

    void updateGraph(SimpleGraph &graph, SimpleGraph &graphTransposed, PositiveGroup *group, unsigned *numActiveGroups, std::vector<bool> &activeRules);
    RestrainedGroup computeRestrainedGroup(std::vector<RelianceRuleInfo> &allRules, const std::vector<unsigned> &groupIndices);

    bool executeGroup(std::vector<RuleExecutionDetails> &ruleset, std::vector<StatIteration> &costRules, bool fixpoint, unsigned long *timeout);
    // bool executeGroupBottomUp(std::vector<RuleExecutionDetails> &ruleset, std::vector<unsigned> &rulesetOrder, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
    // bool executeGroupInOrder(std::vector<RuleExecutionDetails> &ruleset, std::vector<unsigned> &rulesetOrder, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
    // bool executeGroupAverageRuntime(std::vector<RuleExecutionDetails> &ruleset, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
    PositiveGroup *executeGroupUnrestrainedFirst(RestrainedGroup &group, std::vector<StatIteration> &costRules, unsigned long *timeout, SemiNaiverOrderedType strategy);
    PositiveGroup *executeGroupByPositiveGroups(RestrainedGroup &group, std::vector<StatIteration> &costRules, unsigned long *timeout);
};

#endif
