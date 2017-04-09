Prereqs:

cmake version 2.5 or higher 
c++11 compatible compiler

Build instructions:

Linux:
    create a build folder at root directory
    cd into build
    cmake -D CMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ..
    cmake --build .
	
Windows:
    create a build folder at root directory
    cd into build
    cmake -G "Visual Studio 11 2012 Win64" ..   OR    cmake -G "visual Studio 14 2015 Win64" ..
    cmake --build . --config Release
	
	
Location of files to be processed:
Place your file to be processed in the Database/Data folder


Command line instructions:
./PatternFinder.exe -f TaleOfTwoCities.txt -v 1 -threads 4 -ram
Runs a PatternFinder program using 4 threads to process TaleOfTwoCities.txt forcing to use only DRAM processing and display the output using -v
	

Python run instructions:
Use splitFileForProcessing.py Python script to split files into chunks and run multiple instances of PatternFinder on those chunks.
Run command: python splitFileForProcessing.py [direct file path] [number of chunks]
For example python splitFileForProcessing.py ~/Github/PatternDetective/Database/Data/TaleOfTwoCities.txt 4
equally splits up TaleOfTwoCities.txt into 4 files and 4 instances of PatternFinder get dispatched each processing one of the split up files.
