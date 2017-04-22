import os
import sys
import subprocess



fileName = sys.argv[1]
jobs = int(sys.argv[2])
file = open(fileName, "rb")
fileData = file.read()
file.close()

prevdir = os.getcwd()
if os.name == 'nt':
        BIN_DIR = "../bin/RelWithDebInfo"
	os.chdir("../..")
else:
        BIN_DIR = "../bin"
	os.chdir("..")

rootdir = os.getcwd()
os.chdir(prevdir)

os.chdir(BIN_DIR)

leftrange = 0
division =  256/jobs
counter = 0
while counter < jobs:
	cmdargs = ['./PatternFinder', '-f', '../' + fileName , '-threads', '1', '-v', '1', '-lr', str(leftrange), '-hr', str(leftrange + division - 1), '-ram']
	print cmdargs
	proc = subprocess.Popen(cmdargs)
	leftrange = leftrange + division
	counter = counter + 1