clear
close all

addpath('..\GitHub\PatternDetective\PatternDetectiveTCMallocCompletelyParallel\Runs');
addpath('..\GitHub\PatternDetective\PatternDetectiveTCMallocCompletelyParallel');
fid = fopen('PatternVsFileCoverage9_09_4720242.csv'); %text sub sample
%fid = fopen('PatternVsFileCoverage8_51_0416575.csv'); %image sub sample
%fid = fopen('PatternVsFileCoverage9_04_2819200.csv'); %show sub sample


patternCoverage = textscan(fid, '%[^\n]');
patternCoverage = patternCoverage{1};
[numRows, numColums] = size(patternCoverage);

for i = 1:numRows
    patternCoverage{i} = str2double(strsplit(patternCoverage{i},','));
end

numCoverageFiles = numRows / 2;
figure 
index = 1;
for row = 1:numCoverageFiles
    plot(patternCoverage{index}, patternCoverage{index+1}.*100);
    index = index + 2;
    hold on;
end

xlabel('Pattern Size');
ylabel('Percentage of file covered');
title('Non-overlapping pattern coverage of Text Files')


%trend line quadratic
%my_poly=polyfit(patternCoverage(1, 1:end),patternCoverage(2, 1:end),2); % 2 means quad
%x= 1:0.1:max(patternCoverage(1:end, 1)); % X data range 
%y=polyval(my_poly,x);
%plot(x,y, '*');

%trend line linear
%my_linear=polyfit(dramFile(1:end, 1),dramFile(1:end, 2),1); % 1 mean linear
%x= 1:0.1:max(dramFile(1:end, 1)); % X data range 
%y=polyval(my_linear,x);
%plot(x,y, 'o');