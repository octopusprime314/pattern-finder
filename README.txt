PREREQS:

cmake version 2.5 or higher 
c++11 compatible compiler
python 2.7 to run parallel serial jobs
Visual Studio 2012 or 2015 if building in Windows

BUILD INSTRUCTIONS:

!!!!!!!!!!!ALWAYS BUILD IN RELEASE UNLESS DEBUGGING CODE!!!!!!!!!!

Linux:
    create a build folder at root directory
    cd into build
    cmake -D CMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ..
    cmake --build .
	
Windows:
    create a build folder at root directory
    cd into build
    cmake -G "Visual Studio 11 2012 Win64" ..   OR    cmake -G "Visual Studio 14 2015 Win64" ..
    cmake --build . --config Release
	
	
LOCATION OF FILES TO BE PROCESSED:
Place your file to be processed in the Database/Data folder


EXAMPLE USES OF PATTERNFINDER:
1) ./PatternFinder -f Database -threads 4 -ram
Pattern searches all files recursively in directory using DRAM with 4 threads

2) ./PatternFinder -f TaleOfTwoCities.txt -c -ram
Finds the most optimal thread usage for processing a file

3) ./PatternFinder -f TaleOfTwoCities.txt -plevel 3 -ptop 10
Processes file using memory prediction per level for HD or DRAM processing and displays detailed information for level 3 patterns for the top 10 patterns found

4) ./PatternFinder -f TaleOfTwoCities.txt -plevel 3 -ptop 10 -pnoname
Processes file using memory prediction per level for HD or DRAM processing and displays detailed information for level 3 patterns for the top 10 patterns found but doesn't print pattern string

5) ./PatternFinder -f TaleOfTwoCities.txt -v 1 -mem 1000
Processes file using memory prediction per level for HD or DRAM processing with a memory constraint of 1 GB and -v displays logging program information


PYTHON RUN EXAMPLES:
1) python splitFileForProcessing.py [file path] [number of chunks]
Use splitFileForProcessing.py Python script to split files into chunks and run multiple instances of PatternFinder on those chunks
Ex. python splitFileForProcessing.py ~/Github/PatternDetective/Database/Data/TaleOfTwoCities.txt 4
equally splits up TaleOfTwoCities.txt into 4 files and 4 instances of PatternFinder get dispatched each processing one of the split up files.

2) python segmentRootProcessing.py [file path] [number of jobs] [threads per job]
Use segmentRootProcessing.py Python script splits up PatternFinder jobs to search for patterns starting with a certain value
Ex. python segmentedRootProcessing.py ../Database/Data/Boosh.avi 4 4
Dispatches 4 processes equipped with 4 threads each.  Each PatternFinder will only look for patterns starting with the byte
representation of 0-63, 64-127, 128-191, 192-255.
