#include "vlog/reliances/reliances.h"

unsigned highestLiteralsId(const std::vector<Literal> &literalVector)
{
    unsigned result = 0;

    for (const Literal &literal: literalVector)
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
        
            if (currentTerm.getId() > result)
            {
                result = currentTerm.getId();
            }
        }
    }
    
    return result;
}

Rule markExistentialVariables(const Rule &rule)
{
    uint32_t ruleId = rule.getId();
    std::vector<Literal> body = rule.getBody();
    std::vector<Literal> heads;

    std::vector<Var_t> existentialVariables = rule.getExistentialVariables();
    for (const Literal &literal: rule.getHeads())
    {
        VTuple *tuple = new VTuple(literal.getTupleSize());

        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);;

            if (currentTerm.getId() > 0 && std::find(existentialVariables.begin(), existentialVariables.end(), currentTerm.getId()) != existentialVariables.end())
            {
                currentTerm.setId(-currentTerm.getId());
            }

            tuple->set(currentTerm, termIndex);
        }

        heads.push_back(Literal(literal.getPredicate(), *tuple, literal.isNegated()));
    }

    return Rule(ruleId, heads, body);
}

TermInfo getTermInfoUnify(VTerm term, const VariableAssignments &assignments, RelianceRuleRelation relation)
{
    TermInfo result;
    result.relation = relation;
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
            result.groupId = assignments.getGroupId(term.getId(), relation);
            result.constant = assignments.getConstant(term.getId(), relation);
        }
    }

    return result;
}

TermInfo getTermInfoModels(VTerm term, const VariableAssignments &assignments, RelianceRuleRelation relation,
    bool alwaysDefaultAssignExistentials)
{
    TermInfo result;
    result.relation = relation;
    result.termId = (int32_t)term.getId();

    if (term.getId() == 0)
    {
        result.type = TermInfo::Types::Constant;
        result.constant = (int64_t)term.getValue();
    }
    else if ((int32_t)term.getId() > 0)
    {
        result.type = TermInfo::Types::Universal;

        result.groupId = assignments.getGroupId(term.getId(), relation);
        result.constant = assignments.getConstant(term.getId(), relation);
    }
    else
    {
        result.type = TermInfo::Types::Existential;

        if (alwaysDefaultAssignExistentials) //they are different nulls then the own assigned during unification
        {
            result.constant = (int32_t)term.getId() - assignments.getVariableToOffset();
        }
        else
        {
            result.groupId = assignments.getGroupId(term.getId(), relation);
            result.constant = assignments.getConstant(term.getId(), relation);
        
            if (result.groupId == NOT_ASSIGNED && result.constant == NOT_ASSIGNED)
            {
                result.constant = (int32_t)term.getId();
            }
        }
    }

    return result;
}

bool termsEqual(const TermInfo &termLeft, const TermInfo &termRight)
{
    if (termLeft.type == TermInfo::Types::Constant || termRight.type == TermInfo::Types::Constant)
        return termLeft.constant == termRight.constant;

    if (termLeft.constant != NOT_ASSIGNED && termLeft.constant == termRight.constant)
        return true;
    
    if (termLeft.groupId != NOT_ASSIGNED && termLeft.groupId == termRight.groupId)
        return true;

    if (termLeft.groupId == NOT_ASSIGNED && termRight.groupId == NOT_ASSIGNED
        && termLeft.termId == termRight.termId && termLeft.relation == termRight.relation)
        return true;

    return false;
}

bool unifyTerms(const TermInfo &fromInfo, const TermInfo &toInfo, VariableAssignments &assignments)
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
        assignments.connectVariables(fromInfo.termId, toInfo.termId);
    }

    return true;
}

void prepareExistentialMappings(const std::vector<Literal> &right, RelianceRuleRelation rightRelation,
    const VariableAssignments &assignments,
    std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &existentialMappings)
{
    existentialMappings.clear();

    for (unsigned rightIndex = 0; rightIndex < right.size(); ++rightIndex)
    {      
        const Literal &rightLiteral = right[rightIndex];
        unsigned tupleSize = rightLiteral.getTupleSize(); 

        for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
        {
            VTerm rightTerm = rightLiteral.getTermAtPos(termIndex);
            TermInfo rightInfo = getTermInfoModels(rightTerm, assignments, rightRelation, false);
     
            if (rightInfo.type == TermInfo::Types::Existential)
            {
                existentialMappings.emplace_back();
                break;
            }
        }
    }
}

