#include "vlog/reliances/reliances.h"
#include "vlog/reliances/models.h"

#include <vector>
#include <utility>
#include <list>
#include <unordered_map>
#include <unordered_set>

std::chrono::system_clock::time_point globalPositiveTimepointStart;
unsigned globalPositiveTimeout = 0;
unsigned globalPositiveTimeoutCheckCount = 0;
bool globalPositiveIsTimeout = false;

bool positiveIsTimeout(bool rare)
{
    if (globalPositiveTimeout == 0)
        return false;

    if (!rare || globalPositiveTimeoutCheckCount++ % 100 == 0)
    {
        unsigned currentTimeMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - globalPositiveTimepointStart).count();
        if (currentTimeMilliSeconds > globalPositiveTimeout)
        {
            globalPositiveIsTimeout = true;
            return true;
        }
    }

    return false;
}

bool positiveExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    VariableAssignments &assignments, RelianceStrategy strat)
{
    unsigned tupleSize = literalFrom.getTupleSize(); //Should be the same as literalTo.getTupleSize()

    for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
    {
        VTerm fromTerm = literalFrom.getTermAtPos(termIndex);
        VTerm toTerm = literalTo.getTermAtPos(termIndex);
    
        TermInfo fromInfo = getTermInfoUnify(fromTerm, assignments, RelianceRuleRelation::From);
        TermInfo toInfo = getTermInfoUnify(toTerm, assignments, RelianceRuleRelation::To);

        // We may not assign a universal variable of fromRule to a null
        if (((strat & RelianceStrategy::EarlyTermination) > 0)
            && (fromInfo.type == TermInfo::Types::Universal && toInfo.constant < 0))
            return false;

        if (!unifyTerms(fromInfo, toInfo, assignments))
            return false;
    }

    assignments.finishGroupAssignments();

    return true;
}

template<typename T>
int positiveCheckNullsInToBody(const std::vector<T> &literals,
    const VariableAssignments &assignments, RelianceRuleRelation relation)
{
    for (int literalIndex = 0; literalIndex < literals.size(); ++literalIndex)
    {
        const Literal &literal = literals[literalIndex];

        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
        
            if ((int32_t)currentTerm.getId() > 0)
            {
                if (assignments.getConstant((int32_t)currentTerm.getId(), relation) < 0)
                    return literalIndex;
            }
        }
    }   

    return -1;
}

bool positiveExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    VariableAssignments &assignments,
    RelianceStrategy strat);

std::vector<std::reference_wrapper<const Literal>> notMappedToBodyLiterals;
std::vector<unsigned> notMappedToBodyIndexes;

