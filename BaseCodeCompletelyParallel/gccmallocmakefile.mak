all: PatternFinder


PatternFinder: main.o Forest.o TreeRAM.o TreeHD.o TreeRAMExperiment.o Logger.o PListArchive.o FileReader.o
	g++ main.o Forest.o TreeRAM.o TreeHD.o TreeRAMExperiment.o Logger.o PListArchive.o FileReader.o -o PatternFinder -pthread -O3 -s -DNDEBUG
main.o: main.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG main.cpp -pthread
Forest.o: Forest.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG Forest.cpp -pthread
TreeRAM.o: TreeRAM.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG TreeRAM.cpp -pthread
TreeRAMExperiment.o: TreeRAM.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG TreeRAMExperiment.cpp -pthread
TreeHD.o: TreeHD.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG TreeHD.cpp -pthread
Logger.o: Logger.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG Logger.cpp -pthread
PListArchive.o: PListArchive.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG PListArchive.cpp -pthread
FileReader.o: FileReader.cpp
	g++ -c -std=c++11 -O3 -s -DNDEBUG FileReader.cpp -pthread
clean:
	rm *o
#Clean command: make -f gccmallocmakefile.mak clean
#Make Command: make -f gccmallocmakefile.mak
#Debug: g++ -std=c++11 main.cpp TreeNode.cpp -pthread -o Debug/PatternFinder
#Release: g++ -std=c++11 -O3 -s -DNDEBUG main.cpp TreeNode.cpp -pthread -o Debug/PatternFinder
#./PatternFinder /f LifeAquatic.mp4 /min 1 /max 10000 /d /threads 9 /mem 12000 /lev 1 /his 0 /RAM