bool possiblySatisfied(const std::vector<Literal> &right, std::initializer_list<std::vector<Literal>> leftParts)
{
    for (const Literal &currentRight : right)
    {
        bool isPresent = false;
        for (const std::vector<Literal> &left : leftParts)
        {
            for (const Literal &currentLeft : left)
            {
                if (currentLeft.getPredicate().getId() == currentRight.getPredicate().getId())
                {
                    isPresent = true;
                    break;
                }
            }

            if (isPresent)
                break;
        }

        if(!isPresent)
            return false;
    }

    return true;
}


bool possiblySatisfied(const std::vector<Literal> &right, std::initializer_list<std::vector<Literal>> leftParts, const std::vector<std::reference_wrapper<const Literal>> &leftRef)
{
    for (const Literal &currentRight : right)
    {
        bool isPresent = false;
        for (const std::vector<Literal> &left : leftParts)
        {
            for (const Literal &currentLeft : left)
            {
                if (currentLeft.getPredicate().getId() == currentRight.getPredicate().getId())
                {
                    isPresent = true;
                    break;
                }
            }

            if (isPresent)
                break;
        }

         for (const Literal &currentLeft : leftRef)
            {
                if (currentLeft.getPredicate().getId() == currentRight.getPredicate().getId())
                {
                    isPresent = true;
                    break;
                }
            }

        if(!isPresent)
            return false;
    }

    return true;
}

bool isMappingConsistent(std::unordered_map<int64_t, TermInfo> &mapping, 
    const std::unordered_map<int64_t, TermInfo> &compare)
{
    for (auto compareIterator : compare)
    {
        auto mappingIterator = mapping.find(compareIterator.first);
        if (mappingIterator == mapping.end())
        {
            mapping[compareIterator.first] = compareIterator.second;
        }
        else
        {
            if (!termsEqual(mappingIterator->second, compareIterator.second))
                return false;
        }
    }

    return true;
}

bool checkConsistentExistentialDeep(const std::unordered_map<int64_t, TermInfo> &currentMapping, 
    const std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &mappings,
    size_t nextIndex)
{
    if (nextIndex >= mappings.size())
        return true;

    for (const std::unordered_map<int64_t, TermInfo> &nextMap : mappings[nextIndex])
    {
        std::unordered_map<int64_t, TermInfo> updatedMap = currentMapping;
        if (isMappingConsistent(updatedMap, nextMap))
        {
            if (checkConsistentExistentialDeep(updatedMap, mappings, nextIndex + 1))
                return true;
        }
    }

    return false;
}

bool checkConsistentExistential(const std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &mappings)
{
    if (mappings.size() < 1)
        return false;

    for (auto &possibleMappings : mappings)
    {
        if (possibleMappings.size() == 0)
            return false;
    }

    for (const std::unordered_map<int64_t, TermInfo> &startMap : mappings[0])
    {
        if (checkConsistentExistentialDeep(startMap, mappings, 1))
            return true;
    }

    return false;
}

std::pair<SimpleGraph, SimpleGraph> combineGraphs(
    const SimpleGraph &positiveGraph, const SimpleGraph &restraintGraph)
{
    SimpleGraph unionGraph(positiveGraph.nodes.size()), unionGraphTransposed(positiveGraph.nodes.size());

    auto copyGraph = [] (const SimpleGraph &simple, SimpleGraph &outUnion, SimpleGraph &outUnionTransposed) -> void
    {
        for (unsigned fromNode = 0; fromNode < simple.nodes.size(); ++fromNode)
        {
            for (unsigned toNode : simple.edges[fromNode])
            {
                outUnion.addEdge(fromNode, toNode);
                outUnionTransposed.addEdge(toNode, fromNode);
            }
        }
    };

    copyGraph(positiveGraph, unionGraph, unionGraphTransposed);
    copyGraph(restraintGraph, unionGraph, unionGraphTransposed);
    
    return std::make_pair(unionGraph, unionGraphTransposed);
}

