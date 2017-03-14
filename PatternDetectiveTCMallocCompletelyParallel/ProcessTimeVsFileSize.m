data = csvread('Runs/TimeVsFileSize5_06_029074.csv');
data2 = csvread('Runs/FinalPatternVsCount5_06_029074.csv');

figure 

scatter(data(1:end, 1), data(1:end, 2));

xlabel('Processing Time (milliSeconds)');
ylabel('File size (Bytes)');

figure 

plot(data2(1:end, 1), data2(1:end, 2));

xlabel('Pattern Size');
ylabel('File Count');