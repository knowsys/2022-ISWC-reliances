mkdir -p Results
mkdir -p Results/mfa_ours
for file in ./RuleSets/*
do
	timeout 40m ./VLog/build/vlog rel --cycles 1 --alg MFA --splitPositive 1 --rule $file --time 0 > "Results/mfa_ours/$(basename $file).result"
done

mkdir Results/mfa_vlog
for file in ./RuleSets/*
do
	timeout 40m ./VLog/build/vlog rel --cycles 1 --alg MFA --splitPositive 0 --rule $file --time 0 > "Results/mfa_vlog/$(basename $file).result"
done