RelianceCheckResult positiveCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments)
{
    unsigned nextInDomainIndex = 0;
    const std::vector<Literal> &toBodyLiterals = ruleTo.getBody();
    
    notMappedToBodyLiterals.clear();
    notMappedToBodyIndexes.clear();
    
    notMappedToBodyLiterals.reserve(toBodyLiterals.size());
    notMappedToBodyIndexes.reserve(toBodyLiterals.size());
    
    for (unsigned bodyIndex = 0; bodyIndex < toBodyLiterals.size(); ++bodyIndex)
    {
        if (bodyIndex == mappingDomain[nextInDomainIndex])
        {
            if (nextInDomainIndex < mappingDomain.size() - 1)
            {
                ++nextInDomainIndex;
            }

            continue;
        }

        notMappedToBodyLiterals.push_back(toBodyLiterals[bodyIndex]);
        notMappedToBodyIndexes.push_back(bodyIndex);
    }

    if (positiveCheckNullsInToBody(ruleFrom.getBody(), assignments, RelianceRuleRelation::From) >= 0)
    {
        return RelianceCheckResult::False;
    }

    int unmappedResult = positiveCheckNullsInToBody(notMappedToBodyLiterals, assignments, RelianceRuleRelation::To);

    if (unmappedResult >= 0)
    {
        if (notMappedToBodyIndexes[unmappedResult] < mappingDomain.back())
        {
            return RelianceCheckResult::False;
        }
        else
        {
            return RelianceCheckResult::Extend;
        }
    }

    std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> existentialMappings;
    std::vector<unsigned> satisfied;
    
    if (possiblySatisfied(ruleFrom.getHeads(), {ruleFrom.getBody()}, notMappedToBodyLiterals))
    {
        satisfied.resize(ruleFrom.getHeads().size(), 0);
        prepareExistentialMappings(ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, existentialMappings);

        bool fromRuleSatisfied = relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
        fromRuleSatisfied |= relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);

        if (!fromRuleSatisfied && ruleFrom.isExistential())
        {
            fromRuleSatisfied = checkConsistentExistential(existentialMappings);
        }

        if (fromRuleSatisfied)
        {
            return RelianceCheckResult::Extend;
        }
    }

    if (possiblySatisfied(ruleTo.getBody(), {ruleFrom.getBody()}, notMappedToBodyLiterals))
    {
        satisfied.clear();
        satisfied.resize(ruleTo.getBody().size(), 0);
        prepareExistentialMappings(ruleTo.getBody(), RelianceRuleRelation::To, assignments, existentialMappings);

        bool toBodySatisfied = relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getBody(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
        toBodySatisfied |= relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleTo.getBody(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);

        if (!toBodySatisfied && ruleTo.isExistential())
        {
            toBodySatisfied = checkConsistentExistential(existentialMappings);
        }

        if (toBodySatisfied)
        {
            return RelianceCheckResult::Extend;
        }
    }

    if (possiblySatisfied(ruleTo.getHeads(), {ruleFrom.getBody(), ruleFrom.getHeads()}, notMappedToBodyLiterals))
    {
        satisfied.clear();
        satisfied.resize(ruleTo.getHeads().size(), 0);
        prepareExistentialMappings(ruleTo.getHeads(), RelianceRuleRelation::To, assignments, existentialMappings);

        bool toRuleSatisfied = relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
        toRuleSatisfied |= relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
        toRuleSatisfied |= relianceModels(ruleFrom.getHeads(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
    
        if (!toRuleSatisfied && ruleTo.isExistential())
        {
            toRuleSatisfied = checkConsistentExistential(existentialMappings);
        }

        if (toRuleSatisfied)
            return RelianceCheckResult::False;
    }

    return RelianceCheckResult::True;
}

bool positiveExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    VariableAssignments &assignments,
    RelianceStrategy strat)
{
    if (positiveIsTimeout(true))
        return true;

    unsigned bodyToStartIndex = (mappingDomain.size() == 0) ? 0 : mappingDomain.back() + 1;

    for (unsigned bodyToIndex = bodyToStartIndex; bodyToIndex < ruleTo.getBody().size(); ++bodyToIndex)
    {
        const Literal &literalTo = ruleTo.getBody().at(bodyToIndex);
        mappingDomain.push_back(bodyToIndex);

        for (unsigned headFromIndex = 0; headFromIndex < ruleFrom.getHeads().size(); ++headFromIndex)
        {
            const Literal &literalFrom =  ruleFrom.getHeads().at(headFromIndex);

            if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId())
                continue;

            assignments.increaseDepth();
            VariableAssignments &extendedAssignments = assignments;
            
            if (!positiveExtendAssignment(literalFrom, literalTo, extendedAssignments, strat))
            {
                assignments.decreaseDepth();
                continue;
            }

            switch (positiveCheck(mappingDomain, ruleFrom, ruleTo, extendedAssignments))
            {
                case RelianceCheckResult::Extend:
                {
                    if (positiveExtend(mappingDomain, ruleFrom, ruleTo, extendedAssignments, strat))
                        return true;
                } break;

                case RelianceCheckResult::False:
                {
                    if ((strat & RelianceStrategy::EarlyTermination) == 0
                        && positiveExtend(mappingDomain, ruleFrom, ruleTo, extendedAssignments, strat))
                        return true;
                } break;

                case RelianceCheckResult::True:
                {
                    return true;
                } break;
            }

            assignments.decreaseDepth();
        }

        mappingDomain.pop_back();
    }

    return false;
}

