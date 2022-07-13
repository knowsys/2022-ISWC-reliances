#include "vlog/seminaiver_ordered.h"

#include <iostream>

SemiNaiverOrdered::SemiNaiverOrdered(EDBLayer &layer,
    Program *program, 
    bool opt_intersect,
    bool opt_filtering, 
    bool multithreaded,
    TypeChase chase, 
    int nthreads, 
    bool shuffleRules,
    bool ignoreExistentialRule,
    Program *RMFC_check,
    SemiNaiverOrderedType strategy) 
        : SemiNaiver(layer,
                    program, opt_intersect, opt_filtering, multithreaded, chase,
                    nthreads, shuffleRules, ignoreExistentialRule, RMFC_check)
{
    this->strategy = strategy;
    // std::cout << "Running constructor of SemiNaiverOrdered" << '\n';
}

void SemiNaiverOrdered::prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules,
    std::vector<RelianceRuleInfo> &outInfo,
    std::vector<RuleExecutionDetails> &outRuleDetails
)
{
    outInfo.reserve(allRules.size());
    outRuleDetails.reserve(allRules.size());

    for (unsigned ruleIndex = 0; ruleIndex < allRules.size(); ++ruleIndex)
    {
        const Rule &currentRule = allRules[ruleIndex];

        outRuleDetails.emplace_back(currentRule, currentRule.getId());

        outRuleDetails.back().createExecutionPlans(checkCyclicTerms);
        outRuleDetails.back().calculateNVarsInHeadFromEDB();
        outRuleDetails.back().lastExecution = lastExecution;   

        outInfo.emplace_back(ruleIndex, &outRuleDetails.back());
    }

    chaseMgmt = std::shared_ptr<ChaseMgmt>(new ChaseMgmt(outRuleDetails,
        typeChase, checkCyclicTerms,
        singleRuleToCheck,
        predIgnoreBlock));
}

SemiNaiverOrdered::PositiveGroup *SemiNaiverOrdered::executeGroupUnrestrainedFirst(
    RestrainedGroup &group, 
    std::vector<StatIteration> &costRules, unsigned long *timeout,
    SemiNaiverOrderedType strategy
)
{
    struct PriorityGroup
    {
        std::vector<RelianceRuleInfo*> &members;
        size_t currentRuleIndex = 0;
        size_t numWithoutDerivations = 0;
        bool containsTriggeredRule = false;

        PriorityGroup(std::vector<RelianceRuleInfo *> &infos)
            : members(infos)
        {
            for (const RelianceRuleInfo* member: members)
            {
                if (member->triggered)
                {
                    containsTriggeredRule= true;
                    break;
                }
            } 
        }
    } restrainedRules(group.restrainedMembers), unrestrainedRules(group.unrestrainedMembers);

    std::chrono::system_clock::time_point executionStart = std::chrono::system_clock::now();
    while (restrainedRules.containsTriggeredRule || unrestrainedRules.containsTriggeredRule)
    {
        PriorityGroup &currentGroup = (unrestrainedRules.containsTriggeredRule) ? unrestrainedRules : restrainedRules;
        RelianceRuleInfo &currentRuleInfo = *currentGroup.members[currentGroup.currentRuleIndex];

        currentGroup.currentRuleIndex = (currentGroup.currentRuleIndex + 1) % currentGroup.members.size();

        if (!currentRuleInfo.triggered)
        {
            ++currentGroup.numWithoutDerivations;

            if (currentGroup.numWithoutDerivations == currentGroup.members.size())
            {
                currentGroup.containsTriggeredRule = false;
            }

            continue;
        }

        std::chrono::system_clock::time_point iterationStart = std::chrono::system_clock::now();
        bool response = executeRule(*currentRuleInfo.ruleDetails, iteration, 0, NULL);

        if (timeout != NULL && *timeout != 0) 
        {
            std::chrono::duration<double> runDuration = std::chrono::system_clock::now() - startTime;
            if (runDuration.count() > *timeout) {
                *timeout = 0;   // To indicate materialization was stopped because of timeout.
                return nullptr;
            }
        }
        std::chrono::duration<double> iterationDuration = std::chrono::system_clock::now() - iterationStart;
        StatIteration stat;
        stat.iteration = iteration;
        stat.rule = &currentRuleInfo.ruleDetails->rule;
        stat.time = iterationDuration.count() * 1000;
        stat.derived = response;
        costRules.push_back(stat);

        currentRuleInfo.ruleDetails->executionTime += (double)iterationDuration.count();
        currentRuleInfo.ruleDetails->lastExecution = iteration;
        iteration++;

        currentRuleInfo.setTriggered(false);

        if (response)
        {
            for (RelianceRuleInfo *successor : currentRuleInfo.positiveSuccessors)
            {
                successor->setTriggered(true);

                if (successor->positiveGroup->id == currentRuleInfo.positiveGroup->id)
                {
                    if (successor->numRestrains == 0)
                    {
                        unrestrainedRules.containsTriggeredRule = true;
                        unrestrainedRules.numWithoutDerivations = 0;
                        unrestrainedRules.currentRuleIndex = 0;
                    }
                    else
                    {
                        restrainedRules.containsTriggeredRule = true;
                        restrainedRules.numWithoutDerivations = 0;
                        restrainedRules.currentRuleIndex = 0;
                    }
                }
            }
        }

        if (!currentRuleInfo.positiveGroup->isActive() && (strategy & SemiNaiverOrderedType::Dynamic > 0))
        {
            return currentRuleInfo.positiveGroup;
        }
    }

    return nullptr;
}

