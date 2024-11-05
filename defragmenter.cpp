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
string locSort = "/tmp/disk_analize_sort2.txt";

std::string line;
int fl_fileOutSize = 40;

std::vector<string> files;
int entries=0;
long* dataStart=0;
long* dataEnd=0;
int* sorted=0;
std::vector<long> spaceS, spaceE, spaceSize;

bool procArgs(int argS, const char* args[]);	// Process every arg passed
bool readIndex();								// Reads the file with the index, returns if succeded
void index(string path);						// Indexes every file in the given path, and attempts saving it
void analize();									// Analizes where does the file starts and ends with hdparm --fibmap
int* sort(int size, long* data, int*& indexes);	// Generates a sorted index of the data
void identifySpace();							// With the sorted data, it can identify where unused sectors in the disk
void defragment();								// TODO: Defragments the data, ONLY suggestions on what to do.

string exec(string command);

int main(int argS, const char* args[]){
	if (!procArgs(argS, args)) { return -1; }
	fl_fileOutSize = std::stoi(exec("tput cols"));

	printf("Indexing...\r");
	if(!readIndex()) { index(parentDir); }

	dataStart = (long*) calloc(entries, sizeof(long));
	dataEnd = (long*) calloc(entries, sizeof(long));

	printf("Analizing...\r");
	analize();
	printf("Analisis Completed! 100%%\e[K\n");

	printf("Sorting... \r");
	sort(entries, dataStart, sorted);
	printf("Finished sorting!       \n");

	printf("Searching for empty space... \r");
	identifySpace();

	defragment();
	
	free(sorted); free(dataEnd); free(dataStart);
	return 0;
}

bool procArgs(int argS, const char* args[]){
	if (argS <= 1) { return false; }
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
			printf(" | mini defragmenter.cpp - Version 0.5.5\n");
			return false;
		} else
		if(help == args[i]){
			printf("No."); // TODO: Be polite
			return false;
		}
	}
	parentDir = args[argS - 1];
	return true;
}

bool readIndex(){
	int i=0;
	entries=0;
	if(!fs::exists(locIndex)){return false;}
	std::ifstream file(locIndex);

    while (std::getline(file, line)) {
		i++;
        printf("Indexing: %i files...\r", i);
		files.push_back(line);
    }
	printf("Indexed: %i files!   \n", i);
	entries = i;

	return true;
}

void index_(string dir){
	const char* dirName = dir.c_str();
	for (const fs::directory_entry & entry : fs::directory_iterator(dir)) {
		if(entry.is_directory()){
			index_(entry.path());
			continue;
		}
        printf("Indexing: %i files, at: %.*s\r", entries, fl_fileOutSize, dirName);
		files.push_back(entry.path());
		entries++;
	}
}
void index(string dir){
	entries=0;
	index_(dir);
	std::ofstream out(locIndex);
	if(!out.good()) { printf("Indexed: %i files! (Not Saved...)\e[K\n", entries); return; }

    for(int i=0; i<entries; i++){
		out << files[i] << "\n";
	}
    out.close();
	printf("Indexed: %i files! (Saved)\e[K\n", entries);
}

void analize(){
	std::ifstream savedDataI(locData);
	int i=0, it=0;
    while (std::getline(savedDataI, line)) {
		dataStart[i]	= std::stol(line.substr(0, line.find(" ")));
		dataEnd[i]		= std::stol(line.substr(line.find(" ")+1));
		i++;
        printf("Reading saved data: %i/%i\r", i, entries);
    }
	if(i!=0) { printf("Read saved data %i/%i    \n", i, entries); }

	std::ofstream savedDataO(locData);
	for (; i<entries; i++){
		if(!fs::exists(files[i])) {
			dataStart[i] = 0; dataEnd[i] = 0;
			savedDataO << "0 0\n";
			continue;
		}
		printf("Analising [%i/%i] %.*s \e[K\r", i, entries, fl_fileOutSize, files[i].c_str());

		string out = exec(string("sudo hdparm --fibmap \"") + files[i] + "\"");
		out = out.substr(out.find("sectors", out.find("sectors")+1)+8);
		int firstInd	= out.find_first_not_of(" ");
		int secondInd	= out.find_first_not_of(" ", out.find(" ", firstInd));
		int thirdInd	= out.find_first_not_of(" ", out.find(" ", secondInd));
		string secondVal= out.substr(secondInd, out.find(" ", secondInd)-secondInd);
		string thirdVal = out.substr(thirdInd, out.find(" ", thirdInd)-thirdInd);
		if(secondVal == "-") { savedDataO << "0 0\n"; continue; }
		dataStart[i]	= std::stol(secondVal);
		dataEnd[i]		= std::stol(thirdVal);
		savedDataO << secondVal << " " << thirdVal << "\n";
	}
	savedDataO.close();

	entries = i;
}

int* sort(int size, long* data_, int*& indexes){
	indexes = (int*)calloc(size, sizeof(int));
	if (fs::exists(locSort)){
		std::ifstream file(locSort);
		int i=0;
  		while (std::getline(file, line)) {
			indexes[i] = std::stoi(line);
			i++;
			printf("Sorting %i remaining \r", size);
    	}
		entries=i;
		return indexes;
	}
	long* data = (long*)calloc(size, sizeof(long));
	memcpy(data, data_, size*sizeof(long));

	for (int i=0; i<size; i++){
		indexes[i]=i;
		while(data[i] == 0){
			size--;
			data[i]=data[size];
			data[size]=1;
			indexes[i]=size;
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
		greatest = indexes[greatestInd];
		indexes[greatestInd] = indexes[size];
		indexes[size] = greatest;
		data[greatestInd] = data[size];
		
		size--;
	}

	std::ofstream file(locSort);
	for(int i=0; i<entries; i++){
		file << indexes[i] << "\n";
	}
	
	file.close();
	free(data);
	return indexes;
}

void identifySpace(){
	int lastEnd = 1;
	for(int i; i<entries; i++){
		if(dataStart[i] > lastEnd){
			spaceS.push_back(lastEnd);
			spaceE.push_back(dataStart[i]);
			spaceSize.push_back(dataStart[i] - lastEnd);
		}
		lastEnd = dataEnd[i];
	}
	int sectorCount=234412032;
	if(lastEnd != sectorCount){
		spaceS.push_back(lastEnd);
		spaceE.push_back(sectorCount);
		spaceSize.push_back(sectorCount - lastEnd);
	}
}

void defragment(bool simple, void* arg){
	// TODO
	if(simple){ // Searches for spaces of size above the threshold, and finds the next biggest fitting file
		return;
	}
}

string exec(string command){
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