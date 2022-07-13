mkdir -p Results
mkdir -p Results/corestrat-nopiece
for file in ./RuleSets/*
do
	./VLog/build/vlog rel --rule $file --piece 0 --strat 15 --time 0 > "Results/corestrat-nopiece/$(basename $file).result"
done

mkdir -p Results/corestrat-piece
for file in ./RuleSets/*
do
	./VLog/build/vlog rel --rule $file --piece 1 --strat 15 --time 0 > "Results/corestrat-piece/$(basename $file).result"
done
