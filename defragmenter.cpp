// Disk defragmenter
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <fstream>
namespace fs = std::filesystem;
using std::string;

string parentDir;
string cacheLoc = "/tmp/disk_analize_<>.txt";

int fl_fileOutSize = 40;

struct FileData {
	string name;
	long start, end;
};
struct FileSpace {
	long start, end;
	long size;
};
FileData* files;
FileSpace* spaces;
int* sortedFiles;
long sectorCount;
int entries;

bool procArgs(int argS, const char* args[]);	// Process every arg passed, true = exit
bool readIndex();								// Reads the file with the index, returns if succeded
void index(string path);						// Indexes every file in the given path, and attempts saving it
void analize();									// Analizes where does the file starts and ends with hdparm --fibmap
void sortFiles();								// Generates a sorted index of the data
void getDiskData();								// Gets data about the disk, sectorCount, etc...
void identifySpace();							// With the sorted data, it can identify where unused sectors in the disk
void defragment(bool simple);					// TODO: Defragments the data, ONLY suggestions on what to do.

string exec(string command);
string replace(string& str, const char* from, const char* to);
int* sort(long* data, int*& sortedFiles, int size, const char* cache); 

int main(int argS, const char* args[]){
	if (procArgs(argS, args)) { return 0; }
	fl_fileOutSize = std::stoi(exec("tput cols"));

	printf("Indexing...\r");
	if(!readIndex()) { index(parentDir); }

	printf("Analizing...\r");
	analize();
	printf("Analisis Completed! 100%%\e[K\n");

	printf("Sorting... \r");
	sortFiles();
	printf("Finished sorting! %i values\n", entries);

	getDiskData();
	identifySpace();

	defragment(true);
	
	free(sortedFiles); free(files); free(spaces);
	return 0;
}

bool procArgs(int argS, const char* args[]){
	// True == exit
	if (argS <= 1) { return true; }
	string refresh("-r");
	string version("-v");
	string output("-o");
	string help("--help");

	for(int i=1; i < argS; i++){
		if(refresh == args[i]){
			fs::remove(replace(cacheLoc, "<>", "index"));
			fs::remove(replace(cacheLoc, "<>", "data"));
			fs::remove(replace(cacheLoc, "<>", "sort"));
		} else
		if(version == args[i]){
			printf(" | mini-defragmenter.cpp - Version 0.8.5\n");
			return true;
		} else
		if(help == args[i]){
			printf("No."); // TODO: Be polite
			return true;
		} else
		if(output == args[i]){
			if((i+1) == argS){ return true; }
			cacheLoc = string(args[i+1]);
			if(fs::is_directory(cacheLoc)){
				cacheLoc += "/disk_analize_<>.txt";
			} else if(cacheLoc.find("<>") == -1) { return true; }
		}
	}
	parentDir = args[argS - 1];
	return false;
}

bool readIndex(){
	if(!fs::exists(replace(cacheLoc, "<>", "index"))){return false;}

	int i=0;
	std::ifstream file(replace(cacheLoc, "<>", "index"));
	std::vector<string> filesStr;
	string line;
    while (std::getline(file, line)) {
		filesStr.push_back(line);
		i++;
        printf("Indexing: %i files...\r", i);
    }
	entries = i;

	files = (FileData*) calloc(entries, sizeof(FileData));
	for(i=0; i<entries; i++){
		files[i].name = filesStr[i];
	}
	printf("Indexed: %i files!   \n", entries);

	return true;
}

