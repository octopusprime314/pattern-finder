all: PatternFinder


PatternFinder: main.o Forest.o TreeRAM.o TreeHD.o Logger.o PListArchive.o FileReader.o StopWatch.o ChunkFactory.o ProcessorConfig.o
	g++ main.o Forest.o TreeRAM.o TreeHD.o Logger.o PListArchive.o FileReader.o StopWatch.o ChunkFactory.o ProcessorConfig.o -o PatternFinder -pthread -O3 -s -DNDEBUG
main.o: main.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG main.cpp -pthread
Forest.o: Forest.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG Forest.cpp -pthread
TreeRAM.o: TreeRAM.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG TreeRAM.cpp -pthread
TreeHD.o: TreeHD.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG TreeHD.cpp -pthread
Logger.o: Logger.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG Logger.cpp -pthread
PListArchive.o: PListArchive.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG PListArchive.cpp -pthread
FileReader.o: FileReader.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG FileReader.cpp -pthread
StopWatch.o: StopWatch.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG StopWatch.cpp -pthread
ChunkFactory.o: ChunkFactory.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG ChunkFactory.cpp -pthread
ProcessorConfig.o: ProcessorConfig.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG ProcessorConfig.cpp -pthread
clean:
	rm *o
#Clean command: make -f gccmallocmakefile.mak clean
#Make Command: make -f gccmallocmakefile.mak
#./PatternFinder -f TaleOfTwoCities.txt -min 1 -max 1000000 -d -threads 5 -mem 40 -lev 1 -his 0 -v 1 -HD
