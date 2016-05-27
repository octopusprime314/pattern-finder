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
#Clean command: make -f gccmallocmakefile.mak clean
#Make Command: make clean -f tcmallocmakefile.mak
#./PatternFinder -f TaleOfTwoCities.txt -min 1 -max 1000000 -d -threads 5 -mem 40 -lev 1 -his 0 -v 1 -HD
