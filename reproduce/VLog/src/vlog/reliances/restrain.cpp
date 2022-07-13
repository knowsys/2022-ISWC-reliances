#include "vlog/reliances/reliances.h"
#include "vlog/reliances/models.h"

#include <vector>
#include <utility>
#include <list>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

std::chrono::system_clock::time_point globalRestraintTimepointStart;
unsigned globalRestraintTimeout = 0;
unsigned globalRestraintTimeoutCheckCount = 0;
bool globalRestraintIsTimeout = false;

bool restrainIsTimeout(bool rare)
{
    if (globalRestraintTimeout == 0)
        return false;

    if (!rare || globalRestraintTimeoutCheckCount++ % 100 == 0)
    {
        unsigned currentTimeMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - globalRestraintTimepointStart).count();
        if (currentTimeMilliSeconds > globalRestraintTimeout)
        {
            globalRestraintIsTimeout = true;
            return true;
        }
    }

    return false;
}

bool restrainExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo, 
    VariableAssignments &assignments,
    RelianceStrategy strat);

int checkUnmappedExistentialVariables(const std::vector<std::reference_wrapper<const Literal>> &literals, 
    const VariableAssignments &assignments, RelianceRuleRelation relation = RelianceRuleRelation::To)
{
    for (int literalIndex = 0; literalIndex < literals.size(); ++literalIndex)
    {
        const Literal &literal = literals[literalIndex];

        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);

            if ((int32_t)currentTerm.getId() < 0)
            {
                // if (assignments.getGroupId((int32_t)currentTerm.getId(), RelianceRuleRelation::To) != NOT_ASSIGNED
                //     && assignments.getConstant((int32_t)currentTerm.getId(), RelianceRuleRelation::To) != NOT_ASSIGNED)
                //     return false;

                int64_t assignedConstant = assignments.getConstant((int32_t)currentTerm.getId(), relation);
                if (assignedConstant != NOT_ASSIGNED && assignedConstant < 0)
                    return literalIndex;
            }                
        }
    }

    return -1;
}

bool restrainCheckNullsInBody(const std::vector<Literal> &literals,
    const VariableAssignments &assignments, RelianceRuleRelation relation)
{
    for (const Literal &literal : literals)
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
        
            if ((int32_t)currentTerm.getId() > 0)
            {
                if (assignments.getConstant((int32_t)currentTerm.getId(), relation) < 0)
                    return false;
            }
        }
    }   

    return true;   
}

std::vector<std::reference_wrapper<const Literal>> notMappedHeadLiterals;
std::vector<unsigned> notMappedHeadIndexes;