SemiNaiverOrdered::PositiveGroup *SemiNaiverOrdered::executeGroupByPositiveGroups(
    RestrainedGroup &group, 
    std::vector<StatIteration> &costRules, unsigned long *timeout
)
{   
    for (PositiveGroup *currentGroup : group.positiveGroups)
    {
        if (currentGroup->numTriggeredRules == 0 || currentGroup->numActivePredecessors > 0)
            continue;

        size_t currentRuleIndex = 0; 
        size_t numWithoutDerivations = 0;

        do 
        {
            RelianceRuleInfo &currentRuleInfo = *currentGroup->members[currentRuleIndex];
            currentRuleIndex = (currentRuleIndex + 1) % currentGroup->members.size();

            std::chrono::system_clock::time_point iterationStart = std::chrono::system_clock::now();
            bool response = executeRule(*currentRuleInfo.ruleDetails, iteration, 0, NULL);

            if (timeout != NULL && *timeout != 0) 
            {
                std::chrono::duration<double> runDuration = std::chrono::system_clock::now() - startTime;
                if (runDuration.count() > *timeout) {
                    *timeout = 0;   // To indicate materialization was stopped because of timeout.
                    return nullptr;
                }
            }

            std::chrono::duration<double> iterationDuration = std::chrono::system_clock::now() - iterationStart;
            StatIteration stat;
            stat.iteration = iteration;
            stat.rule = &currentRuleInfo.ruleDetails->rule;
            stat.time = iterationDuration.count() * 1000;
            stat.derived = response;
            costRules.push_back(stat);

            currentRuleInfo.ruleDetails->executionTime += (double)iterationDuration.count();
            currentRuleInfo.ruleDetails->lastExecution = iteration;
            iteration++;

            currentRuleInfo.setTriggered(false);

            if (response)
            {
                numWithoutDerivations = 0;

                for (RelianceRuleInfo *successor : currentRuleInfo.positiveSuccessors)
                {
                    successor->setTriggered(true);
                }
            }
            else
            {
                ++numWithoutDerivations;
            }
        } while (numWithoutDerivations < currentGroup->members.size());

        return currentGroup;
    }   

    return nullptr;
}

void SemiNaiverOrdered::sortRuleVectorById(std::vector<RelianceRuleInfo *> &infos)
{
    auto sortRule = [] (RelianceRuleInfo *rule1, RelianceRuleInfo *rule2) -> bool
    {
        return rule1->ruleDetails->rule.getId() < rule2->ruleDetails->rule.getId();
    };
    
    std::sort(infos.begin(), infos.end(), sortRule);
}