bool positiveFullIteration(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, unsigned variableCountTo,
    RelianceStrategy strat, std::vector<size_t> &atomMapping, size_t index)
{
    size_t targetSize = ruleFrom.getHeads().size() + 1; // +1 because it can be unassigned

    for(atomMapping[index] = 0; atomMapping[index] < targetSize; atomMapping[index]++)
    {
        if(index == atomMapping.size() - 1)
        {
            if (positiveIsTimeout(true))
                return true;

            // atomMapping now contains a valid mapping
            std::vector<unsigned> currentMappingDomain;
            VariableAssignments currentAssignments(variableCountFrom, variableCountTo);
            bool validAssignments = true;

            for (unsigned bodyToIndex = 0; bodyToIndex < atomMapping.size(); ++bodyToIndex)
            {
                unsigned headFromIndex = (unsigned)atomMapping[bodyToIndex];
                if (headFromIndex == 0)
                    continue;
                
                currentMappingDomain.push_back(bodyToIndex);

                const Literal &literalTo = ruleTo.getBody().at(bodyToIndex);
                const Literal &literalFrom =  ruleFrom.getHeads().at(headFromIndex - 1);

                if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId()
                    || !positiveExtendAssignment(literalFrom, literalTo, currentAssignments, strat))
                {
                    validAssignments = false;
                    break;
                }
            }

            if (currentMappingDomain.size() == 0 || !validAssignments)
                continue;

            if (positiveCheck(currentMappingDomain, ruleFrom, ruleTo, currentAssignments) == RelianceCheckResult::True)
                return true;
        }
        else
        {
            if (positiveFullIteration(ruleFrom, variableCountFrom, ruleTo, variableCountTo, strat, 
                atomMapping, index + 1))
            {
                return true;   
            }
        }
    }

    return false;
}

bool positiveReliance(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, unsigned variableCountTo,
    RelianceStrategy strat)
{
    if ((strat & RelianceStrategy::BetterIterate) > 0)
    {
        std::vector<unsigned> mappingDomain;
        VariableAssignments assignments(variableCountFrom, variableCountTo);

        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }
    else
    {
        std::vector<size_t> atomMapping;
        atomMapping.resize(ruleTo.getBody().size(), 0);

        return positiveFullIteration(ruleFrom, variableCountFrom, ruleTo, variableCountTo, 
            strat, atomMapping, 0);
    }
}

