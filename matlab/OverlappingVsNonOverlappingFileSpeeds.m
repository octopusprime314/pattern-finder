clear

close all

addpath('Runs');

overlap = csvread('FileNumberVsProcessingTime6_10_56822158042.csv');
nonoverlap = csvread('FileNumberVsProcessingTime6_13_09107304526.csv');

overlap2 = csvread('FileNumberVsProcessingTime9_53_59165262795.csv');
nonoverlap2 = csvread('FileNumberVsProcessingTime6_23_111707642049.csv');

figure 

joined = [nonoverlap(2), overlap(2) , nonoverlap2(2), overlap2(2)];
joined = joined./1000.0

%dram performance with memory limits
bar(joined);
Labels = {'nonoverlap','overlap', 'nonoverlap','overlap'};
set(gca, 'XTick', 1:length(Labels), 'XTickLabel', Labels);
title('Large Pattern File NonOverlapping Search Speedup');
xlabel('Overlapping or Non Overlapping Selection')
ylabel('Processing Time (Seconds)')

for i1=1:numel(joined)
    text(i1,joined(i1),num2str(joined(i1),'%0.2f'),...
               'HorizontalAlignment','center',...
               'VerticalAlignment','bottom')
end