
#include <vlog/reliances/tests.h>

bool graphContained(const SimpleGraph &left, const SimpleGraph &right)
{
    for (size_t from = 0; from < left.numberOfInitialNodes; ++from)
    {
        for (size_t to : left.edges[from])
        {
            if (!right.containsEdge(from, to))
                return false;
        }
    }

    return true;
}

bool performSingleTest(const std::string &ruleFolder, const TestCase &test, RelianceStrategy strat)
{
    EDBConf emptyConf("", false);
    EDBLayer edbLayer(emptyConf, false);

    Program program(&edbLayer);
    std::string errorString = program.readFromFile(ruleFolder + "/" + test.name, false);
    if (!errorString.empty()) {
        LOG(ERRORL) << errorString;
        return false;
    }

    const std::vector<Rule> &allRules = program.getAllRules();

    std::pair<SimpleGraph, SimpleGraph> graphs;

    if (test.type == TestCase::Type::Positive)
    {
        RelianceComputationResult compResult = computePositiveReliances(allRules, strat);
        graphs = compResult.graphs;
    }
    else 
    {
        RelianceComputationResult compResult = computeRestrainReliances(allRules, strat);
        graphs = compResult.graphs;
    }

    return (graphContained(graphs.first, test.expected) && graphContained(test.expected, graphs.first));
}

typedef std::vector<std::vector<size_t>> Edges;

void performTests(const std::string &ruleFolder, RelianceStrategy strat)
{
    std::vector<TestCase> cases;
    cases.emplace_back(TestCase::Type::Positive, "pos_basic.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_basic_2.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_thesis.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_null_1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_null_2.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_phi2Ia.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi1Ia_phi1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi1Ia_phi1phi22.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi1Ia_phi22.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi2Ib_phi1.dl", Edges{{}, {0}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi2Ib_phi22.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi2Ib_psi1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_unif_1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_unif_2.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi1Ia_ext.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_phi2Ia_ext.dl", Edges{{1}, {}});

    cases.emplace_back(TestCase::Type::Restraint, "res_basic_1.dl", Edges{{}, {0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_basic_2.dl", Edges{{}, {0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_null_1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_null_2.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_null_3.dl", Edges{{1, 0}, {1}});
    cases.emplace_back(TestCase::Type::Restraint, "res_null_4.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_psi2Iam_phi2.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_psi1Ibm_psi2.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_psi1Ibm_phi1phi2.dl", Edges{{}, {1}});
    cases.emplace_back(TestCase::Type::Restraint, "res_psi1Ibm_phi1phi2psi22.dl", Edges{{}, {0}}); 
    cases.emplace_back(TestCase::Type::Restraint, "res_altPresent.dl", Edges{{}, {0}}); 
    
    cases.emplace_back(TestCase::Type::Restraint, "res_self_markus.dl", Edges{{0}}); 
    cases.emplace_back(TestCase::Type::Restraint, "res_self_markus+.dl", Edges{{0}}); 
    cases.emplace_back(TestCase::Type::Restraint, "res_self_trivial.dl", Edges{{0}}); 
    cases.emplace_back(TestCase::Type::Restraint, "res_self_twice.dl", Edges{{0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_self_Im.dl", Edges{{0}}); 

    cases.emplace_back(TestCase::Type::Restraint, "res_larry_1.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_2.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_3.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_4.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_5.dl", Edges{{0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_6.dl", Edges{{0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_7.dl", Edges{{0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_8.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_9.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_10.dl", Edges{{0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_11.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_12.dl", Edges{{0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_13.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_14.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_15.dl", Edges{{}});
    cases.emplace_back(TestCase::Type::Restraint, "res_larry_16.dl", Edges{{}});

    size_t numberOfFailedTests = 0;

    for (const TestCase &test : cases)
    {
        if (!performSingleTest(ruleFolder, test, strat))
        {
            std::cout << test.name << " failed." << std::endl;

            ++numberOfFailedTests;
        }
    }

    std::cout << (cases.size() - numberOfFailedTests) << "/" << cases.size() << " passed." << std::endl;

    SimpleGraph testGraph(5), testGraphTransposed(5);
    testGraph.addEdge(0, 2); testGraphTransposed.addEdge(2, 0);
    testGraph.addEdge(2, 1); testGraphTransposed.addEdge(1, 2);
    testGraph.addEdge(1, 0); testGraphTransposed.addEdge(0, 1);
    testGraph.addEdge(0, 3); testGraphTransposed.addEdge(3, 0);
    testGraph.addEdge(3, 4); testGraphTransposed.addEdge(4, 3);

    std::vector<std::vector<unsigned>> expected = {{2, 0, 1}, {3}, {4}};

    RelianceGroupResult groupResult = computeRelianceGroups(testGraph, testGraphTransposed);
    std::cout << "Reliance group test passed: " << ((groupResult.groups == expected) ? "yes" : "no") << std::endl;
}