void createPiece(const std::vector<Literal> &heads, unsigned currentLiteralIndex, std::vector<bool> &literalsInPieces, const std::vector<uint32_t> &existentialVariables, 
    std::vector<bool> &visited, std::vector<Literal> &result)
{
    const Literal &currentLiteral = heads[currentLiteralIndex];
    literalsInPieces[currentLiteralIndex] = true;
    visited[currentLiteralIndex] = true;
    result.push_back(currentLiteral);
    
    for (unsigned termIndex = 0; termIndex < currentLiteral.getTupleSize(); ++termIndex)
    {
        VTerm currentTerm = currentLiteral.getTermAtPos(termIndex);

        if (std::find(existentialVariables.begin(), existentialVariables.end(), currentTerm.getId()) == existentialVariables.end())
            continue;

        for (unsigned searchIndex = 0; searchIndex < heads.size(); ++searchIndex)
        {
            if (searchIndex == currentLiteralIndex)
                continue;

            if (literalsInPieces[searchIndex])
                continue;

            if (visited[searchIndex])
                continue;

            const Literal &currentSearchedLiteral = heads[searchIndex];
            for (unsigned searchTermIndex = 0; searchTermIndex < currentSearchedLiteral.getTupleSize(); ++searchTermIndex)
            {
                VTerm currentSearchedTerm = currentSearchedLiteral.getTermAtPos(searchTermIndex);
            
                if (currentTerm.getId() == currentSearchedTerm.getId())
                {
                    createPiece(heads, searchIndex, literalsInPieces, existentialVariables, visited, result);
                }
            }
        }
    }
}

void splitIntoPieces(const Rule &rule, std::vector<Rule> &outRules)
{
    uint32_t ruleId = outRules.size();
    std::vector<Literal> body = rule.getBody();
    std::vector<Literal> heads = rule.getHeads();

    std::vector<Var_t> existentialVariables = rule.getExistentialVariables();

    std::vector<bool> literalsInPieces; literalsInPieces.resize(heads.size(), false);

    for (unsigned literalIndex = 0; literalIndex < heads.size(); ++literalIndex)
    {
        const Literal &currentLiteral = heads[literalIndex];
        
        if (literalsInPieces[literalIndex])
            continue;

        std::vector<bool> visited; visited.resize(heads.size(), false);
        std::vector<Literal> newPiece;
        createPiece(heads, literalIndex, literalsInPieces, existentialVariables, visited, newPiece);

        if (newPiece.size() > 0)
        {
            outRules.emplace_back(ruleId, newPiece, body);
            ++ruleId;
        }   
    }
}

void fillOrder(const SimpleGraph &graph, unsigned node, 
    std::vector<unsigned> &visited, std::stack<unsigned> &stack, std::vector<bool> *activeNodes = nullptr)
{
    visited[node] = 1;
  
    for(unsigned adjacentNode : graph.edges[node])
    {
        if (activeNodes != nullptr && !(*activeNodes)[adjacentNode])
            continue;

        if(visited[adjacentNode] == 0)
            fillOrder(graph, adjacentNode, visited, stack);
    }
   
    stack.push(node);
}

void fillOrderNonRecursive(const SimpleGraph &graph, unsigned node, 
    std::vector<unsigned> &visited, std::stack<unsigned> &stack, std::vector<bool> *activeNodes = nullptr)
{
    std::stack<std::pair<unsigned, bool>> dfsStack;
    dfsStack.push(std::make_pair(node, false));

    while (!dfsStack.empty())
    {
        unsigned currentNode = dfsStack.top().first;
        bool currentNodePost = dfsStack.top().second;
        dfsStack.pop();

        if (currentNodePost)
        {
            stack.push(currentNode);
        }
        else
        {
            if (visited[currentNode] == 1)
                continue;

            visited[currentNode] = 1;

            dfsStack.push(std::make_pair(currentNode, true));

            for(unsigned adjacentNode : graph.edges[currentNode])
            {
                if (activeNodes != nullptr && !(*activeNodes)[adjacentNode])
                    continue;

                if (visited[adjacentNode] == 0)
                    dfsStack.push(std::make_pair(adjacentNode, false));
            }
        }        
    }
}

