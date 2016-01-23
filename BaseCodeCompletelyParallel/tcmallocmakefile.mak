all: PatternFinder


PatternFinder: main.o Forest.o TreeRAM.o TreeHD.o Logger.o PListArchive.o FileReader.o StopWatch.o
	g++ main.o Forest.o TreeRAM.o TreeHD.o Logger.o PListArchive.o FileReader.o StopWatch.o -o PatternFinder -pthread -ltcmalloc -O3 -s -DNDEBUG
main.o: main.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG main.cpp -pthread -ltcmalloc
Forest.o: Forest.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG Forest.cpp -pthread -ltcmalloc
TreeRAM.o: TreeRAM.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG TreeRAM.cpp -pthread -ltcmalloc
TreeHD.o: TreeHD.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG TreeHD.cpp -pthread -ltcmalloc
Logger.o: Logger.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG Logger.cpp -pthread -ltcmalloc
PListArchive.o: PListArchive.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG PListArchive.cpp -pthread -ltcmalloc
FileReader.o: FileReader.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG FileReader.cpp -pthread -ltcmalloc
StopWatch.o: StopWatch.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG StopWatch.cpp -pthread -ltcmalloc

clean:
	rm *o
#Make Command: make clean -f tcmallocmakefile.mak
#Debug: g++ -std=c++11 main.cpp TreeNode.cpp -pthread -o Debug/PatternFinder
#Release: g++ -std=c++11 -O3 -s -DNDEBUG main.cpp TreeNode.cpp -pthread -o Debug/PatternFinder
#./PatternFinder /f my.client02.pc1 /min 1 /max 100000000 /d /threads 33 /mem 12000 /lev 1 /his 0 /RAM
