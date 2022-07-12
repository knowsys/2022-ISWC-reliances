# Supplementary Material

This archive contains all supplementary for the publication
"Efficient Dependency Analysis for Existential Rules". This
includes a version of the paper with an appendix that gives
proofs and further details; detailed measurement data from
our own evaluations; and source code and data files for 
reproducing our experiments.

Below are further instructions on how to run the experiments.

# Installation

## VLog

Detailed installation instructions and prerequisites for VLog
are the same as given on the tool's [website](https://github.com/karmaresearch/vlog).

On a system with a suitably configured C++ development environment,
the installation can be done by running the script ```sh install-vlog.sh```
or the commands that are contained therein.

##  Graal

You can follow the instructions of the maintainers [Website](http://graphik-team.github.io/graal/doc/without-ide),
and add the dependencies from ```Graal/graal.deps``` to ```pom.xml``` (one additional
dependency needed compared to the Website description). Our program took the main method
from the file ```Graal/graal.java```.

## Rule Sets

Extract the file ```RuleSets.zip``` to obtain a directory ```RuleSets``` with
all rule sets used in the evaluation.

# Experiments

Each experiment is associated with a bash script that launches the computation
on all ontologies contained in ```RuleSets```. The result of each computation
is stored in a separate ```.result``` file. The relevant statistics are stored
in the following format: ```<Stat>: <Value>```. To easily aggregate the results,
we provide a python script called with ```python3 combine.py <Stat> <Results-Folder>```.
It takes the name of the statistic and a path to the folder containing the
```.result```-files as input and generates a semi-colon separated list with all
of the values ordered by the ontology-id.

## Optimisations Impact

**Command:** ```sh ./ex_optimisations.sh```

This will produce 4 folders containing the results:

- ```Results/optimizations_all```: All optimizations enabled
- ```Results/optimizations_global```: Only global optimizations are enabled
- ```Results/optimizations_local```: Only local optimizations are enabled
- ```Results/optimizations_none```: No optimizations

The ```.result```-files will contain the entry ```Time-Positive``` with the time
in ms for rule sets that finished within the 60s timeout. Similarly with the 
entry ```Time-Restraint``` for restraint reliances.

## Acyclic Positive Reliances

### Our implementation

**Command:** ```sh ./ex_grd_ours.sh```

This will produce 1 folder containing the result: ```Results/grd_ours```.

The entry ```Time-Positive``` will contain the time in ms.

### Graal

Build the package and run the program as reported on the maintainer's Website (see above).
Pass the rule file you want to analyze as a command line parameter (by adding
```-Dexec.args="FILENAME"``` to the maven call). Upon termination, there is a CSV string
output starting with the file name. Besides other values, the fourth position in the CSV
string is the GRD construction time in ms.

## Faster MFA

**Command:** ```sh ./ex_mfa.sh```

This will run both the original implementation and our improved version. The following
folders will be produced:
- ```Results/mfa_ours```: Our version, where MFA is run on strongly connected components
- ```Results/mfa_vlog```: Original implentation in VLog

The entry ```Reliance-Time``` contains the time in ms for computing the positive reliances
(not relevant for the ```mfa_vlog```). The entry ```Cycle-Time``` contains the time in ms
of all MFA runs. Furthermore, ```Acyclic``` is set to ```1```
if the rule set is MFA and ```0``` otherwise.

## Core Stratification

**Command:** ```sh ./ex_core-strat.sh```

This will test for core stratification for both the piece-decomposed rule set and the
original one. This results in the following folders:
- ```Results/corestrat-nopiece```: No piece-decomposition applied
- ```Results/corestrat-piece```: Running experiments with piece-decomposition

The entry ```Core-Stratified``` will contain ```1```
if the rule set is core stratified and ```0``` otherwise.