RelianceCheckResult restrainCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments)
{
    unsigned nextInDomainIndex = 0;
    const std::vector<Literal> &toHeadLiterals = ruleTo.getHeads();

    notMappedHeadLiterals.clear();
    notMappedHeadIndexes.clear();

    notMappedHeadLiterals.reserve(ruleTo.getHeads().size());
    notMappedHeadIndexes.reserve(ruleTo.getHeads().size());

    for (unsigned headIndex = 0; headIndex < toHeadLiterals.size(); ++headIndex)
    {
        if (headIndex == mappingDomain[nextInDomainIndex])
        {
            if (nextInDomainIndex < mappingDomain.size() - 1)
            {
                ++nextInDomainIndex;
            }

            continue;
        }

        notMappedHeadLiterals.push_back(toHeadLiterals[headIndex]);
        notMappedHeadIndexes.push_back(headIndex);
    }

    if (!restrainCheckNullsInBody(ruleFrom.getBody(), assignments, RelianceRuleRelation::From)
            || !restrainCheckNullsInBody(ruleTo.getBody(), assignments, RelianceRuleRelation::To))
        return RelianceCheckResult::False;

    if (!assignments.hasMappedExistentialVariable)
        return RelianceCheckResult::Extend;

    int unmappedResult = checkUnmappedExistentialVariables(notMappedHeadLiterals, assignments);

    if (unmappedResult >= 0)
    {
        if (notMappedHeadIndexes[unmappedResult] < mappingDomain.back())
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
    
    if (possiblySatisfied(ruleTo.getHeads(), {ruleTo.getBody()}))
    {
        satisfied.resize(ruleTo.getHeads().size(), 0);
        prepareExistentialMappings(ruleTo.getHeads(), RelianceRuleRelation::To, assignments, existentialMappings);

        bool toHeadSatisfied =
            relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
            
        if (!toHeadSatisfied && ruleTo.isExistential())
        {
            toHeadSatisfied = checkConsistentExistential(existentialMappings);
        }

        if (toHeadSatisfied)
            return RelianceCheckResult::False;
    }

    prepareExistentialMappings(ruleTo.getHeads(), RelianceRuleRelation::To, assignments, existentialMappings);
    satisfied.clear();
    satisfied.resize(ruleTo.getHeads().size(), 0);

    bool alternativeMatchAlreadyPresent = relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);
    alternativeMatchAlreadyPresent |= relianceModels(ruleTo.getHeads(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, true, false);
    alternativeMatchAlreadyPresent |= relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);
    alternativeMatchAlreadyPresent |= relianceModels(notMappedHeadLiterals, RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);

    if (!alternativeMatchAlreadyPresent && ruleTo.isExistential())
    {
        alternativeMatchAlreadyPresent = checkConsistentExistential(existentialMappings);
    }

    if (alternativeMatchAlreadyPresent)
        return RelianceCheckResult::Extend;

    if (possiblySatisfied(ruleFrom.getHeads(), {ruleTo.getBody(), ruleTo.getHeads(), ruleFrom.getBody()}, notMappedHeadLiterals))
    {
        prepareExistentialMappings(ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, existentialMappings);
        satisfied.clear();
        satisfied.resize(ruleFrom.getHeads().size(), 0);

        bool fromHeadSatisfied = relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
        fromHeadSatisfied |= relianceModels(ruleTo.getHeads(), RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings, true);
        fromHeadSatisfied |= relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
        fromHeadSatisfied |= relianceModels(notMappedHeadLiterals, RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);

        if (!fromHeadSatisfied && ruleFrom.isExistential())
        {
            fromHeadSatisfied = checkConsistentExistential(existentialMappings);
        }

        if (fromHeadSatisfied)
            return RelianceCheckResult::Extend;
    }

    return RelianceCheckResult::True;
}

bool restrainExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    VariableAssignments &assignments, RelianceStrategy strat)
{
    unsigned tupleSize = literalFrom.getTupleSize(); //Should be the same as literalTo.getTupleSize()

    for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
    {
        VTerm fromTerm = literalFrom.getTermAtPos(termIndex);
        VTerm toTerm = literalTo.getTermAtPos(termIndex);
    
        TermInfo fromInfo = getTermInfoUnify(fromTerm, assignments, RelianceRuleRelation::From);
        TermInfo toInfo = getTermInfoUnify(toTerm, assignments, RelianceRuleRelation::To);

         // We may not assign any universal variable to a null
        if (((strat & RelianceStrategy::EarlyTermination) > 0) &&
            (toInfo.type == TermInfo::Types::Universal || fromInfo.type == TermInfo::Types::Universal) 
            && (fromInfo.constant < 0 || toInfo.constant < 0))
            return false;

        if (!unifyTerms(fromInfo, toInfo, assignments))
            return false;

        if (toInfo.type == TermInfo::Types::Existential)
            assignments.hasMappedExistentialVariable = true;
    }

    assignments.finishGroupAssignments();

    return true;
}

