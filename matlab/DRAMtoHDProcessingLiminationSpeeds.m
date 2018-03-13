clear
close all
addpath('Runs');
dramnolimit = csvread('FileNumberVsProcessingTime6_34_488819.csv');
dram1000MB = csvread('FileNumberVsProcessingTime6_35_268943.csv');
dram500MB = csvread('FileNumberVsProcessingTime6_36_129093.csv');
dram250MB = csvread('FileNumberVsProcessingTime6_44_1010654.csv');
dram100MB = csvread('FileNumberVsProcessingTime6_54_5822771.csv');

joined = [dramnolimit(2); dram1000MB(2); dram500MB(2); dram250MB(2); dram100MB(2)];
joined = joined./1000.0

figure 

%dram performance with memory limits
bar(joined);
Labels = {'nolimit','1GB', '500MB', '250MB', '100MB'};
set(gca, 'XTick', 1:length(Labels), 'XTickLabel', Labels);
title('Limited Memory Processing of a 250 MB file');
xlabel('Memory Limitations')
ylabel('Processing Time (Seconds)')

for i1=1:numel(joined)
    text(i1,joined(i1),num2str(joined(i1),'%0.2f'),...
               'HorizontalAlignment','center',...
               'VerticalAlignment','bottom')
end

