// Punisher.VPPTool.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <filesystem>
#include <sstream>
#include "filef.h"
struct VPPHeader {
	int       header; // 0x51890ACE
	int       version; // 3 - The Punisher, 1 - Summoner
	int       files;
	int       size;
	int       padsize;
};

struct VPPEntry {
	char        filename[24] = {};
	int         unk;
	int         size;
};


enum eMode {
	extract,
	create,
};



int main(int argc, char* argv[])
{
	if (argc != 3) {
		std::cout << "Usage: VPPTool <mode> <input> \n Available modes: \n extract \n create" << std::endl;
		return 1;
	}

	if (!(strcmp(argv[1], "e") == 0 || strcmp(argv[1], "c") == 0)) {
		std::cout << "ERROR: Invalid mode selected! Available modes: \n extract \n create" << std::endl;
		return 1;
	}
	bool mode;

	if ((strcmp(argv[1], "e") == 0)) mode = extract;
	if ((strcmp(argv[1], "c") == 0)) mode = create;


	if (mode == extract)
	{
		std::ifstream pFile(argv[2], std::ifstream::binary);

		if (!pFile) {
			std::cout << "ERROR: Could not open " << argv[2] << "!" << std::endl;
			return 1;
		}

		VPPHeader vpp;
		pFile.read((char*)&vpp, sizeof(VPPHeader));
		int headerPos = (int)pFile.tellg();
		if (vpp.header != 0x51890ACE || vpp.version != 3) {
			std::cout << "ERROR: " << argv[2] << " is not a valid VPP archive!" << std::endl;
			return 1;
		}


		// calc and skip pad
		std::unique_ptr<char[]> pad = std::make_unique<char[]>((vpp.padsize) - sizeof(VPPHeader));
		pFile.read((char*)&pad, sizeof(pad));
		
		// get files

		std::string folder = argv[2];
		folder += "_extract";
		std::experimental::filesystem::create_directory(folder);

		std::unique_ptr<VPPEntry[]> ent = std::make_unique<VPPEntry[]>(vpp.files);

	
		pFile.seekg(calcOffsetFromPad(sizeof(pad), vpp.padsize), pFile.beg);
		
		int padPos = 0;
		for (int i = 0; i < vpp.files; i++) {
			pFile.read((char*)&ent[i], sizeof(VPPEntry));
		}

		pFile.seekg(calcOffsetFromPad((sizeof(VPPEntry) * vpp.files), vpp.padsize) + vpp.padsize, pFile.beg);

		
		int headerSize = calcOffsetFromPad((sizeof(VPPEntry) * vpp.files), vpp.padsize) + vpp.padsize;
		int currentPos = (int)pFile.tellg();
		for (int i = 0; i < vpp.files; i++)
		{
			// create output
			std::string filename = ent[i].filename;
			std::string output = folder;
			output += '\\';
			output += filename;
			std::ofstream oFile(output, std::ofstream::binary);
			// extract content
			std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(ent[i].size);
			pFile.read(dataBuff.get(), ent[i].size);
			std::cout << "Processing: " << ent[i].filename << std::endl;
			oFile.write(dataBuff.get(), ent[i].size);
			pFile.seekg(calcOffsetFromPad((int)pFile.tellg(), vpp.padsize), pFile.beg);
			

		}
		std::cout << "Finished." << std::endl;
	}


	if (mode == create)
	{
		std::experimental::filesystem::path folder(argv[2]);
		if (!std::experimental::filesystem::exists(folder))
		{
			std::cout << "ERROR: Could not open directory: " << argv[2] << "!" << std::endl;
			return 1;
		}

		if (std::experimental::filesystem::exists(folder))
		{

			int filesFound = 0;
			// get number
			for (const auto & file : std::experimental::filesystem::recursive_directory_iterator(folder))
			{
				if (file.path().has_extension())
					filesFound++;
				
			}

			// get dirs
			std::unique_ptr<std::string[]> filePaths = std::make_unique<std::string[]>(filesFound);
			std::unique_ptr<std::string[]> fileNames = std::make_unique<std::string[]>(filesFound);
			std::unique_ptr<int[]> sizes = std::make_unique<int[]>(filesFound);
			int i = 0;
			for (const auto & file : std::experimental::filesystem::recursive_directory_iterator(folder))
			{
				if (file.path().has_extension())
				{
					filePaths[i] = file.path().string();
					fileNames[i] = file.path().filename().string();
					std::ifstream tFile(filePaths[i], std::ifstream::binary);
					if (tFile)
					{
						sizes[i] = (int)getSizeToEnd(tFile);
					}
					i++;
					
				}
			}
		    
			// create VPP header 
			VPPHeader vpp;
			vpp.files = filesFound;
			vpp.header = 0x51890ACE;
			vpp.padsize = 2048;
			vpp.version = 3;
			vpp.size = sizeof(VPPHeader) + sizeof(VPPEntry) * vpp.files;
			vpp.size += calcOffsetFromPad(vpp.size, vpp.padsize);
			for (int i = 0; i < vpp.files; i++) vpp.size += calcOffsetFromPad(sizes[i], 2048);;
			vpp.size += calcOffsetFromPad(sizeof(VPPEntry) * vpp.files, vpp.padsize) - sizeof (VPPEntry) + 12;
			


			// create output
			std::string output = argv[2];
			output += ".vpp";
			std::ofstream oFile(output,std::ofstream::binary);

			// write header and pad
			oFile.write((char*)&vpp, sizeof(VPPHeader));


			for (int i = 0; i < vpp.padsize - sizeof(VPPHeader); i++)
			{
				char pad = 0;
				oFile.write((char*)&pad, sizeof(char));
			}


			for (int i = 0; i < vpp.files; i++)
			{
				VPPEntry ent;
				sprintf(ent.filename, "%s", fileNames[i].c_str());
				ent.unk = sizes[i];
				ent.size = sizes[i];
				oFile.write((char*)&ent, sizeof(VPPEntry));

			}
			for (int i = 0; i < calcOffsetFromPad(sizeof(VPPEntry) * vpp.files, vpp.padsize) - sizeof(VPPEntry) * vpp.files; i++)
			{
				char pad = 0;
				oFile.write((char*)&pad, sizeof(char));
			}

			// write files!

			for (int i = 0; i < vpp.files; i++)
			{
				std::ifstream pFile(filePaths[i], std::ifstream::binary);

				if (pFile)
				{
					int size = (int)getSizeToEnd(pFile);
					std::cout << "Processing: " << fileNames[i] <<  std::endl;
					std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(size);

					pFile.read(dataBuff.get(), size);
					oFile.write(dataBuff.get(), size);
					for (int i = 0; i < calcOffsetFromPad(size, vpp.padsize) - size; i++)
					{
						char pad = 0;
						oFile.write((char*)&pad, sizeof(char));
					}
				}
			}
			std::cout << "Finished." << std::endl;
		    
		}
	}
	
    return 0;
}
