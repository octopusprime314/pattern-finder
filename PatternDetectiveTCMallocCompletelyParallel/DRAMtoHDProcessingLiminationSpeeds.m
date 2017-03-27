clear
close all
addpath('..\GitHub\PatternDetective\PatternDetectiveTCMallocCompletelyParallel\Runs');
dram = csvread('FileNumberVsProcessingTime3_29_588136.csv');
harddisk250MB = csvread('FileNumberVsProcessingTime4_06_2515278.csv');

joined = [dram; harddisk250MB];

figure 
prevVal = 0;
count = 0;

%dram performance
bar(joined);

hold on

% prevVal = 0;
% count = 0;
% 
% %hard disk performance 
% for idx = 1:numel(harddiskFile(1:end, 1))+1
%     element = harddiskFile(idx);
%     if(prevVal > element)
%         plot(harddiskFile(idx-count:idx-1, 1), harddiskFile(idx-count:idx-1, 2));
%         count = 0;
%         hold on
%     end
%     prevVal = harddiskFile(idx);
%     count = count + 1;
% end
% 
% %plot(harddiskFile(idx-(count-1):idx-1, 1), harddiskFile(idx-(count-1):idx-1, 2));
% slowdown = harddiskFile(idx-(count-1):idx-1, 2)./dramFile(idx-(count-1):idx-1, 2);
% plot(dramFile(idx-(count-1):idx-1, 1), slowdown, '-');
% 
% title('HD processing slow down of an mp4 file');
% xlabel('threads');
% ylabel('HD/DRAM processing slowdown');
% 
% %trend line quadratic
% my_poly=polyfit(dramFile(idx-(count-1):idx-1, 1),slowdown, 1); % 2 means quad
% a = my_poly(1)
% b = my_poly(2)
% polyfit_str = ['y = ' num2str(a) ' *x + ' num2str(b)]
% % polyfit_str qill be : y = 4*x + 2
% legend(polyfit_str);
% x= 1:0.1:max(dramFile(idx-(count-1):idx-1, 1)); % X data range 
% y=polyval(my_poly,x);
% plot(x,y);
% 
% %trend line linear
% %my_poly=polyfit(dramFile(idx-(count-1):idx-1, 1),slowdown, 2); % 2 means quad
% %x= 1:0.1:max(dramFile(idx-(count-1):idx-1, 1)); % X data range 
% %y=polyval(my_poly,x);
% %plot(x,y);