void index_(string dir, std::vector<string>& files){
	const char* dirName = dir.c_str();
	for (const fs::directory_entry & entry : fs::directory_iterator(dir)) {
		if(entry.is_directory()){
			index_(entry.path(), files);
			continue;
		}
        printf("Indexing: %i files, at: %.*s\r", entries, fl_fileOutSize, dirName);
		files.push_back(entry.path());
		entries++;
	}
}
void index(string dir){
	entries=0;
	std::vector<string> filesStr;
	index_(dir, filesStr);
	std::ofstream out(replace(cacheLoc, "<>", "index"));

	files = (FileData*) calloc(entries, sizeof(FileData));
	for(int i=0; i<entries; i++){
		files[i].name = filesStr[i];
	}

	if(!out.good()) { printf("Indexed: %i files! (Not Saved...)\e[K\n", entries); return; }
    for(int i=0; i<entries; i++){
		out << filesStr[i] << "\n";
	}
    out.close();
	printf("Indexed: %i files! (Saved)\e[K\n", entries);
}

void analize(){
	std::ifstream savedDataI(replace(cacheLoc, "<>", "data"));
	int i=0;
	string line;
    while (std::getline(savedDataI, line)) {
		files[i].start	= std::stol(line.substr(0, line.find(" ")));
		files[i].end	= std::stol(line.substr(line.find(" ")+1));
		i++;
        printf("Reading saved data: %i/%i\r", i, entries);
    }
	if(i!=0) { printf("Read saved data: %i/%i   \n", i, entries); }

	std::ofstream savedDataO(replace(cacheLoc, "<>", "data"));
	for (; i<entries; i++){
		if(!fs::exists(files[i].name)) {
			files[i].start = 0; files[i].end = 0;
			savedDataO << "0 0\n";
			continue;
		}
		printf("Analising [%i/%i] %.*s \e[K\r", i, entries, fl_fileOutSize, files[i].name.c_str());

		string out = exec(string("sudo hdparm --fibmap \"") + files[i].name + "\"");
		out = out.substr(out.find("sectors", out.find("sectors")+1)+8);
		int firstInd	= out.find_first_not_of(" ");
		int secondInd	= out.find_first_not_of(" ", out.find(" ", firstInd));
		int thirdInd	= out.find_first_not_of(" ", out.find(" ", secondInd));
		string secondVal= out.substr(secondInd, out.find(" ", secondInd)-secondInd);
		string thirdVal = out.substr(thirdInd, out.find(" ", thirdInd)-thirdInd);
		if(secondVal == "-") { savedDataO << "0 0\n"; continue; }
		files[i].start	= std::stol(secondVal);
		files[i].end	= std::stol(thirdVal);
		savedDataO << secondVal << " " << thirdVal << "\n";
	}
	exec("sudo -k");
	savedDataO.close();
}

void sortFiles(){
	int size = entries;
	long* data = (long*)calloc(size, sizeof(long));
	sortedFiles = (int*)calloc(size, sizeof(int));
	for (int i=0; i<size; i++){
		sortedFiles[i]=i;
		data[i]=files[i].start;
		while(data[i] == 0){
			size--;
			data[i]=files[size].start;
			sortedFiles[i]=size;
			if(i==size){break;}
		}
	}
	sort(data, sortedFiles, size, "sort");
}

void getDiskData(){
	if(fs::exists(replace(cacheLoc, "<>", "disk"))){
		std::ifstream in(replace(cacheLoc, "<>", "disk"));
		string disk, partitionStr, sectorCountStr;
		std::getline(in, disk);
		std::getline(in, partitionStr);
		std::getline(in, sectorCountStr);
		sectorCount = std::stol(sectorCountStr);
		printf("Disk %s, partition %s. Has %li sectors.\n",
			disk.c_str(), partitionStr.c_str(), sectorCount);
		return;
	}
	string diskStr = exec(string("df -P \"" + parentDir + "\""));
	diskStr = diskStr.substr(diskStr.find("\n")+1);
	diskStr = diskStr.substr(0, diskStr.find(" "));
	string sectorCountStr = exec(string("sudo -S fdisk " + diskStr + " -l; sudo -k"));
	sectorCountStr = sectorCountStr.substr(sectorCountStr.find("bytes, ") + 7);
	sectorCount = std::stol(sectorCountStr.substr(0, sectorCountStr.find("\n")));
	int partitionInd = diskStr.find_last_not_of("0123456789")+1;

	std::ofstream out(replace(cacheLoc, "<>", "disk"));
	out << diskStr.substr(0,partitionInd) << "\n" << diskStr.substr(partitionInd) << "\n" << sectorCount;
	printf("Disk %s, partition %s. Has %li sectors.\n",
		diskStr.substr(0,partitionInd).c_str(), diskStr.substr(partitionInd).c_str(), sectorCount);
}

