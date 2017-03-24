clear

close all

addpath('Runs');

overlap = csvread('FileNumberVsProcessingTime7_17_0418159.csv');
nonoverlap = csvread('FileNumberVsProcessingTime7_19_3618655.csv');

figure 

stem(overlap(1:end, 1), overlap(1:end, 2)/1000.0, 'r');
xlabel('File Number');
ylabel('Processing Time (Seconds)');
hold on
stem(nonoverlap(1:end, 1), nonoverlap(1:end, 2)/1000.0, 'g');
legend('overlapping patterns', 'non-overlapping patterns');


overlapCoverage = csvread('FileNumberVsProcessingTime7_17_0418159.csv');
nonoverlapCoverage = csvread('FileNumberVsProcessingTime7_19_3618655.csv');

figure 

scatter(overlapCoverage(1:end, 1), overlapCoverage(1:end, 2));
xlabel('File Number');
ylabel('Processing Time (mS)');

hold on

scatter(nonoverlapCoverage(1:end, 1), nonoverlapCoverage(1:end, 2));