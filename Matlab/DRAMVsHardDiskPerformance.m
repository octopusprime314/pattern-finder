clear
close all
addpath('..\Runs');
dramFile = csvread('ThreadsVsThroughput8_20_321554242210.csv');


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

plot(dramFile(idx-(count-1):idx-1, 1), dramFile(idx-(count-1):idx-1, 2));
title('Video file scalability trend lines');
xlabel('Threads');
ylabel('Throughput Increase');

% hold on
% harddiskFile = csvread('ThreadsVsSpeed10_25_37534107576.csv');
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