void identifySpace(){
	long lastEnd = 1;
	int count = 0;
	printf("Searching for empty space... \r");
	int small=0, medium=0, large=0, dlarge=0;
	long smallS, mediumS, largeS;
	smallS = sectorCount / 10000;	// 100GB -> 10mb 0.01%
	mediumS = sectorCount / 100;	// 100GB -> 1GB		1%
	largeS = sectorCount / 10;		// 100GB -> 10GB   10%

	spaces = (FileSpace*) calloc(entries+1, sizeof(FileSpace));
	for(int i=0; i<entries; i++){
		if(files[sortedFiles[i]].start > lastEnd){
			spaces[count].start = lastEnd;
			spaces[count].end = files[sortedFiles[i]].start;
			spaces[count].size = files[sortedFiles[i]].start - lastEnd;

			if(spaces[count].size < smallS){ small++; }
			else if(spaces[count].size < mediumS){ medium++; }
			else if(spaces[count].size < largeS){ large++; }
			else{ dlarge++;	}

			count++;
			printf("Found %i empty spaces...\r", count);
		}
		lastEnd = files[sortedFiles[i]].end;
	}
	if(lastEnd != sectorCount){
		spaces[count].start = lastEnd;
		spaces[count].end = sectorCount;
		spaces[count].size = sectorCount - lastEnd;
		count++;
	}
	
	printf("There are %i empty spaces, of which...\n", count);
	printf(" \e[1m%i\e[0m are insignificant | \e[1m%i\e[0m of a normal size | and \e[1m%i\e[0m ar large"
								" | but \e[1m%i\e[0m are deadly large!\n", small, medium, large, dlarge);
}

void defragment(bool simple){
	// TODO
	if(simple){ // Searches for spaces of size above the threshold, and finds the next biggest fitting file
		return;
	}
}

string replace(string& str, const char* from, const char* to){
	string newStr = str;
	newStr.replace(str.find(from), strlen(from), to);
	return newStr;
}
string exec(string command){
	// TODO: add a single command check (sanitizer)
	char buffer[256];
    std::string result = "";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}
int* sort(long* data, int*& sorted, int size, const char* cache){
	if(!&sorted){ // If sorted is a nullptr
		sorted = (int*)calloc(size, sizeof(int));
		for (int i=0; i<size; i++){
			sorted[i]=i;
		}
	}
	if (fs::exists(replace(cacheLoc, "<>", cache))){
		std::ifstream file(replace(cacheLoc, "<>", cache));
		int i=0;
		string line;
  		while (std::getline(file, line)) {
			sorted[i] = std::stoi(line);
			i++;
			printf("Sorted %i entries...\r", size);
    	}
		entries=i;
		return sorted;
	}
	size--;
	
	while (size != 0){
		printf("Sorting %i remaining \r", size);
		int greatestInd = 0;
		long greatest = data[0];

		for (int i=0; i <= size; i++){
			if(data[i] > greatest){
				greatest = data[i];
				greatestInd = i;
			}
		}
		greatest = sorted[greatestInd];
		sorted[greatestInd] = sorted[size];
		sorted[size] = greatest;
		data[greatestInd] = data[size];
		
		size--;
	}

	std::ofstream file(replace(cacheLoc, "<>", cache));
	for(int i=0; i<entries; i++){
		file << sorted[i] << "\n";
	}
	
	file.close();
	free(data);
	return sorted;
}