RelianceComputationResult computePositiveReliances(const std::vector<Rule> &rules, RelianceStrategy strat, unsigned timeoutMilliSeconds)
{
    RelianceComputationResult result;
    result.graphs = std::pair<SimpleGraph, SimpleGraph>(SimpleGraph(rules.size()), SimpleGraph(rules.size()));

    std::vector<Rule> markedRules;
    markedRules.reserve(rules.size());

    std::vector<unsigned> variableCounts;
    variableCounts.reserve(rules.size());

    std::vector<unsigned> IDBRuleIndices;
    IDBRuleIndices.reserve(rules.size());

    for (unsigned ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {
        const Rule &currentRule = rules[ruleIndex];

        unsigned variableCount = std::max(highestLiteralsId(currentRule.getHeads()), highestLiteralsId(currentRule.getBody()));
        variableCounts.push_back(variableCount + 1);

        if (currentRule.getNIDBPredicates() > 0)
        {
            IDBRuleIndices.push_back(ruleIndex);
        }
    }

    for (const Rule &currentRule : rules)
    {
        Rule markedRule = markExistentialVariables(currentRule);
        markedRules.push_back(markedRule);
    }

    std::vector<RuleHashInfo> ruleHashInfos; ruleHashInfos.reserve(rules.size());
    for (const Rule &currentRule : rules)
    {
        ruleHashInfos.push_back(ruleHashInfoFirst(currentRule));
    }

    std::unordered_map<PredId_t, std::vector<size_t>> bodyToMap;
    std::unordered_map<std::string, bool> resultCache;

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {
        const Rule &markedRule = markedRules[ruleIndex];

        for (const Literal &currentLiteral : markedRule.getBody())
        {
            PredId_t currentPredId = currentLiteral.getPredicate().getId();

            bodyToMap[currentPredId].push_back(ruleIndex);
        }
    }

    std::chrono::system_clock::time_point timepointStart = std::chrono::system_clock::now();
    globalPositiveTimepointStart = timepointStart;
    globalPositiveTimeout = timeoutMilliSeconds;

    uint64_t numCalls = 0;

    enum RelianceExecutionCommand {
        None,
        Continue,
        Return
    };

    auto relianceExecution = [&] (size_t ruleFrom, size_t ruleTo, std::unordered_set<size_t> &proccesedRules) {
        if ((strat & RelianceStrategy::CutPairs) > 0)
        {
            if (proccesedRules.find(ruleTo) != proccesedRules.end())
                return RelianceExecutionCommand::Continue;

            proccesedRules.insert(ruleTo);
        }

        if (positiveIsTimeout(false))
        {
            result.timeout = true;
            result.numberOfCalls = numCalls;
            return RelianceExecutionCommand::Return;
        }

        std::string stringHash;
        if ((strat & RelianceStrategy::PairHash) > 0)
        {
            stringHash = rulePairHash(ruleHashInfos[ruleFrom], ruleHashInfos[ruleTo], rules[ruleTo]);
            auto cacheIterator = resultCache.find(stringHash);
            if (cacheIterator != resultCache.end())
            {
                if (cacheIterator->second)
                {
                    result.graphs.first.addEdge(ruleFrom, ruleTo);
                    result.graphs.second.addEdge(ruleTo, ruleFrom);
                }

                return RelianceExecutionCommand::Continue;
            }
        }

        ++numCalls;

        unsigned variableCountFrom = variableCounts[ruleFrom];
        unsigned variableCountTo = variableCounts[ruleTo];
        
        std::chrono::system_clock::time_point relianceExecutionStart = std::chrono::system_clock::now();
        bool isReliance = positiveReliance(markedRules[ruleFrom], variableCountFrom, markedRules[ruleTo], variableCountTo, strat);
        if (isReliance)
        {
            result.graphs.first.addEdge(ruleFrom, ruleTo);
            result.graphs.second.addEdge(ruleTo, ruleFrom);
        }
        size_t relianceExecutionDurationMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - relianceExecutionStart).count();
        if (relianceExecutionDurationMicroseconds > result.timeLongestPairMicro)
        {
            result.timeLongestPairMicro = relianceExecutionDurationMicroseconds;
            result.longestPairString = rules[ruleFrom].tostring() + " -> " + rules[ruleTo].tostring();
        }

        if (((strat & RelianceStrategy::PairHash) > 0) && (resultCache.size() < rulePairCacheSize))
        {
            resultCache[stringHash] = isReliance;
        }

        if (globalPositiveIsTimeout)
        {
            std::cout << "Timeout-Rule-Positive: " 
                << rules[ruleFrom].tostring() << " -> " << rules[ruleTo].tostring() << '\n'; 
            
            result.numberOfCalls = numCalls;
            result.timeout = true;
            return RelianceExecutionCommand::Return;
        }

        return RelianceExecutionCommand::None;
    };

    if ((strat & RelianceStrategy::CutPairs) > 0)
    {
        for (size_t ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
        {
            std::unordered_set<size_t> proccesedRules;

            for (const Literal &currentLiteral : rules[ruleFrom].getHeads())
            {
                PredId_t currentPredicate = currentLiteral.getPredicate().getId();
                auto toIterator = bodyToMap.find(currentPredicate);

                if (toIterator == bodyToMap.end())
                    continue;
                
                for (size_t ruleTo : toIterator->second)
                {
                    switch (relianceExecution(ruleFrom, ruleTo, proccesedRules))
                    {
                        case RelianceExecutionCommand::Return:
                            return result;
                        case RelianceExecutionCommand::Continue:
                            continue;
                    }
                }
            }
        }
    }
    else
    {
        std::unordered_set<size_t> proccesedRules;

        for (size_t ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
        {
            for (size_t ruleTo = 0; ruleTo < rules.size(); ++ruleTo)
            {
                switch (relianceExecution(ruleFrom, ruleTo, proccesedRules))
                {
                    case RelianceExecutionCommand::Return:
                        return result;
                    case RelianceExecutionCommand::Continue:
                        continue;
                }
            }
        }
    }    
    
    result.timeMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - timepointStart).count();
    result.timeout = false;
    result.numberOfCalls = numCalls;

    return result;
}

unsigned DEBUGcountFakePositiveReliances(const std::vector<Rule> &rules, const SimpleGraph &positiveGraph)
{
    unsigned result = 0;

    for (unsigned ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
    {
        for (unsigned ruleTo = 0; ruleTo < rules.size(); ++ruleTo)
        {
            if (positiveGraph.containsEdge(ruleFrom, ruleTo))
                continue;

            bool headPredicateInBody = false;

            for (const Literal &headLiteral : rules[ruleFrom].getHeads())
            {
                for (const Literal &bodyLiteral : rules[ruleTo].getBody())
                {
                    if (headLiteral.getPredicate().getId() == bodyLiteral.getPredicate().getId())
                    {
                        headPredicateInBody = true;
                        break;
                    }
                }

                if (headPredicateInBody)
                    break;
            }

            if (headPredicateInBody)
            {
                std::cout << "Found fake reliance from " << ruleFrom << " to " << ruleTo << '\n';
                ++result;
            }
        }
    }

    return result;
}