void dfsUntilNonRecursive(const SimpleGraph &graph, unsigned node, 
    std::vector<unsigned> &visited, std::vector<unsigned> &currentGroup,
    std::vector<bool> *activeNodes = nullptr)
{
    std::stack<unsigned> dfsStack;
    dfsStack.push(node);

    while (!dfsStack.empty())
    {
        unsigned currentNode = dfsStack.top();
        dfsStack.pop();

        if (visited[currentNode] == 1)
            continue;

        currentGroup.push_back(currentNode);

        visited[currentNode] = 1;

        for(unsigned adjacentNode : graph.edges[currentNode])
        {
            if (activeNodes != nullptr && !(*activeNodes)[adjacentNode])
                continue;

            if (visited[adjacentNode] == 0)
                dfsStack.push(adjacentNode);
        }
    }
}

void dfsUntil(const SimpleGraph &graph, unsigned node, 
    std::vector<unsigned> &visited, std::vector<unsigned> &currentGroup,
    std::vector<bool> *activeNodes = nullptr)
{
    visited[node] = 1;
    currentGroup.push_back(node);

    for(unsigned adjacentNode : graph.edges[node])
    {
        if (activeNodes != nullptr && !(*activeNodes)[adjacentNode])
            continue;

        if(visited[adjacentNode] == 0)
            dfsUntil(graph, adjacentNode, visited, currentGroup, activeNodes);
    }
}

RelianceGroupResult computeRelianceGroups(
    const SimpleGraph &graph, const SimpleGraph &graphTransposed,
    std::vector<bool> *activeNodes)
{
    RelianceGroupResult result;
    result.assignments.resize(graph.numberOfInitialNodes, -1);

    if (graph.nodes.size() == 0)
        return result;

    std::stack<unsigned> nodeStack;
    std::vector<unsigned> visited;
    visited.resize(graph.numberOfInitialNodes, 0);

    // for (unsigned node = 0; node < graph.numberOfInitialNodes; ++node)
    for (int node = graph.numberOfInitialNodes - 1; node >= 0 ; --node)
    {
        if (activeNodes != nullptr && !(*activeNodes)[node])
            continue;

        if (visited[node] == 0)
            fillOrderNonRecursive(graph, node, visited, nodeStack, activeNodes);
    }

    std::fill(visited.begin(), visited.end(), 0);

    std::vector<unsigned> currentGroup;
    bool minFound = false;
    while (!nodeStack.empty())
    {
        unsigned currentNode = nodeStack.top();
        nodeStack.pop();
  
        if (visited[currentNode] == 0)
        {
            dfsUntilNonRecursive(graphTransposed, currentNode, visited, currentGroup, activeNodes);

            if (currentGroup.size() > 0)
            {
                unsigned currentGroupIndex = result.groups.size();
                result.groups.push_back(currentGroup);

                for (unsigned member : currentGroup)
                {
                    result.assignments[member] = currentGroupIndex;
                
                    if (!minFound && activeNodes != nullptr)
                    {
                        bool hasPredeccessors = false; 
                        for (unsigned pred : graphTransposed.edges[member])
                        {
                            if (!(*activeNodes)[pred])
                              continue;

                            if (std::find(currentGroup.begin(), currentGroup.end(), pred) == currentGroup.end())
                            {
                                hasPredeccessors = true;
                                break;
                            }
                        }

                        if (!hasPredeccessors)
                        {
                            result.minimumGroup = result.groups.size() - 1;
                            minFound = true;
                        }
                    }
                  
                    // result.hasPredeccessors.push_back(hasPredeccessors);
                }

                currentGroup.clear();
            }
        }
    }

    return result;
}

CoreStratifiedResult isCoreStratified(const SimpleGraph & unionGraph, const SimpleGraph & unionGraphTransposed, const SimpleGraph &restrainingGraph)
{
    CoreStratifiedResult result;
    result.stratified = true;

    RelianceGroupResult staticRestrainedGroups = computeRelianceGroups(unionGraph, unionGraphTransposed);
    
    for (auto &group : staticRestrainedGroups.groups)
    {
        bool isGroupRestrained = false;

        for (unsigned ruleIndex : group)
        {
            for (unsigned restrainedRule : restrainingGraph.edges[ruleIndex])
            {
                if (std::find(group.begin(), group.end(), restrainedRule) != group.end())
                {
                    isGroupRestrained = true;

                    break;
                }    
            }

            if (isGroupRestrained)
                break;
        }

        if (isGroupRestrained)
        {
            result.stratified = false;
            
            if (group.size() > result.biggestRestrainedGroupSize)
            {
                result.biggestRestrainedGroupSize = (unsigned)group.size();
            }

            ++result.numberOfRestrainedGroups;
            result.numberOfRulesInRestrainedGroups += (unsigned)group.size();

            if (result.smallestRestrainedComponent.size() == 0 || result.smallestRestrainedComponent.size() > group.size())
                result.smallestRestrainedComponent = group;
        }
    }

    return result;
}

