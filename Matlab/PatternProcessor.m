clear

close all

addpath('..\Runs');
addpath('..');


fcpdata = fopen('..\Runs\CPData.csv','w+');

begIndex = 25;

f = fopen('CollectivePatternData1812.csv','r');
block = fread(f);
fwrite(fcpdata,block);
f = fopen('CollectivePatternData5552.csv','r');
block = fread(f);
block = block(begIndex:end);
fwrite(fcpdata,block);
f = fopen('CollectivePatternData6376.csv','r');
block = fread(f);
block = block(begIndex:end);
fwrite(fcpdata,block);
f = fopen('CollectivePatternData6472.csv','r');
block = fread(f);
block = block(begIndex:end);
fwrite(fcpdata,block);
f = fopen('CollectivePatternData13004.csv','r');
block = fread(f);
block = block(begIndex:end);
fwrite(fcpdata,block);
f = fopen('CollectivePatternData13260.csv','r');
block = fread(f);
block = block(begIndex:end);
fwrite(fcpdata,block);
f = fopen('CollectivePatternData14520.csv','r');
block = fread(f);
block = block(begIndex:end);
fwrite(fcpdata,block);
f = fopen('CollectivePatternData21360.csv','r');
block = fread(f);
block = block(begIndex:end);
fwrite(fcpdata,block);
fclose(fcpdata);


ds = datastore('..\Runs\CPData.csv','TreatAsMissing','NA');
ds.SelectedVariableNames = {'Count', 'Length', 'Pattern'};
Data = read(ds);

patterns = (table2array(Data(:,'Pattern')));
counts = table2array(Data(:, 'Count'));
mappy = containers.Map();

for x = 1:length(patterns)
    val = cell2mat(patterns(x));
    if isKey(mappy,val)
        mappy(val) = mappy(val) + counts(x); 
    else
        mappy(val) = counts(x); 
    end
end
uniquemap = zeros(length(mappy), 1);
k = keys(mappy);
val = values(mappy);
largest = 0;
mapIndexes = zeros(length(mappy), 1);

for i = 1:length(mappy)
    stringVal = length(k{i});
    if(stringVal > largest)
        largest = stringVal;
    end
    if val{i} > uniquemap(stringVal)
        uniquemap(stringVal) = val{i}; 
        mapIndexes(length(k{i})) = i;
    end
end

uniquemap = uniquemap(1:largest);
mapIndexes = mapIndexes(1:largest);

figure

plot(log(1:1:length(uniquemap)), log(uniquemap), '-');
xlabel('Log[ Pattern Length ]');
ylabel('Log[ Pattern Frequency ]');

ds = datastore('..\Runs\CollectivePatternData8208.csv','TreatAsMissing','NA');
ds.SelectedVariableNames = {'Count', 'Length', 'Pattern'};
Data = read(ds);
counts = table2array(Data(:, 'Count'));
patterns = table2array(Data(:, 'Pattern'));
%hold on 
figure

plot(log(1:1:length(counts)), log(counts), '-');
legend('Split processing pattern data', 'Non-split processing');
title('Split and Non-split processing coverage');

%pattern matching
figure

matching = zeros(length(patterns), 1);
matchcount = 0.0;
for i = 1:length(patterns)
    if length(k{mapIndexes(i)}) == length(patterns{i})
        if(char(patterns{i}) == char(k{mapIndexes(i)}))
            matching(i) = 1;
            matchcount = matchcount + 1.0;
        end
    end
end

percentage = matchcount / length(patterns)


plot(log(1:1:length(counts)), log(counts), '-');
legend('Split processing pattern data', 'Non-split processing');
title('Split and Non-split processing coverage');





