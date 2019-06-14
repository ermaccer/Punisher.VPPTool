#pragma once



std::streampos getSizeToEnd(std::ifstream& is)
{
	auto currentPosition = is.tellg();
	is.seekg(0, is.end);
	auto length = is.tellg() - currentPosition;
	is.seekg(currentPosition, is.beg);
	return length;
}

std::string splitString(std::string& str, bool file) {

	std::string str_ret;
	int fnd = str.find_last_of("/\\");

	
	if (file == true) str_ret = str.substr(fnd + 1);
	if (file == false) str_ret = str.substr(0, fnd);
	return str_ret;
}

int calcOffsetFromPad(int val, int padsize)
{
	int retval = val;
	if (!(retval % padsize == 0))
	{
		do
		{
			retval++;
		} while (retval % padsize != 0);

	}
	return retval;
}