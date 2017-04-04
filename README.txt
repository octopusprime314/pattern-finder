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


Run instructions:

kick off test scripts located within the scripts folder for program validation
