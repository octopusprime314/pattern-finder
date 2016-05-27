all: PatternFinder


PatternFinder: main.o Forest.o TreeRAM.o TreeHD.o Logger.o PListArchive.o FileReader.o StopWatch.o
	g++ main.o Forest.o TreeRAM.o TreeHD.o Logger.o PListArchive.o FileReader.o StopWatch.o -o PatternFinder -pthread -ltcmalloc
main.o: main.cpp
	g++ -g -c -std=c++11 main.cpp -pthread -ltcmalloc
Forest.o: Forest.cpp
	g++ -g -c -std=c++11 Forest.cpp -pthread -ltcmalloc
TreeRAM.o: TreeRAM.cpp
	g++ -g -c -std=c++11 TreeRAM.cpp -pthread -ltcmalloc
TreeHD.o: TreeHD.cpp
	g++ -g -c -std=c++11 TreeHD.cpp -pthread -ltcmalloc
Logger.o: Logger.cpp
	g++ -g -c -std=c++11 Logger.cpp -pthread -ltcmalloc
PListArchive.o: PListArchive.cpp
	g++ -g -c -std=c++11 PListArchive.cpp -pthread -ltcmalloc
FileReader.o: FileReader.cpp
	g++ -g -c -std=c++11 FileReader.cpp -pthread -ltcmalloc
StopWatch.o: StopWatch.cpp
	g++ -g -c -std=c++11 StopWatch.cpp -pthread -ltcmalloc
clean:
	rm *o
#Clean command: make -f gccmallocmakefile.mak clean
#Make Command: make -f gccmallocmakefile.mak
#./PatternFinder -f TaleOfTwoCities.txt -min 1 -max 1000000 -d -threads 5 -mem 40 -lev 1 -his 0 -v 1 -HD
