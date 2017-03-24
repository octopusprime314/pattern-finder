clear
close all

dramFile = csvread('ThreadsVsSpeed6_54_441381013564.csv');
harddiskFile = csvread('ThreadsVsSpeed6_54_441381013564.csv');

figure 
prevVal = 0;
count = 0;

%dram performance
for idx = 1:numel(dramFile(1:end, 1))+1
    element = dramFile(idx);
    if(prevVal > element)
        plot(dramFile(idx-count:idx-1, 1), dramFile(idx-count:idx-1, 2));
        count = 0;
        hold on
    end
    prevVal = dramFile(idx);
    count = count + 1;
end

xlabel('Threads');
ylabel('Speed');

hold on

%trend line quadratic
%my_poly=polyfit(dramFile(1:end, 1),dramFile(1:end, 2),2); % 2 means quad
%x= 1:0.1:max(dramFile(1:end, 1)); % X data range 
%y=polyval(my_poly,x);
%plot(x,y, '*');

%trend line linear
%my_linear=polyfit(dramFile(1:end, 1),dramFile(1:end, 2),1); % 1 mean linear
%x= 1:0.1:max(dramFile(1:end, 1)); % X data range 
%y=polyval(my_linear,x);
%plot(x,y, 'o');


%hard disk performance 
for idx = 1:numel(harddiskFile(1:end, 1))+1
    element = harddiskFile(idx);
    if(prevVal > element)
        plot(harddiskFile(idx-count:idx-1, 1), harddiskFile(idx-count:idx-1, 2));
        count = 0;
        hold on
    end
    prevVal = harddiskFile(idx);
    count = count + 1;
end

xlabel('Threads');
ylabel('Speed');

hold on

%trend line quadratic
% my_poly=polyfit(harddiskFile(1:end, 1),harddiskFile(1:end, 2),2); % 2 means quad
% x= 1:0.1:max(harddiskFile(1:end, 1)); % X data range 
% y=polyval(my_poly,x);
% plot(x,y);

%trend line linear
% my_linear=polyfit(harddiskFile(1:end, 1),harddiskFile(1:end, 2),1); % 1 mean linear
% x= 1:0.1:max(harddiskFile(1:end, 1)); % X data range 
% y=polyval(my_linear,x);
% plot(x,y);
