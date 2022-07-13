mkdir -p Results

mkdir -p Results/optimizations_all
for file in ./RuleSets/*
do
    ./VLog/build/vlog rel --rule $file --piece 0 --strat 15 --time 60000 > "Results/optimizations_all/$(basename $file).result"
done

mkdir -p Results/optimizations_global
for file in ./RuleSets/*
do
    ./VLog/build/vlog rel --rule $file --piece 0 --strat 6 --time 60000 > "Results/optimizations_global/$(basename $file).result"
done

mkdir -p Results/optimizations_local
for file in ./RuleSets/*
do
    ./VLog/build/vlog rel --rule $file --piece 0 --strat 9 --time 60000 > "Results/optimizations_local/$(basename $file).result"
done

mkdir -p Results/optimizations_none
for file in ./RuleSets/*
do
    ./VLog/build/vlog rel --rule $file --piece 0 --strat 0 --time 60000 > "Results/optimizations_none/$(basename $file).result"
done