std::vector<SemiNaiverOrdered::PositiveGroup> SemiNaiverOrdered::computePositiveGroups(
    std::vector<RelianceRuleInfo> &allRules,
    SimpleGraph &positiveGraph,
    RelianceGroupResult &groupsResult
)
{
    std::vector<PositiveGroup> result;

    result.resize(groupsResult.groups.size());

    for (unsigned groupIndex = 0; groupIndex < groupsResult.groups.size(); ++groupIndex)
    {
        const std::vector<unsigned> &group = groupsResult.groups[groupIndex];
        
        PositiveGroup &newGroup = result[groupIndex];
        newGroup.members.reserve(group.size());
        newGroup.id = groupIndex;

        std::unordered_set<unsigned> successorSet;
        
        for (unsigned ruleIndex : group)
        {
            newGroup.members.push_back(&allRules[0] + ruleIndex);
        
            for (unsigned successor : positiveGraph.edges[ruleIndex])
            {
                unsigned successorGroup = groupsResult.assignments[successor];
                if (successorGroup == groupIndex)
                    continue;

                successorSet.insert(successorGroup);
            }
        }

        sortRuleVectorById(newGroup.members);

        for (unsigned successor : successorSet)
        {
            PositiveGroup *successorGroup = &result[0] + successor;
            newGroup.successors.push_back(successorGroup);
            ++successorGroup->numActivePredecessors;
        }
    }

    return result;
}

void SemiNaiverOrdered::updateGraph(SimpleGraph &graph, SimpleGraph &graphTransposed, PositiveGroup *group, unsigned *numActiveGroups, std::vector<bool> &activeRules)
{
    if (group->removed || group->isActive())
        return;

    (*numActiveGroups)--;
    group->setInactive();
    group->removed = true;

    for (RelianceRuleInfo *member : group->members)
    {
        if (!activeRules[member->id])
        {
            std::cout << "Error" << std::endl;
        }

        activeRules[member->id] = false;
    }

    for (PositiveGroup *successorGroup : group->successors)
    {
      if (successorGroup->numActivePredecessors == 0)
        std::cout << "Error" << std::endl;

        --successorGroup->numActivePredecessors;

        updateGraph(graph, graphTransposed, successorGroup, numActiveGroups, activeRules);
    }
}


SemiNaiverOrdered::RestrainedGroup SemiNaiverOrdered::computeRestrainedGroup(
    std::vector<RelianceRuleInfo> &allRules, 
    const std::vector<unsigned> &groupIndices)
{
    RestrainedGroup result;

    std::unordered_set<PositiveGroup*> groupSet;

    for (unsigned memberIndex : groupIndices)
    {
        RelianceRuleInfo *currentInfo = &allRules[0] + memberIndex;

        groupSet.insert(currentInfo->positiveGroup);

        if (currentInfo->numRestrains == 0)
        {
            result.unrestrainedMembers.push_back(currentInfo);
        }
        else
        {
            result.restrainedMembers.push_back(currentInfo);
        }
    }

    sortRuleVectorById(result.unrestrainedMembers);
    sortRuleVectorById(result.restrainedMembers);

    for (PositiveGroup *group : groupSet)
    {
        result.positiveGroups.push_back(group);
    }

    return result;
}

