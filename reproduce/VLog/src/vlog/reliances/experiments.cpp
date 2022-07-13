#include <vlog/reliances/experiments.h>


SimpleGraph createSubgraph(const SimpleGraph &originalGraph, std::vector<unsigned> component)
{
    SimpleGraph result(originalGraph.numberOfInitialNodes);

    std::sort(component.begin(), component.end());

    for (unsigned node : component)
    {
        for (unsigned successor : originalGraph.edges[node])
        {
            if (!std::binary_search(component.begin(), component.end(), successor))
                continue;

            result.addEdge(node, successor);
        }
    } 

    return result;
}

size_t minIndex(const std::vector<size_t> &distances, const std::vector<unsigned> &considered)
{
    unsigned minIndex = 0; 
    size_t minDistance = std::numeric_limits<size_t>::max();

    for (unsigned index = 0; index < distances.size(); ++index)
    {
        if (distances[index] < minDistance && considered[index] == 0)
        {
            minDistance = distances[index];
            minIndex = index;
        }
    }

    return minIndex;
}

void printShortestCycle(const SimpleGraph &positiveGraph, const SimpleGraph &positiveGraphTransposed,
    const SimpleGraph &restraintGraph, const SimpleGraph &restraintgraphTransposed,
    const std::vector<unsigned> &component,
    const std::vector<Rule> &rules, Program *program, EDBLayer *db)
{
    SimpleGraph positiveSubGraph = createSubgraph(positiveGraph, component);
    SimpleGraph restraintSubGraph = createSubgraph(restraintGraph, component);

    std::cout << "print cycles?" << component.size() << std::endl;

    std::vector<unsigned> smallestCycle;

    for (unsigned sourceNode : component)
    {
        if (restraintgraphTransposed.edges[sourceNode].size() == 0)
            continue;

        std::vector<size_t> distances;
        std::vector<unsigned> predecessors;
        std::vector<unsigned> considered;

        distances.resize(positiveGraph.numberOfInitialNodes, std::numeric_limits<size_t>::max());
        predecessors.resize(positiveGraph.numberOfInitialNodes, std::numeric_limits<unsigned>::max());
        considered.resize(positiveGraph.numberOfInitialNodes, 0);
    
        distances[sourceNode] = 0;

        for (size_t count = 0; count < component.size() - 1; ++count)
        {
            unsigned currentNode = minIndex(distances, considered);
            if (distances[currentNode] == std::numeric_limits<size_t>::max())
                break;

            considered[currentNode] = 1;

            for (unsigned adjacent : positiveSubGraph.edges[currentNode])
            {
                if (considered[adjacent] == 0 && distances[currentNode] + 1 < distances[adjacent])
                {
                    distances[adjacent] = distances[currentNode] + 1;
                    predecessors[adjacent] = currentNode;
                }
            }

            for (unsigned adjacent : restraintSubGraph.edges[currentNode])
            {
                if (considered[adjacent] == 0 && distances[currentNode] + 1 < distances[adjacent])
                {
                    distances[adjacent] = distances[currentNode] + 1;
                    predecessors[adjacent] = currentNode;
                }
            }
        }

        unsigned minPred = 0;
        unsigned minPredDistance = std::numeric_limits<unsigned>::max();
        for (unsigned pred : restraintgraphTransposed.edges[sourceNode])
        {
            if (distances[pred] < minPredDistance)
            {
                minPredDistance = distances[pred];
                minPred = pred;
            }
        }

        std::vector<unsigned> currentCycle;
        currentCycle.push_back(sourceNode);

        if (minPred == sourceNode)
        {
            smallestCycle = currentCycle;
            break;
        }

        unsigned currentPred = minPred;
        bool isSmaller = true;
        while (currentPred != sourceNode)
        {
            currentCycle.push_back(currentPred);
           
            if (smallestCycle.size() > 2 && smallestCycle.size() != 0 && currentCycle.size() > smallestCycle.size())
            {
                isSmaller = false;
                break;
            }

            currentPred = predecessors[currentPred];
        }

        if (isSmaller)
            smallestCycle = currentCycle; 
    }

    for (unsigned cycleIndex = 0; cycleIndex < smallestCycle.size(); ++cycleIndex)
    {
        std::cout << (cycleIndex + 1) << " - " << rules[smallestCycle[cycleIndex]].tostring(program, db) << '\n';
    }
}

