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
string locIndex = "/tmp/disk_analize_index.txt";
string locData = "/tmp/disk_analize_data.txt";
string locSort = "/tmp/disk_analize_sort.txt";

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
int sectorCount;
int entries;

int procArgs(int argS, const char* args[]);	// Process every arg passed
bool readIndex();								// Reads the file with the index, returns if succeded
void index(string path);						// Indexes every file in the given path, and attempts saving it
void analize();									// Analizes where does the file starts and ends with hdparm --fibmap
int* sort();									// Generates a sorted index of the data
void identifySpace();							// With the sorted data, it can identify where unused sectors in the disk
void defragment(bool simple);					// TODO: Defragments the data, ONLY suggestions on what to do.

string exec(string command);

int main(int argS, const char* args[]){
	int ret = procArgs(argS, args);
	if (ret != 0) { return ret; }
	fl_fileOutSize = std::stoi(exec("tput cols"));

	printf("Indexing...\r");
	if(!readIndex()) { index(parentDir); }

	printf("Analizing...\r");
	analize();
	printf("Analisis Completed! 100%%\e[K\n");

	printf("Sorting... \r");
	sortedFiles = sort();
	printf("Finished sorting!       \n");

	printf("Searching for empty space... \r");
	identifySpace();

	defragment(true);
	
	free(sortedFiles); free(files); free(spaces);
	return 0;
}

int procArgs(int argS, const char* args[]){
	if (argS <= 1) { return 1; }
	string refresh("-r");
	string version("-v");
	string help("--help");

	for(int i=1; i < argS; i++){
		if(refresh == args[i]){
			fs::remove(locIndex);
			fs::remove(locData);
			fs::remove(locSort);
		} else
		if(version == args[i]){
			printf(" | mini defragmenter.cpp - Version 0.6.5\n");
			return 0;
		} else
		if(help == args[i]){
			printf("No."); // TODO: Be polite
			return 0;
		}
	}
	parentDir = args[argS - 1];
	return 0;
}

bool readIndex(){
	if(!fs::exists(locIndex)){return false;}

	int i=0;
	std::ifstream file(locIndex);
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
	std::ofstream out(locIndex);

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
	std::ifstream savedDataI(locData);
	int i=0;
	string line;
    while (std::getline(savedDataI, line)) {
		files[i].start	= std::stol(line.substr(0, line.find(" ")));
		files[i].end	= std::stol(line.substr(line.find(" ")+1));
		i++;
        printf("Reading saved data: %i/%i\r", i, entries);
    }
	if(i!=0) { printf("Read saved data: %i/%i   \n", i, entries); }

	std::ofstream savedDataO(locData);
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
	savedDataO.close();
}

int* sort(){
	int size = entries;
	int* sortedFiles = (int*)calloc(size, sizeof(int));
	if (fs::exists(locSort)){
		std::ifstream file(locSort);
		int i=0;
		string line;
  		while (std::getline(file, line)) {
			sortedFiles[i] = std::stoi(line);
			i++;
			printf("Sorting %i remaining \r", size);
    	}
		entries=i;
		return sortedFiles;
	}
	long* data = (long*)calloc(size, sizeof(long));

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
	entries = size;
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
		greatest = sortedFiles[greatestInd];
		sortedFiles[greatestInd] = sortedFiles[size];
		sortedFiles[size] = greatest;
		data[greatestInd] = data[size];
		
		size--;
	}

	std::ofstream file(locSort);
	for(int i=0; i<entries; i++){
		file << sortedFiles[i] << "\n";
	}
	
	file.close();
	free(data);
	return sortedFiles;
}

void identifySpace(){
	int lastEnd = 1, count = 0;
	spaces = (FileSpace*) calloc(entries, sizeof(FileSpace));
	for(int i; i<entries; i++){
		if(files[i].start > lastEnd){
			spaces[count].start = lastEnd;
			spaces[count].end = files[i].start;
			spaces[count].size = files[i].start - lastEnd;
			count++;
		}
		lastEnd = files[i].end;
	}
	
	string sectorCountStr = exec(string("df -P \"" + parentDir + "\""));
	sectorCountStr = sectorCountStr.substr(sectorCountStr.find("\n")+1);
	if(lastEnd != sectorCount){
		spaces[count].start = lastEnd;
		spaces[count].end = sectorCount;
		spaces[count].size = sectorCount - lastEnd;
		count++;
	}
}

void defragment(bool simple){
	// TODO
	if(simple){ // Searches for spaces of size above the threshold, and finds the next biggest fitting file
		return;
	}
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