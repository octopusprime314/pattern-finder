clear

close all

addpath('..\GitHub\PatternDetective\PatternDetectiveTCMallocCompletelyParallel\Runs');
addpath('..\GitHub\PatternDetective\PatternDetectiveTCMallocCompletelyParallel');

timevsfilesize = csvread('TimeVsFileSize10_30_0028723.csv');
finalpatternvscount = csvread('FinalPatternVsCount10_30_0128726.csv');

figure 

scatter(timevsfilesize(1:end, 1), timevsfilesize(1:end, 2));
xlabel('Processing Time (milliSeconds)');
ylabel('File size (Bytes)');

hold on

%quadratic fit trend line
my_poly=polyfit(timevsfilesize(1:end, 1),timevsfilesize(1:end, 2),2);
X2= 1:0.1:max(timevsfilesize(1:end, 1)); % X data range 
Y2=polyval(my_poly,X2);
plot(X2,Y2);

%linear fit trend line
my_poly=polyfit(timevsfilesize(1:end, 1),timevsfilesize(1:end, 2),1);
X2= 1:0.1:max(timevsfilesize(1:end, 1)); % X data range 
Y2=polyval(my_poly,X2);
plot(X2,Y2);


figure 

stem(finalpatternvscount(1:end, 1), finalpatternvscount(1:end, 2));

axis([0 100 0 inf])
xlabel('Pattern Size');
ylabel('File Count');
