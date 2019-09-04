// Punisher.VPPTool.cpp : Defines the entry point for the console application.
//


#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>


#include "filef.h"
#include "stdafx.h"
#include "zlib\zlib.h"

#pragma comment (lib, "zlib/zdll.lib" )

struct vpp_header {
	int       header; // 0x51890ACE
	int       version; // 3 - The Punisher, 1 - Summoner
	int       files;
	int       size;
	int       padsize;
};

struct vpp_entry {
	char        filename[24] = {};
	int         rawSize;
	int         size;
};


enum eMode {
	MODE_EXTRACT  = 1,
	MODE_CREATE,
	PARAM_COMPRESS_ZLIB,
	PARAM_DECOMPRESS_ALL,
};



int main(int argc, char* argv[])
{
	if (argc == 1) {
		std::cout << "VPPTool - a tool to work with The Punisher files\n"
			"Usage: VPPTool <params> <input>\n"
			"Params:\n"
			" -e                 Extracts input file\n"
			" -c                 Creates file from input folder\n"
			" -o <name>          Specifies output file or folder\n"
			" -z                 Compress all files using ZLIB for creation\n"
			" -d                 Decompress all files for extraction\n"
			" -l <name>          Specifies output or input level table\n"
			"Examples:\n"
			"vpptool -e -o tables_out tables.vpp\n   - Extracts tables.vpp into tables_out \n\n"
			"vpptool -c -o tables.vpp my_new_tables_folder\n   - Creates tables.vpp from my_new_tables_folder\n\n"
			"vpptool -e -l bar1_order.txt -d bar1.vpp \n   - Extracts level file, decompresses contents and writes order.\n\n"
			"vpptool -c -l bar1_order.txt -z bar1_out \n   - Creates level file, compresses contents and reads order.\n\n"
			"Level archives require specific order, this can be saved using the -l parameter."
			"Remember to decompress level archive data before recreating it!";
	}


	int mode = 0;
	int param = 0;
	std::string o_param;
	std::string l_param;
	// params
	for (int i = 1; i < argc - 1; i++)
	{
		if (argv[i][0] != '-' || strlen(argv[i]) != 2) {
			return 1;
		}
		switch (argv[i][1])
		{
		case 'e': mode = MODE_EXTRACT;
			break;
		case 'c': mode = MODE_CREATE;
			break;
		case 'z': param = PARAM_COMPRESS_ZLIB;
			break;
		case 'd': param = PARAM_DECOMPRESS_ALL;
			break;
		case 'o':
			i++;
			o_param = argv[i];
			break;
		case 'l':
			i++;
			l_param = argv[i];
			break;
		default:
			std::cout << "ERROR: Param does not exist: " << argv[i] << std::endl;
			break;
		}
	}



	if (mode == MODE_EXTRACT)
	{
		std::ifstream pFile(argv[argc - 1], std::ifstream::binary);

		if (!pFile) 
		{
			std::cout << "ERROR: Could not open: " << argv[argc - 1] << "!" << std::endl;
			return 1;
		}

		if (pFile)
		{
			// read and check header
			vpp_header vpp;
			pFile.read((char*)&vpp, sizeof(vpp_header));
			int headerPos = (int)pFile.tellg();
			if (vpp.header != 0x51890ACE || vpp.version != 3) {
				std::cout << "ERROR: " << argv[argc - 1] << " is not a valid VPP archive!" << std::endl;
				return 1;
			}

			// get files
			std::unique_ptr<vpp_entry[]> ent = std::make_unique<vpp_entry[]>(vpp.files);

			// skip padding
			pFile.seekg(calcOffsetFromPad((vpp.padsize) - sizeof(vpp_header), vpp.padsize), pFile.beg);

			// read entries
			for (int i = 0; i < vpp.files; i++) {
				pFile.read((char*)&ent[i], sizeof(vpp_entry));
			}

			// skip padding
			pFile.seekg(calcOffsetFromPad((sizeof(vpp_entry) * vpp.files), vpp.padsize) + vpp.padsize, pFile.beg);

			// write order
			if (!l_param.empty())
			{
				std::ofstream pLevel(l_param, std::ofstream::trunc | std::ofstream::out);

				for (int i = 0; i < vpp.files; i++)
				{
					std::string str = o_param;
					str += "\\";
					str += ent[i].filename;
					pLevel << str << std::endl;
				}
			}

			if (!o_param.empty())
			{
				if (!std::experimental::filesystem::exists(o_param))
					std::experimental::filesystem::create_directory(o_param);

				std::experimental::filesystem::current_path(
					std::experimental::filesystem::system_complete(std::experimental::filesystem::path(o_param)));
			}

			for (int i = 0; i < vpp.files; i++)
			{

				std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(ent[i].size);
				pFile.read(dataBuff.get(), ent[i].size);

				std::string filename = ent[i].filename;
				std::ofstream oFile(filename, std::ofstream::binary);

				std::cout << "Processing: " << ent[i].filename << std::endl;

				if (param == PARAM_DECOMPRESS_ALL)
				{

					// decompress data
					std::unique_ptr<char[]> uncompressedBuffer = std::make_unique<char[]>(ent[i].rawSize);
					unsigned long uncompressedSize = ent[i].rawSize;
					int zlib_output = uncompress((Bytef*)uncompressedBuffer.get(), &uncompressedSize,
						(Bytef*)dataBuff.get(), ent[i].size);

					if (zlib_output == Z_MEM_ERROR)
					{
						std::cout << "ERROR: ZLIB: Out of memory!" << std::endl;
						return 1;
					}

					oFile.write(uncompressedBuffer.get(), ent[i].rawSize);
				}
				else
					oFile.write(dataBuff.get(), ent[i].size);

				pFile.seekg(calcOffsetFromPad((int)pFile.tellg(), vpp.padsize), pFile.beg);
			}
			std::cout << "Finished." << std::endl;
		}
		
	}


	if (mode == MODE_CREATE)
	{
		if (!std::experimental::filesystem::exists(argv[argc - 1]))
		{
			std::cout << "ERROR: Could not open directory: " << argv[argc - 1] << "!" << std::endl;
			return 1;
		}

		if (std::experimental::filesystem::exists(argv[argc - 1]))
		{

			int filesFound = 0;
			// get files number
			for (const auto & file : std::experimental::filesystem::recursive_directory_iterator(argv[argc - 1]))
			{
				if (file.path().has_extension())
					filesFound++;
				
			}


			// stuffs
			std::unique_ptr<std::string[]> filePaths = std::make_unique<std::string[]>(filesFound); // full path
			std::unique_ptr<std::string[]> fileNames = std::make_unique<std::string[]>(filesFound); // file name
			std::unique_ptr<int[]> sizes = std::make_unique<int[]>(filesFound); // raw size
			std::unique_ptr<int[]> cmpSizes = std::make_unique<int[]>(filesFound); // zlibbed size

			std::string line;
			int i = 0;
			if (!l_param.empty())
			{
				std::ifstream pList(l_param, std::ifstream::binary);
				if (!pList)
				{
					std::cout << "ERROR: Could not open list file: " << l_param << "!" << std::endl;
					return 1;
				}
				while (std::getline(pList, line))
				{
					std::stringstream ss(line);
					ss >> filePaths[i];
					fileNames[i] = splitString(filePaths[i], true);
					i++;
				}
			}


			if (l_param.empty())
			{
				int i = 0;
				for (const auto & file : std::experimental::filesystem::recursive_directory_iterator(argv[argc - 1]))
				{
					if (file.path().has_extension())
					{
						filePaths[i] = file.path().string();
						fileNames[i] = file.path().filename().string();
						std::ifstream tFile(filePaths[i], std::ifstream::binary);
						if (tFile)
						{
							sizes[i] = (int)getSizeToEnd(tFile);
							if (param == PARAM_COMPRESS_ZLIB)
							{
								unsigned long compSize = sizes[i] * 1.2 + 12;
								std::unique_ptr<char[]> compBuff = std::make_unique<char[]>(compSize);
								std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(sizes[i]);
								tFile.read(dataBuff.get(), sizes[i]);
								compress((Bytef*)compBuff.get(), &compSize, (Bytef*)dataBuff.get(), sizes[i]);
								cmpSizes[i] = compSize;
							}

						}
						i++;

					}
				}
			}
			else
			{
				for (int i = 0; i < filesFound; i++)
				{
					std::ifstream pFile(filePaths[i], std::ifstream::binary);
					if (pFile)
					{
						sizes[i] = (int)getSizeToEnd(pFile);
						if (param == PARAM_COMPRESS_ZLIB)
						{
							unsigned long compSize = sizes[i] * 1.2 + 12;
							std::unique_ptr<char[]> compBuff = std::make_unique<char[]>(compSize);
							std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(sizes[i]);
							pFile.read(dataBuff.get(), sizes[i]);
							compress((Bytef*)compBuff.get(), &compSize, (Bytef*)dataBuff.get(), sizes[i]);
							cmpSizes[i] = compSize;
						}
					}
				}
			}

		    
			// create VPP header 
			vpp_header vpp;
			vpp.files = filesFound;
			vpp.header = 0x51890ACE;
			vpp.padsize = 2048;
			vpp.version = 3;
			vpp.size = vpp.padsize + sizeof(vpp_entry) * vpp.files;
			vpp.size += calcOffsetFromPad(sizeof(vpp_entry) * vpp.files, vpp.padsize);
			for (int i = 0; i < vpp.files; i++)
			{
				if (param == PARAM_COMPRESS_ZLIB)
					vpp.size += calcOffsetFromPad(cmpSizes[i], 2048);
				 else
				 vpp.size += calcOffsetFromPad(sizes[i], 2048);
			}
			
			// create output
			std::string output;
			if (!o_param.empty())
				output = o_param;
			else
				output = "new.vpp";

			std::ofstream oFile(output,std::ofstream::binary);

			// write header and pad
			oFile.write((char*)&vpp, sizeof(vpp_header));


			for (int i = 0; i < vpp.padsize - sizeof(vpp_header); i++)
			{
				char pad = 0;
				oFile.write((char*)&pad, sizeof(char));
			}

			for (int i = 0; i < vpp.files; i++)
			{
				vpp_entry ent;
				sprintf(ent.filename, "%s", fileNames[i].c_str());
				ent.rawSize= sizes[i];
				if (param == PARAM_COMPRESS_ZLIB)
				ent.size = cmpSizes[i];
				else ent.size = sizes[i];
				oFile.write((char*)&ent, sizeof(vpp_entry));

			}

			for (int i = 0; i < calcOffsetFromPad(sizeof(vpp_entry) * vpp.files, vpp.padsize) - sizeof(vpp_entry) * vpp.files; i++)
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
					if (param == PARAM_COMPRESS_ZLIB)
					{
						unsigned long compSize = size * 1.2 + 12;
						std::unique_ptr<char[]> compBuff = std::make_unique<char[]>(compSize);
						int zlib_output = compress((Bytef*)compBuff.get(), &compSize, (Bytef*)dataBuff.get(), size);

						if (zlib_output == Z_MEM_ERROR) {
							std::cout << "ERROR: ZLIB: Out of memory!" << std::endl;
							return 1;
						}
						size = compSize;
						oFile.write(compBuff.get(), compSize);
					}
					else
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
