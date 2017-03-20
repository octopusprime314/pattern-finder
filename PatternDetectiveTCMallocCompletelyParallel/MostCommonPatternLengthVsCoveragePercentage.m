clear
close all

patternCoverageFile = csvread('PatternVsFileCoverage8_27_4524772.csv');

figure 
plot(patternCoverageFile(1:end, 1), patternCoverageFile(1:end, 2).*100);
xlabel('Pattern Size');
ylabel('Percentage of file covered');
title('Non-overlapping pattern coverage of the file')

hold on

