import os
import textwrap
import sys
import subprocess

BIN_DIR = "../bin/Release"
os.chdir(BIN_DIR);

fileName = sys.argv[1]
jobs = int(sys.argv[2])
file = open(fileName, "rb")
fileData = file.read()
file.close()
fileSize = len(fileData)
divider = fileSize/jobs;
remainder = fileSize % jobs;

fileChunks = [fileData[x:x+divider] for x in range(0, len(fileData), divider)]


if remainder > 0:
	fileChunks[len(fileChunks) - 2] = fileChunks[len(fileChunks) - 2] + fileChunks[len(fileChunks) - 1]
	del fileChunks[-1]
	
folderName = fileName.rsplit('/', 1)[-1]
extension = folderName.rsplit('.', 1)[-1]
folderName = folderName.rsplit('.', 1)[0]

folderPath = "../../Database/Split" + folderName
if not(os.path.exists(folderPath)):
	os.makedirs(folderPath)

counter = 0

for fileChunk in fileChunks:
	fileTag = folderPath + "/" + folderName + str(counter) + "." + extension
	file = open(fileTag, "wb+")
	file.write(fileChunks[counter]);
	file.close()
	cmdargs = ["PatternFinder", " -f " + fileTag + " -threads 2 -v 1 -ram"]
	proc = subprocess.Popen([cmdargs], shell=True,stdin=None, stdout=None, stderr=None, close_fds=True)
	counter = counter + 1



	

	
