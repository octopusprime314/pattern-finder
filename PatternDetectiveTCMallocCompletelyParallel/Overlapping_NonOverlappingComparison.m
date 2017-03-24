clear
close all

addpath('..\GitHub\PatternDetective\PatternDetectiveTCMallocCompletelyParallel\Runs');
addpath('..\GitHub\PatternDetective\PatternDetectiveTCMallocCompletelyParallel');

overlappingfid = fopen('PatternVsFileCoverage7_16_0917979.csv'); %overlapping coverage
overlappingPatternCoverage = textscan(overlappingfid, '%[^\n]');
overlappingPatternCoverage = overlappingPatternCoverage{1};
[numRows, numColums] = size(overlappingPatternCoverage);
numRows = 2;
for i = 1:numRows
    overlappingPatternCoverage{i} = str2double(strsplit(overlappingPatternCoverage{i},','));
end

numCoverageFiles = numRows / 2;
figure 
index = 1;
for row = 1:numCoverageFiles
    plot(log(overlappingPatternCoverage{index}), overlappingPatternCoverage{index+1}.*100);
    index = index + 2;
    hold on;
end



xlabel('Pattern Size in bytes (log scale)');
ylabel('Percentage of file covered');
title('Overlapping pattern coverage of Text Files')

nonoverlappingfid = fopen('PatternVsFileCoverage10_20_5721420.csv'); %non overlapping coverage
nonoverlappingPatternCoverage = textscan(nonoverlappingfid, '%[^\n]');
nonoverlappingPatternCoverage = nonoverlappingPatternCoverage{1};
[numRows, numColums] = size(nonoverlappingPatternCoverage);
numRows = 2;
for i = 1:numRows
    nonoverlappingPatternCoverage{i} = str2double(strsplit(nonoverlappingPatternCoverage{i},','));
end

numCoverageFiles = numRows / 2;
figure 
index = 1;
for row = 1:numCoverageFiles
    plot(log(nonoverlappingPatternCoverage{index}), nonoverlappingPatternCoverage{index+1}.*100);
    index = index + 2;
    hold on;
end


xlabel('Pattern Size in bytes (log scale)');
ylabel('Percentage of file covered');
title('Non-overlapping pattern coverage of Text Files')


figure 
index = 1;
for row = 1:numCoverageFiles
    [numRows, numColums] = size(overlappingPatternCoverage{index});
    [numRows2, numColums2] = size(nonoverlappingPatternCoverage{index});
    nonoverlappingPatternCoverage{index+1} = [double(nonoverlappingPatternCoverage{index+1}) double(zeros(1, numColums - numColums2))];
    plot(log(overlappingPatternCoverage{index}), (overlappingPatternCoverage{index+1}.*100) - (nonoverlappingPatternCoverage{index+1}.*100));  
    index = index + 2;
    hold on;
end

xlabel('Pattern Size in bytes (log scale)');
ylabel('Percentage of file covered');
title('Missed coverage of Text Files')