void experimentCoreStratified(const std::string &rulesPath, bool pieceDecomposition, RelianceStrategy strat, unsigned timeoutMilliSeconds, bool printCycles)
{
    std::cout << "Launched coreStratified experiment with parameters " << '\n';
    std::cout << "\t" << "Path: " << rulesPath << '\n';
    std::cout << "\t" << "Piece: " << ((pieceDecomposition) ? "true" : "false") << '\n';
    std::cout << "\t" <<  "Strat: " << strat << std::endl;

    EDBConf emptyConf("", false);
    EDBLayer edbLayer(emptyConf, false);

    Program program(&edbLayer);
    std::string errorString = program.readFromFile(rulesPath, false);
    if (!errorString.empty()) {
        LOG(ERRORL) << errorString;
        return;
    }

    const std::vector<Rule> &allOriginalRules = program.getAllRules();

    std::vector<Rule> allPieceDecomposedRules;
    if (pieceDecomposition)
    {
        for (const Rule &currentRule : allOriginalRules)
        {
            splitIntoPieces(currentRule, allPieceDecomposedRules);
        }
    }

    const std::vector<Rule> &allRules = (pieceDecomposition) ? allPieceDecomposedRules : allOriginalRules;
    
    std::cout << "#Rules: " << allOriginalRules.size() << '\n';
    if (pieceDecomposition)
        std::cout << "#PieceRules: " << allPieceDecomposedRules.size() << '\n';

    RelianceComputationResult positiveResult = computePositiveReliances(allRules, strat, timeoutMilliSeconds);
    std::pair<SimpleGraph, SimpleGraph> positiveGraphs = positiveResult.graphs;

     if (!positiveResult.timeout)
    {
        std::cout << "Time-Positive: " << positiveResult.timeMilliSeconds << '\n';
    }   

    RelianceComputationResult restrainResult = computeRestrainReliances(allRules, strat, timeoutMilliSeconds);
    std::pair<SimpleGraph, SimpleGraph> restrainingGraphs = restrainResult.graphs;
    
    bool timeout = positiveResult.timeout || restrainResult.timeout;

    std::cout << "Calls-Positive: " << positiveResult.numberOfCalls << '\n';
    std::cout << "Calls-Restraint: " << restrainResult.numberOfCalls << '\n';
    std::cout << "Calls-Overall: " << positiveResult.numberOfCalls + restrainResult.numberOfCalls << '\n';
    
    std::cout << "Longest-Positive: " << positiveResult.timeLongestPairMicro / 1000.0 << '\n';
    std::cout << "Longest-Pair-Positive: " << positiveResult.longestPairString << '\n';
    std::cout << "Longest-Restraint: " << restrainResult.timeLongestPairMicro / 1000.0 << '\n';
    std::cout << "Longest-Pair-Restraint: " << restrainResult.longestPairString << '\n';
    std::cout << "Longest-Overall: " << (positiveResult.timeLongestPairMicro + restrainResult.timeLongestPairMicro) / 1000.0 << '\n';   

    if (!restrainResult.timeout)
    {
        std::cout << "Time-Restraint: " << restrainResult.timeMilliSeconds << '\n';
    } 

    if (!timeout)
    {
        std::cout << "Time-Overall: " << positiveResult.timeMilliSeconds + restrainResult.timeMilliSeconds << '\n';

        std::pair<SimpleGraph, SimpleGraph> unionGraphs = combineGraphs(positiveGraphs.first, restrainingGraphs.first);
        CoreStratifiedResult coreStratifiedResult = isCoreStratified(unionGraphs.first, unionGraphs.second, restrainingGraphs.first);
        bool coreStratified = coreStratifiedResult.stratified;
        std::cout << "Core-Stratified: " << ((coreStratified) ? "1" : "0") << '\n';

        if (!coreStratified)
        {
            std::cout << "NumberRestrainedGroups: " << coreStratifiedResult.numberOfRestrainedGroups << '\n';
            std::cout << "BiggestRestrainedGroup-Abs: " << coreStratifiedResult.biggestRestrainedGroupSize << '\n';
            std::cout << "BiggestRestrainedGroup-Rel: " << (coreStratifiedResult.biggestRestrainedGroupSize / (float)allRules.size()) << '\n';
            std::cout << "RulesInRestrainedGroups-Rel: " << (coreStratifiedResult.numberOfRulesInRestrainedGroups / (float)allRules.size()) << '\n';
        
            if (printCycles)
            {
                printShortestCycle(positiveGraphs.first, positiveGraphs.second, restrainingGraphs.first, restrainingGraphs.second, 
                    coreStratifiedResult.smallestRestrainedComponent, allRules, &program, &edbLayer);
            }
        }
    }

    std::cout << "Timeout: " << ((timeout) ? "1" : "0") << '\n';
}

