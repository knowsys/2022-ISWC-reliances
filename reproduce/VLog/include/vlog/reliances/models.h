#ifndef _MODELS_H
#define _MODELS_H

#include "vlog/reliances/reliances.h"

template<typename T>
bool relianceModels(const std::vector<T> &left, RelianceRuleRelation leftRelation,
    const std::vector<Literal> &right, RelianceRuleRelation rightRelation,
    const VariableAssignments &assignments,
    std::vector<unsigned> &satisfied, std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &existentialMappings,
    bool alwaysDefaultAssignExistentials = false, 
    bool treatExistentialAsVariables = true
)
{
    bool isCompletelySatisfied = true; //refers to non-existential atoms
    
    size_t existentialMappingIndex = 0;

    for (unsigned rightIndex = 0; rightIndex < right.size(); ++rightIndex)
    {   
        if (satisfied[rightIndex] == 1)
            continue;

        const Literal &rightLiteral = right[rightIndex];
        unsigned tupleSize = rightLiteral.getTupleSize(); //Should be the same as leftLiteral.getTupleSize()

        bool rightSatisfied = false;
        
        bool rightExistential = false;
        std::vector<std::unordered_map<int64_t, TermInfo>> *currentMappingVector = nullptr; 

        for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
        {
            VTerm rightTerm = rightLiteral.getTermAtPos(termIndex);
            TermInfo rightInfo = getTermInfoModels(rightTerm, assignments, rightRelation, false); 

            if (treatExistentialAsVariables && rightInfo.type == TermInfo::Types::Existential)
            {
                rightExistential = true;
                currentMappingVector = &existentialMappings[existentialMappingIndex];

                break;
            }
        }

        for (const Literal &leftLiteral : left)
        {
            if (rightLiteral.getPredicate().getId() != leftLiteral.getPredicate().getId())
                continue;

            bool leftModelsRight = true;

            std::unordered_map<int64_t, TermInfo> *currentMapping = nullptr;

            for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
            {
                VTerm leftTerm = leftLiteral.getTermAtPos(termIndex);
                VTerm rightTerm = rightLiteral.getTermAtPos(termIndex);

                TermInfo leftInfo = getTermInfoModels(leftTerm, assignments, leftRelation, alwaysDefaultAssignExistentials);
                TermInfo rightInfo = getTermInfoModels(rightTerm, assignments, rightRelation, false); //TODO: Rethink order of for loops in order to save this computation

                if (treatExistentialAsVariables && rightInfo.type == TermInfo::Types::Existential)
                {
                    if (currentMapping == nullptr)
                    {
                        currentMappingVector->emplace_back();
                        currentMapping = &currentMappingVector->back();
                    }

                    auto mapIterator = currentMapping->find(rightInfo.termId);
                    if (mapIterator == currentMapping->end())
                    {
                        (*currentMapping)[rightInfo.termId] = leftInfo;
                    }
                    else
                    {
                        if (termsEqual(mapIterator->second, leftInfo))
                        {
                            continue;
                        }
                        else
                        {
                            leftModelsRight = false;
                            break;
                        }
                    }
                }
                else
                {
                    if (termsEqual(leftInfo, rightInfo))
                    {
                        continue;
                    }
                    else
                    {
                        leftModelsRight = false;
                        break;
                    }
                }
            }

            if (leftModelsRight && !rightExistential)
            {
                rightSatisfied = true;
                break;
            }

            if (rightExistential && !leftModelsRight && currentMapping != nullptr)
            {
                existentialMappings[existentialMappingIndex].pop_back();
            }
        }

        if (rightExistential)
        {
            isCompletelySatisfied = false;
            ++existentialMappingIndex;
        }
        else
        {
            if (rightSatisfied)
                satisfied[rightIndex] = 1;
            else
                isCompletelySatisfied = false;
        }
      
    }

    return isCompletelySatisfied;
}

#endif