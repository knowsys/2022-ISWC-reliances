import sys
import os

if len(sys.argv) < 3:
  print(f"python cobine.py <Stat> <Result-Folder>")
  exit(1)

key = sys.argv[1]
resultsFolder = sys.argv[2]
rulesFolder = "RuleSets"

resultDict = dict()

for entry in os.scandir(rulesFolder):   
  if not entry.is_file():
    continue

  containsKey = False
  fileBasename = os.path.basename(entry) + ".result"

  with open(os.path.join(resultsFolder, fileBasename), 'r') as fileHandle:
    lines = fileHandle.readlines()

    for line in lines:
      lineSplit = line.strip().split()
      
      if (len(lineSplit) < 2):
        continue

      if (lineSplit[0] == key + ':'):
        resultDict[fileBasename] = lineSplit[1]
        containsKey = True

  if not containsKey:
    resultDict[fileBasename] = "none"

resultString = ""
for file, value in sorted(resultDict.items()):
  resultString += value + '; '

print(resultString)