clear

%put whatever file you want to parse here!
filetext = fileread('CollectivePatternData4_58_0014736.txt');

%the maximum pattern length
patternCap = 3;

%get the map of all the collected patterns
patternMapping = Parser(filetext, patternCap)

%pull out the keys and values
keyVals = keys(patternMapping);
patternCounts = values(patternMapping);

%convert cells to matrix
keyValsMat = char(keyVals);
patternCountsMat = cell2mat(patternCounts);

%get the index map of the most common pattern
[Y,I] = max(patternCountsMat);

%print most common pattern 
keyValsMat(I)



