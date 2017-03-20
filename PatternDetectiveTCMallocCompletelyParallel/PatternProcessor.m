clear

close all

%put whatever file you want to parse here!
filetext = fileread('CollectivePatternData8_59_3811017.txt');

%the maximum pattern length
patternCap = 100;

%get the map of all the collected patterns
patternMapping = Parser(filetext, patternCap);

%pull out the keys and values
keyVals = keys(patternMapping);

%find out the actual maximum pattern from processing
patternCap = max(cellfun('length', keyVals));

%pull out the pattern counts
patternCounts = values(patternMapping);

%convert cells to matrix
patternCountsMat = cell2mat(patternCounts);

index = 0;
patternData = {};
mostCommonValue = {};
mostCommonIndex = {};
mostCommonPattern = {};
overlappingPatterns = {};
patternCollectionList = {};
for j = 1:patternCap
    maxVal = 0;
    mostCommonValue{end+1} = 0;
    mostCommonIndex{end+1} = 0;
    mostCommonPattern{end+1} = 0;
    for i = 1:length(keyVals)
        pattern=keyVals{i};
        if(length(pattern) == j)
            
            if(mostCommonValue{j} < patternCounts{i})
                patternData{end+1} = pattern;
                index = index + 1;
                mostCommonValue{j} = patternCounts{i};
                mostCommonIndex{j} = i;
                mostCommonPattern{j} = pattern;
            end
        end
    end
end

coverage = {};

for j = 1:length(mostCommonIndex)
    keyVals{mostCommonIndex{j}};
end

for j = 1:length(mostCommonIndex)
    coverage{end + 1} = mostCommonValue{j} * j;
end

figure

plot(1:1:patternCap, cell2mat(mostCommonValue), '-');
xlabel('Pattern Length');
ylabel('Pattern Frequency');

hold on 

plot(1:1:patternCap, cell2mat(coverage), '-');