bool restrainExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    VariableAssignments &assignments, RelianceStrategy strat)
{
    if (restrainIsTimeout(true))
        return true;

    unsigned headToStartIndex = (mappingDomain.size() == 0) ? 0 : mappingDomain.back() + 1;

    for (unsigned headToIndex = headToStartIndex; headToIndex < ruleTo.getHeads().size(); ++headToIndex)
    {
        const Literal &literalTo = ruleTo.getHeads()[headToIndex];
        mappingDomain.push_back(headToIndex);

        for (unsigned headFromIndex = 0; headFromIndex < ruleFrom.getHeads().size(); ++headFromIndex)
        {
            const Literal &literalFrom =  ruleFrom.getHeads().at(headFromIndex);

            if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId())
                continue;

            assignments.increaseDepth();
            VariableAssignments &extendedAssignments = assignments;

            if (!restrainExtendAssignment(literalFrom, literalTo, extendedAssignments, strat))
            {
                assignments.decreaseDepth();
                continue;
            }

            switch (restrainCheck(mappingDomain, ruleFrom, ruleTo, extendedAssignments))
            {
                case RelianceCheckResult::Extend:
                {
                    if (restrainExtend(mappingDomain, ruleFrom, ruleTo, extendedAssignments, strat))
                        return true;
                } break;

                case RelianceCheckResult::False:
                {
                    if ((strat & RelianceStrategy::EarlyTermination) == 0
                        && restrainExtend(mappingDomain, ruleFrom, ruleTo, extendedAssignments, strat))
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

bool restrainFullIteration(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, unsigned variableCountTo,
    RelianceStrategy strat, std::vector<size_t> &atomMapping, size_t index)
{
    size_t targetSize = ruleFrom.getHeads().size() + 1; // +1 because it can be unassigned

    for(atomMapping[index] = 0; atomMapping[index] < targetSize; atomMapping[index]++)
    {
        if(index == atomMapping.size() - 1)
        {
            if (restrainIsTimeout(true))
                return true;

            // atomMapping now contains a valid mapping
            std::vector<unsigned> currentMappingDomain;
            VariableAssignments currentAssignments(variableCountFrom, variableCountTo);
            bool validAssignments = true;

            for (unsigned headToIndex = 0; headToIndex < atomMapping.size(); ++headToIndex)
            {
                unsigned headFromIndex = (unsigned)atomMapping[headToIndex];
                if (headFromIndex == 0)
                    continue;
                
                currentMappingDomain.push_back(headToIndex);

                const Literal &literalTo = ruleTo.getHeads().at(headToIndex);
                const Literal &literalFrom =  ruleFrom.getHeads().at(headFromIndex - 1);

                if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId()
                    || !restrainExtendAssignment(literalFrom, literalTo, currentAssignments, strat))
                {
                    validAssignments = false;
                    break;
                }
            }

            if (currentMappingDomain.size() == 0 || !validAssignments)
                continue;

            if (restrainCheck(currentMappingDomain, ruleFrom, ruleTo, currentAssignments) == RelianceCheckResult::True)
                return true;
        }
        else
        {
            if (restrainFullIteration(ruleFrom, variableCountFrom, ruleTo, variableCountTo, strat, 
                atomMapping, index + 1))
            {
                return true;   
            }
        }
    }

    return false;
}

bool restrainReliance(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, unsigned variableCountTo, 
    RelianceStrategy strat)
{
    if ((strat & RelianceStrategy::BetterIterate) > 0)
    {
        std::vector<unsigned> mappingDomain;
        VariableAssignments assignments(variableCountFrom, variableCountTo);

        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }
    else
    {
        std::vector<size_t> atomMapping;
        atomMapping.resize(ruleTo.getHeads().size(), 0);

        return restrainFullIteration(ruleFrom, variableCountFrom, ruleTo, variableCountTo, 
            strat, atomMapping, 0);
    }
}

// SELF-RESTRAIN 

bool selfRestrainIsNullReducing(const Rule &rule, const VariableAssignments &assignments)
{
    std::unordered_set<int64_t> existentialVariables, assignedNulls;

    for (const Literal &literal : rule.getHeads())
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm term = literal.getTermAtPos(termIndex);
            
            int32_t termId = (int32_t)term.getId();
            int64_t constant = assignments.getConstant((int32_t)term.getId(), RelianceRuleRelation::From);

            if (termId < 0)
            {
                existentialVariables.insert(termId);

                if (constant != NOT_ASSIGNED)
                {
                    if (constant < 0)
                        assignedNulls.insert(constant);
                    else
                        return true;
                }
                else
                    return true;
            }
        }
    }

    return assignedNulls.size() < existentialVariables.size();
}

bool selfRestrainExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    VariableAssignments &assignments);