void experimentCycles(const std::string &rulesPath, const std::string &algorithm, bool splitPositive, unsigned timeoutMilliSeconds)
{
    std::cout << "Launched Cycles experiment with parameters " << '\n';
    std::cout << "\t" << "Algorithm: " << algorithm<< '\n';

    EDBConf emptyConf("", false);
    EDBLayer edbLayer(emptyConf, false);

    Program initialProgram(&edbLayer);
    std::string errorString = initialProgram.readFromFile(rulesPath, false);
    if (!errorString.empty()) {
        LOG(ERRORL) << errorString;
        return;
    }

    const std::vector<Rule> &allOriginalRules = initialProgram.getAllRules();
    RelianceGroupResult positiveGroupsResult;
    double timeRelianceMilliseconds = 0.0;

    RelianceComputationResult positiveResult;

    if (splitPositive)
    {
        auto relianceStart = std::chrono::system_clock::now();

        positiveResult = computePositiveReliances(allOriginalRules, RelianceStrategy::Full, timeoutMilliSeconds);
        std::pair<SimpleGraph, SimpleGraph> &positiveGraphs = positiveResult.graphs;

        timeRelianceMilliseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - relianceStart).count() / 1000.0;
    
        if (positiveResult.timeout)
        {
            std::cout << "Timeout: 1" << '\n';
            return;
        }
        else
        {
            std::cout << "Reliance-Time: " << timeRelianceMilliseconds << " ms" << '\n';
        }

        positiveGroupsResult = computeRelianceGroups(positiveGraphs.first, positiveGraphs.second);    
    }

    std::vector<std::string> ruleStrings;
    ruleStrings.reserve(allOriginalRules.size());
    for (const Rule &currentRule : allOriginalRules)
    {
        ruleStrings.push_back(currentRule.toprettystring(&initialProgram, &edbLayer));
    }

    auto cycleStart = std::chrono::system_clock::now();
    bool result = true;
    
    if (splitPositive)
    {
        for (const std::vector<unsigned> &group : positiveGroupsResult.groups)
        {
            if (group.size() == 1 && !positiveResult.graphs.first.containsEdge(group[0], group[0]))
                continue;

            EDBConf currentEmptyConf("", false);
            EDBLayer currentEdbLayer(emptyConf, false);

            Program currentProgram(&currentEdbLayer);
            bool containsExisitential = false;
            for (unsigned ruleIndex : group)
            //for (unsigned ruleIndex = 0; ruleIndex < allOriginalRules.size(); ++ruleIndex)
            {
                const Rule &currentRule = allOriginalRules[ruleIndex];

                if (currentRule.isExistential())
                    containsExisitential = true;

                currentProgram.readFromString(ruleStrings[ruleIndex], false);
                //currentProgram.addRule(currentRule.getHeads(), currentRule.getBody());
            }

            if (!containsExisitential)
                continue;

            int checkResult = Checker::check(currentProgram, algorithm, currentEdbLayer);
        
            if (checkResult == 0)
            {
                result = false;
                break;
            }
        }
    }
    else
    {
        int checkResult = Checker::check(initialProgram, algorithm, edbLayer);

        result = (checkResult == 1);
    }
    

    double timeCycleMilliseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - cycleStart).count() / 1000.0;
    double timeOverallMilliseconds = timeRelianceMilliseconds + timeCycleMilliseconds;

    std::cout << "Acyclic: " << ((result) ? "1" : "0") << '\n';
    std::cout << "Cycle-Time: " << timeCycleMilliseconds << " ms" << '\n';
    std::cout << "Overall-Time: " << timeOverallMilliseconds << " ms" << '\n';
    std::cout << "Timeout: 0" << '\n';
}

void experimentGRD(const std::string &rulesPath, unsigned timeoutMilliSeconds)
{
    EDBConf emptyConf("", false);
    EDBLayer edbLayer(emptyConf, false);

    Program program(&edbLayer);
    std::string errorString = program.readFromFile(rulesPath, false);
    if (!errorString.empty()) {
        LOG(ERRORL) << errorString;
        return;
    }

    const std::vector<Rule> &allRules = program.getAllRules();

    auto experimentStart = std::chrono::system_clock::now();
    
    RelianceComputationResult positiveResult = computePositiveReliances(allRules, RelianceStrategy::Full, timeoutMilliSeconds);
    
    if (positiveResult.timeout)
    {
        std::cout << "Timeout: 1" << '\n';
        return;
    }
    
    std::pair<SimpleGraph, SimpleGraph> positiveGraphs = positiveResult.graphs;
    RelianceGroupResult positiveGroupsResult = computeRelianceGroups(positiveGraphs.first, positiveGraphs.second);  

    bool isAcyclic = true;
    for (const std::vector<unsigned> &group : positiveGroupsResult.groups)
    {
        if (group.size() > 1)
        {
            isAcyclic = false;
            break;
        }
        else
        {
            if (positiveGraphs.first.containsEdge(group[0], group[0]))
            {
                isAcyclic = false;
                break;
            }
        }
    }   

    double timeMilliSeconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - experimentStart).count() / 1000.0;   

    size_t totalNumberOfEdges = 0;
    for (const auto &successors : positiveGraphs.first.edges)
    {
        totalNumberOfEdges += successors.size();
    }

    std::cout << "Acyclic: " << (isAcyclic ? "1" : "0") << '\n';
    std::cout << "NumberOfEdges: " << totalNumberOfEdges << '\n';
    std::cout << "Time: " << timeMilliSeconds << " ms" << '\n'; 
    std::cout << "Time-Positive: " << positiveResult.timeMilliSeconds << " ms" << '\n';
}