unsigned addPredicate(RuleHashInfo &info, PredId_t predId)
{
    auto iterator = info.predIdToLocal.find(predId);
    if (iterator == info.predIdToLocal.end())
    {
        info.predIdToLocal[predId] = info.localPredicate;
        return info.localPredicate++;
    }
    else
    {
        return iterator->second;
    }
};

RuleHashInfo ruleHashInfoFirst(const Rule &rule)
{
    RuleHashInfo result;

    auto literalToString = [&] (const Literal &literal) -> std::string {
        std::string result;
        result.reserve(100);

        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
        
            if ((int32_t)currentTerm.getId() > 0)
            {
                result += std::to_string(currentTerm.getId()) + ",";
            }
            else
            {
                result += std::to_string(-1 * (int64_t)currentTerm.getValue()) + ",";
            }
        }

        return result;
    };

    std::string literalStream, predicateStream;
    literalStream.reserve(1000); predicateStream.reserve(1000);

    for (const Literal &currentLiteral : rule.getHeads())
    {
        unsigned currentPredId = addPredicate(result, currentLiteral.getPredicate().getId());
        predicateStream += std::string("|") + std::to_string(currentPredId);

        literalStream += literalToString(currentLiteral) + ";";
    }

    literalStream += "_";

    for (const Literal &currentLiteral : rule.getBody())
    {
        unsigned currentPredId = addPredicate(result, currentLiteral.getPredicate().getId());
        predicateStream += std::string("|") + std::to_string(currentPredId);

        literalStream += literalToString(currentLiteral) + ";";
    }

    result.firstRuleString = literalStream + predicateStream;
    result.secondRuleString = literalStream;

    return result;
}

std::string rulePairHash(RuleHashInfo ruleFromInfo, const RuleHashInfo &ruleToInfo, const Rule &rule)
{
    std::string result = ruleFromInfo.firstRuleString + ruleToInfo.secondRuleString;

    for (const Literal &currentLiteral : rule.getHeads())
    {
        unsigned currentPredId = addPredicate(ruleFromInfo, currentLiteral.getPredicate().getId());
        result += std::string("|") + std::to_string(currentPredId);
    }

    for (const Literal &currentLiteral : rule.getBody())
    {
        unsigned currentPredId = addPredicate(ruleFromInfo, currentLiteral.getPredicate().getId());
        result += std::string("|") + std::to_string(currentPredId);
    }

    return result;

    // std::unordered_map<PredId_t, unsigned> PredIdToLocal;
    // unsigned LocalPredicate = 0;

    // 

    // auto literalToString = [&] (const Literal &literal) -> std::string {
    //     std::stringstream Stream;
    //     unsigned currentPredId = addPredicate(literal.getPredicate().getId());
    //     Stream << currentPredId << '(';
        
    //     for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
    //     {
    //         VTerm currentTerm = literal.getTermAtPos(termIndex);
        
    //         if ((int32_t)currentTerm.getId() > 0)
    //         {
    //             Stream << currentTerm.getId() << ",";
    //         }
    //         else
    //         {
    //             Stream << -1 * (int64_t)currentTerm.getValue() << ",";
    //         }
    //     }

    //     Stream << ")";
    //     return Stream.str();
    // };

    // std::stringstream result;
    // for (const Literal &currentLiteral : ruleFrom.getHeads())
    // {
    //     result << literalToString(currentLiteral);
    // }
    // result << "_";
    // for (const Literal &currentLiteral : ruleFrom.getBody())
    // {
    //     result << literalToString(currentLiteral);
    // }
    // result << "_";
    // for (const Literal &currentLiteral : ruleTo.getHeads())
    // {
    //     result << literalToString(currentLiteral);
    // }
    // result << "_";
    // for (const Literal &currentLiteral : ruleTo.getBody())
    // {
    //     result << literalToString(currentLiteral);
    // }

    // return result.str();
}