void SemiNaiverOrdered::run(size_t lastExecution,
    size_t iteration,
    unsigned long *timeout,
    bool checkCyclicTerm,
    int singleRule,
    PredId_t predIgnoreBlock)
{
    this->checkCyclicTerms = checkCyclicTerms;
    this->foundCyclicTerms = false;
    this->predIgnoreBlock = predIgnoreBlock;
    this->running = true;
    this->startTime = std::chrono::system_clock::now();
    listDerivations.clear();

    strategy = SemiNaiverOrderedType(SemiNaiverOrderedType::Default | SemiNaiverOrderedType::Dynamic);
    // strategy = SemiNaiverOrderedType(SemiNaiverOrderedType::Default | SemiNaiverOrderedType::Dynamic | SemiNaiverOrderedType::UnrestrainedFirst);
    // strategy = SemiNaiverOrderedType::PieceDecomposed;
    // strategy = SemiNaiverOrderedType::UnrestrainedFirst | SemiNaiverOrderedType::Dynamic;
    // strategy = SemiNaiverOrderedType::UnrestrainedFirst;

    this->iteration = iteration;

    const std::vector<Rule> &allOriginalRules = program->getAllRules();

    std::vector<Rule> allPieceDecomposedRules;
    if ((strategy & SemiNaiverOrderedType::PieceDecomposed) > 0)
    {
        for (const Rule &currentRule : allOriginalRules)
        {
            splitIntoPieces(currentRule, allPieceDecomposedRules);
        }
    }

    const std::vector<Rule> &allRules = ((strategy & SemiNaiverOrderedType::PieceDecomposed) > 0) ? allPieceDecomposedRules : allOriginalRules;
    
    std::cout << "#Rules: " << allRules.size() << '\n';

    std::chrono::system_clock::time_point relianceStart = std::chrono::system_clock::now();
    std::cout << "Computing positive reliances..." << '\n';
    RelianceComputationResult positiveResult = computePositiveReliances(allRules);
    std::pair<SimpleGraph, SimpleGraph> positiveGraphs = positiveResult.graphs;
   
    // positiveGraphs.first.saveCSV("positive_final.csv");

    std::cout << "Computing positive reliance groups..." << '\n';
    RelianceGroupResult positiveGroupsResult = computeRelianceGroups(positiveGraphs.first, positiveGraphs.second);    
    std::cout << "Reliance computation took " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - relianceStart).count() / 1000.0 << '\n';

    std::vector<StatIteration> costRules;

    std::cout << "Computing restraint reliances..." << '\n';
    relianceStart = std::chrono::system_clock::now();
    RelianceComputationResult restrainResult = computeRestrainReliances(allRules);
    std::pair<SimpleGraph, SimpleGraph> restrainingGraphs = restrainResult.graphs;
    std::cout << "Restraint computation took " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - relianceStart).count() / 1000.0 << '\n';

    // restrainingGraphs.first.saveCSV("blocking_final.csv");

    std::vector<RuleExecutionDetails> allRuleDetails;
    std::vector<RelianceRuleInfo> allRuleInfos;
    prepare(lastExecution, singleRule, allRules, allRuleInfos, allRuleDetails);

    std::vector<PositiveGroup> positiveGroups = computePositiveGroups(allRuleInfos, positiveGraphs.first, positiveGroupsResult);

    for (RelianceRuleInfo &currentInfo : allRuleInfos)
    {
        std::vector<RelianceRuleInfo *> positiveSuccessors, restraintSuccessors;
        for (unsigned successorIndex : positiveGraphs.first.edges[currentInfo.id])
        {
            positiveSuccessors.push_back(&allRuleInfos[0] + successorIndex);
        }

        for (unsigned successorIndex : restrainingGraphs.first.edges[currentInfo.id])
        {
            restraintSuccessors.push_back(&allRuleInfos[0] + successorIndex);
        }

        PositiveGroup *currentPositiveGroup = &positiveGroups[0] + positiveGroupsResult.assignments[currentInfo.id];
    
        currentInfo.initialize(currentPositiveGroup, positiveSuccessors, restraintSuccessors);
    }

    std::pair<SimpleGraph, SimpleGraph> unionGraphs = combineGraphs(positiveGraphs.first, restrainingGraphs.first);

    for (RelianceRuleInfo &currentInfo : allRuleInfos)
    {
        if (currentInfo.ruleDetails->nIDBs == 0)
        {
            currentInfo.setTriggered(true);
        }
    }

    //Stats
    unsigned numEDBRules = 0, numIDBRules = 0;
    unsigned numExTriggerComplex = 0, numExRestraintEx = 0;
    unsigned numDlRestraintEx = 0;
    unsigned numResInOrder = 0, numResOutOrder = 0;
    unsigned numComplexPred = 0, numComplexRules = 0;
    bool coreStratified = true;
    
    for (RelianceRuleInfo &currentInfo : allRuleInfos)
    {
        if (currentInfo.ruleDetails->nIDBs == 0)
        {
            ++numEDBRules;
        }
        else
        {
            ++numIDBRules;
        }

        const Rule &currentRule = currentInfo.ruleDetails->rule;
        currentInfo.existentialRule = currentRule.isExistential();
        currentInfo.complexRule = currentRule.getBody().size() > 1 || currentInfo.existentialRule;
    }

    for (unsigned fromRule = 0; fromRule < positiveGraphs.first.numberOfInitialNodes; ++fromRule)
    {
        for (unsigned toRule : positiveGraphs.first.edges[fromRule])
        {
            if (allRuleInfos[fromRule].existentialRule 
                && allRuleInfos[toRule].complexRule)
            {
                ++numExTriggerComplex;
            }
        }    
    }

    for (unsigned fromRule = 0; fromRule < restrainingGraphs.first.numberOfInitialNodes; ++fromRule)
    {
        for (unsigned toRule : restrainingGraphs.first.edges[fromRule])
        {
            if (allRuleInfos[fromRule].existentialRule 
                && allRuleInfos[toRule].existentialRule)
            {
                ++numExRestraintEx;
            }

            if (!allRuleInfos[fromRule].existentialRule 
                && allRuleInfos[toRule].existentialRule)
            {
                ++numDlRestraintEx;   
            }

            if (allRuleInfos[fromRule].id < allRuleInfos[toRule].id)
            {
                ++numResInOrder;
            }
            if (allRuleInfos[fromRule].id > allRuleInfos[toRule].id)
            {
                ++numResOutOrder;
            }
        }
    }

    for (unsigned fromRule = 0; fromRule < positiveGraphs.second.numberOfInitialNodes; ++fromRule)
    {
        if (allRuleInfos[fromRule].complexRule)
            ++numComplexRules;

        for (unsigned toRule : positiveGraphs.second.edges[fromRule])
        {
            //from = succ, to = pred

            if (allRuleInfos[fromRule].complexRule)
                ++numComplexPred;
        }
    }

    unsigned numActiveGroups = positiveGroups.size();
    std::vector<bool> activeRules;
    activeRules.resize(allRuleInfos.size(), true);

    for (PositiveGroup &currentGroup : positiveGroups)
    {
        updateGraph(unionGraphs.first, unionGraphs.second, &currentGroup, &numActiveGroups, activeRules);
    }

    RelianceGroupResult staticRestrainedGroups;
    size_t currentStaticGroupIndex = 0;
    // if ((strategy & SemiNaiverOrderedType::Dynamic) == 0)
    staticRestrainedGroups = computeRelianceGroups(unionGraphs.first, unionGraphs.second, &activeRules);
    
    for (auto &group : staticRestrainedGroups.groups)
    {
        for (unsigned ruleIndex : group)
        {
            for (unsigned restrainedRule : restrainingGraphs.first.edges[ruleIndex])
            {
                if (std::find(group.begin(), group.end(), restrainedRule) != group.end())
                {
                    coreStratified = false;
                    break;
                }
            }
        }
    }

    std::cout << "EDB: " << numEDBRules << ", IDB: " << numIDBRules << '\n';
    std::cout << "ExTriggerComplex: " << numExTriggerComplex << ", ExRestrainEx: " << numExRestraintEx << ", DlRestrainEx: " << numDlRestraintEx << '\n';
    std::cout << "ResInOrder: " << numResInOrder << ", ResOutOrder: " << numResOutOrder << '\n';
    std::cout << "ComplexRules: " << numComplexRules << ", ComplexPred: " << numComplexPred << ", Ratio: " << (double)numComplexPred / (double)numComplexRules << '\n';
    std::cout << "Core-Stratfied: " << ((coreStratified) ? "yes" : "no") << '\n';

    while (numActiveGroups > 0)
    {
        RelianceGroupResult dynamicRestrainedGroups;
        if ((strategy & SemiNaiverOrderedType::Dynamic) > 0)
            dynamicRestrainedGroups = computeRelianceGroups(unionGraphs.first, unionGraphs.second, &activeRules);    
        
        RestrainedGroup currentRestrainedGroup = 
            ((strategy & SemiNaiverOrderedType::Dynamic) > 0) ?
            computeRestrainedGroup(allRuleInfos, dynamicRestrainedGroups.groups[dynamicRestrainedGroups.minimumGroup]) :
            computeRestrainedGroup(allRuleInfos, staticRestrainedGroups.groups[currentStaticGroupIndex++]);
   
        // std::cout << "Groups: " << dynamicRestrainedGroups.groups.size() << '\n';

        PositiveGroup *nextInactive;
        if ((strategy & SemiNaiverOrderedType::UnrestrainedFirst) > 0)
        {
            nextInactive = executeGroupUnrestrainedFirst(currentRestrainedGroup, costRules, timeout, strategy);
        }
        else
        {
            nextInactive = executeGroupByPositiveGroups(currentRestrainedGroup, costRules, timeout);
            // std::cout << "Positive first" << std::endl;
        }

        if (nextInactive != nullptr)
        {
            updateGraph(unionGraphs.first, unionGraphs.second, nextInactive, &numActiveGroups, activeRules);
        }
        else
        {
            for (PositiveGroup *inactiveGroup : currentRestrainedGroup.positiveGroups)
            {
                if (inactiveGroup->isActive())
                    continue;

                updateGraph(unionGraphs.first, unionGraphs.second, inactiveGroup, &numActiveGroups, activeRules);
            }
        }
    } 

    std::cout << "Iterations: " << this->iteration << std::endl;

    this->running = false;
}