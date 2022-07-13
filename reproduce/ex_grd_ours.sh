mkdir -p Results
mkdir -p Results/grd_ours
for file in ./RuleSets/*
do
	./VLog/build/vlog rel --grd 1 --rule $file --time 0 > "Results/grd_ours/$(basename $file).result"
done