RelianceCheckResult selfRestrainCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &rule, const VariableAssignments &assignments)
{
    unsigned nextInDomainIndex = 0;
    const std::vector<Literal> toHeadLiterals = rule.getHeads();

    notMappedHeadLiterals.clear();
    notMappedHeadIndexes.clear();

    notMappedHeadLiterals.reserve(rule.getHeads().size());
    notMappedHeadIndexes.reserve(rule.getHeads().size());
    for (unsigned headIndex = 0; headIndex < toHeadLiterals.size(); ++headIndex)
    {
        if (headIndex == mappingDomain[nextInDomainIndex])
        {
            if (nextInDomainIndex < mappingDomain.size() - 1)
            {
                ++nextInDomainIndex;
            }

            continue;
        }

        notMappedHeadLiterals.push_back(toHeadLiterals[headIndex]);
        notMappedHeadIndexes.push_back(headIndex);
    }

    if (!restrainCheckNullsInBody(rule.getBody(), assignments, RelianceRuleRelation::From))
        return RelianceCheckResult::False;

    if (!selfRestrainIsNullReducing(rule, assignments))
        return RelianceCheckResult::False;

    int unmappedResult = checkUnmappedExistentialVariables(notMappedHeadLiterals, assignments, RelianceRuleRelation::From);

    if (unmappedResult >= 0)
    {
        if (notMappedHeadIndexes[unmappedResult] < mappingDomain.back())
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

    if (possiblySatisfied(rule.getHeads(), {rule.getBody()}, notMappedHeadLiterals))
    {
        satisfied.resize(rule.getHeads().size(), 0);
        prepareExistentialMappings(rule.getHeads(), RelianceRuleRelation::From, assignments, existentialMappings);

        bool ruleSatisfied= relianceModels(rule.getBody(), RelianceRuleRelation::From, rule.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
        ruleSatisfied |= relianceModels(notMappedHeadLiterals, RelianceRuleRelation::From, rule.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
    
        if (!ruleSatisfied && rule.isExistential())
        {
            ruleSatisfied = checkConsistentExistential(existentialMappings);
        }

        if (ruleSatisfied)
            return RelianceCheckResult::Extend;
    }

    return RelianceCheckResult::True;
}

bool selfRestrainUnifyTerms(const TermInfo &fromInfo, const TermInfo &toInfo, VariableAssignments &assignments)
{
    if (fromInfo.constant != NOT_ASSIGNED && toInfo.constant != NOT_ASSIGNED && fromInfo.constant != toInfo.constant)
        return false;

    if (fromInfo.type == TermInfo::Types::Constant && toInfo.type == TermInfo::Types::Constant)
        return true;

    if (fromInfo.type != TermInfo::Types::Constant && toInfo.type == TermInfo::Types::Constant)
    {
        assignments.assignConstants(fromInfo.termId, fromInfo.relation, toInfo.constant);
    }
    else if (fromInfo.type == TermInfo::Types::Constant && toInfo.type != TermInfo::Types::Constant)
    {
        assignments.assignConstants(toInfo.termId, toInfo.relation, fromInfo.constant);
    }
    else 
    {
        assignments.connectVariablesSelf(fromInfo.termId, toInfo.termId);
    }

    return true;
}

// does the same thing as getTermInfoUnify but keeps in mind that info is only saved in the From-portion of VairableAssignments
TermInfo selfRestrainGetTermInfoUnify(VTerm term, const VariableAssignments &assignments, RelianceRuleRelation relation)
{
    TermInfo result;
    result.relation = RelianceRuleRelation::From;
    result.termId = (int32_t)term.getId();

    if (term.getId() == 0)
    {
        result.constant = (int64_t)term.getValue();
        result.type = TermInfo::Types::Constant;
    }
    else
    {
        if ((int32_t)term.getId() < 0 && relation == RelianceRuleRelation::From)
        {
            result.constant = (int32_t)term.getId();
            result.type = TermInfo::Types::Constant;
        }
        else
        {
            result.type = ((int32_t)term.getId() > 0) ? TermInfo::Types::Universal : TermInfo::Types::Existential;
            result.groupId = assignments.getGroupId(term.getId(), RelianceRuleRelation::From);
            result.constant = assignments.getConstant(term.getId(), RelianceRuleRelation::From);
        }
    }

    return result;
}


bool selfRestrainExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    VariableAssignments &assignments)
{
    unsigned tupleSize = literalFrom.getTupleSize(); //Should be the same as literalTo.getTupleSize()

    for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
    {
        VTerm fromTerm = literalFrom.getTermAtPos(termIndex);
        VTerm toTerm = literalTo.getTermAtPos(termIndex);
    
        TermInfo fromInfo = selfRestrainGetTermInfoUnify(fromTerm, assignments, RelianceRuleRelation::From);
        TermInfo toInfo = selfRestrainGetTermInfoUnify(toTerm, assignments, RelianceRuleRelation::To);

        if (!selfRestrainUnifyTerms(fromInfo, toInfo, assignments))
            return false;
    }

    assignments.finishGroupAssignments();

    return true;
}

bool selfRestrainExtend(std::vector<unsigned> &mappingDomain, const Rule &rule,
    VariableAssignments &assignments, RelianceStrategy strat)
{
    if (restrainIsTimeout(true))
        return true;

    unsigned headToStartIndex = (mappingDomain.size() == 0) ? 0 : mappingDomain.back() + 1;

    for (unsigned headToIndex = headToStartIndex; headToIndex < rule.getHeads().size(); ++headToIndex)
    {
        const Literal &literalTo = rule.getHeads()[headToIndex];
        mappingDomain.push_back(headToIndex);

        for (unsigned headFromIndex = 0; headFromIndex < rule.getHeads().size(); ++headFromIndex)
        {
            const Literal &literalFrom =  rule.getHeads().at(headFromIndex);

            if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId())
                continue;

            assignments.increaseDepth();
            VariableAssignments &extendedAssignments = assignments;
            
            if (!selfRestrainExtendAssignment(literalFrom, literalTo, extendedAssignments))
            {
                assignments.decreaseDepth();
                continue;
            }

            switch (selfRestrainCheck(mappingDomain, rule, extendedAssignments))
            {
                case RelianceCheckResult::Extend:
                {
                    if (selfRestrainExtend(mappingDomain, rule, extendedAssignments, strat))
                        return true;
                } break;

                case RelianceCheckResult::False:
                {
                    if ((strat & RelianceStrategy::EarlyTermination) == 0
                        && selfRestrainExtend(mappingDomain, rule, extendedAssignments, strat))
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

bool selfRestrainFullIteration(const Rule &rule, unsigned variableCount,
    RelianceStrategy strat, std::vector<size_t> &atomMapping, size_t index)
{
    size_t targetSize = rule.getHeads().size() + 1; // +1 because it can be unassigned

    for(atomMapping[index] = 0; atomMapping[index] < targetSize; atomMapping[index]++)
    {
        if(index == atomMapping.size() - 1)
        {
            if (restrainIsTimeout(true))
                return true;

            // atomMapping now contains a valid mapping
            std::vector<unsigned> currentMappingDomain;
            VariableAssignments currentAssignments(variableCount, 0);
            bool validAssignments = true;

            for (unsigned headToIndex = 0; headToIndex < atomMapping.size(); ++headToIndex)
            {
                unsigned headFromIndex = (unsigned)atomMapping[headToIndex];
                if (headFromIndex == 0)
                    continue;
                
                currentMappingDomain.push_back(headToIndex);

                const Literal &literalTo = rule.getHeads().at(headToIndex);
                const Literal &literalFrom =  rule.getHeads().at(headFromIndex - 1);

                if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId()
                    || !selfRestrainExtendAssignment(literalFrom, literalTo, currentAssignments))
                {
                    validAssignments = false;
                    break;
                }
            }

            if (currentMappingDomain.size() == 0 || !validAssignments)
                continue;

            if (selfRestrainCheck(currentMappingDomain, rule, currentAssignments) == RelianceCheckResult::True)
                return true;
        }
        else
        {
            if (selfRestrainFullIteration(rule, variableCount, strat, 
                atomMapping, index + 1))
            {
                return true;   
            }
        }
    }

    return false;
}

bool selfRestrainReliance(const Rule &rule, unsigned variableCount,
    RelianceStrategy strat)
{
    if ((strat & RelianceStrategy::BetterIterate) > 0)
    {
        std::vector<unsigned> mappingDomain;
        VariableAssignments assignments(variableCount, 0);

        return selfRestrainExtend(mappingDomain, rule, assignments, strat);
    }
    else
    {
        std::vector<size_t> atomMapping;
        atomMapping.resize(rule.getHeads().size(), 0);

        return selfRestrainFullIteration(rule, variableCount, 
            strat, atomMapping, 0);
    }
}

RelianceComputationResult computeRestrainReliances(const std::vector<Rule> &rules, RelianceStrategy strat, unsigned timeoutMilliSeconds)
{
    RelianceComputationResult result;
    result.graphs = std::pair<SimpleGraph, SimpleGraph>(SimpleGraph(rules.size()), SimpleGraph(rules.size()));
    
    std::vector<Rule> markedRules;
    markedRules.reserve(rules.size());
    
    std::vector<unsigned> variableCounts;
    variableCounts.reserve(rules.size());

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {
        const Rule &currentRule = rules[ruleIndex];

        unsigned variableCount = std::max(highestLiteralsId(currentRule.getHeads()), highestLiteralsId(currentRule.getBody()));
        variableCounts.push_back(variableCount + 1);
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

    std::unordered_map<PredId_t, std::vector<size_t>> headToMap;
    std::unordered_map<std::string, bool> resultCache;

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {   
        const Rule &markedRule = markedRules[ruleIndex];

        for (const Literal &currentLiteral : markedRule.getHeads())
        {
            bool containsExistentialVariable = false;
            for (size_t termIndex = 0; termIndex < currentLiteral.getTupleSize(); ++termIndex)
            {
                VTerm currentTerm = currentLiteral.getTermAtPos(termIndex);

                if ((int32_t)currentTerm.getId() < 0)
                {
                    containsExistentialVariable = true;
                    break;
                }
            }

            std::vector<size_t> &headToVector = headToMap[currentLiteral.getPredicate().getId()];
            if (containsExistentialVariable)
                headToVector.push_back(ruleIndex);
        }
    }

    std::chrono::system_clock::time_point timepointStart = std::chrono::system_clock::now();
    globalRestraintTimepointStart = timepointStart;
    globalRestraintTimeout = timeoutMilliSeconds;

    uint64_t numCalls = 0;

    enum class RelianceExecutionCommand {
        None,
        Continue,
        Return
    };

    auto relianceExecution = [&] (size_t ruleFrom, size_t ruleTo, std::unordered_set<size_t> &proccesedRules) -> RelianceExecutionCommand {
        if ((strat & RelianceStrategy::CutPairs) > 0)
        {
            if (proccesedRules.find(ruleTo) != proccesedRules.end())
                return RelianceExecutionCommand::Continue;

            proccesedRules.insert(ruleTo);
        }

        if (restrainIsTimeout(false))
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

        if (ruleFrom == ruleTo && rules[ruleFrom].isExistential())
        {
            std::vector<Rule> splitRules;
            splitIntoPieces(rules[ruleFrom], splitRules);

            if (splitRules.size() > 1)
            {
                result.graphs.first.addEdge(ruleFrom, ruleTo);
                result.graphs.second.addEdge(ruleTo, ruleFrom);
            
                return RelianceExecutionCommand::Continue;
            }
        }

        std::chrono::system_clock::time_point relianceExecutionStart = std::chrono::system_clock::now();
        bool isReliance = restrainReliance(markedRules[ruleFrom], variableCountFrom, markedRules[ruleTo], variableCountTo, strat);

        if (ruleFrom == ruleTo && rules[ruleFrom].isExistential() && !isReliance)
        {
            isReliance = selfRestrainReliance(markedRules[ruleFrom], variableCountFrom, strat);    
        }

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

        if (globalRestraintIsTimeout)
        {
            std::cout << "Timeout-Rule-Restraint: " 
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
                auto toIterator = headToMap.find(currentPredicate);

                if (toIterator == headToMap.end())
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