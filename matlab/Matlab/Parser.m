function patternMapping = Parser(filetext, patternCap)
    
    patternMapping = containers.Map;
    incrementor = 0;
    patternCollector = {};
    patternCollectorIndex = 0;
    totalCounter = 0;
    patternInfo = '';
    patternInfoIndex = 1;
    patternCount = 0;
    extraWait = 0;
    
    for index = filetext

        if extraWait == 0

            if totalCounter == incrementor
                totalCounter = 0;
                patternInfoIndex = 1;
                if incrementor <= patternCap
                    tf = isKey(patternMapping,patternInfo);
                    if tf == 0
                        patternMapping(patternInfo) = 1;
                    else
                        patternMapping(patternInfo) = patternMapping(patternInfo) + 1;
                    end
                end
                patternCollector{end+1} = patternInfo;
                patternCollectorIndex = patternCollectorIndex + 1;
                patternInfo='';

                if index == '-'
                    patternCount = 0;
                    incrementor = 0;
                else
                    patternCount = patternCount + 1;
                    incrementor = incrementor + 1;
                    stringTemp = int2str(patternCount);
                    extraWait = length(stringTemp);
                    extraWait = extraWait - 1;
                end


            else
                %this way of concatenating includes blank space and special
                %chars
                patternInfo(end + 1) = index;
                patternInfoIndex = patternInfoIndex + 1;
                totalCounter = totalCounter + 1;
            end
        else
            extraWait = extraWait - 1;
        end
    end
end