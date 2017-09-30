#include "GlobalFuncs.h"

std::string ws2s(const std::wstring &s)
{
	int len = 0;
	int slength = (int)s.length() + 1;
	len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, NULL, NULL);
	char* buf = new char[len];
	WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, buf, len, NULL, NULL);
	std::string rtn(buf);
	delete[] buf;
	return rtn;
}

std::string GetFileName(std::string path)
{
	char sep = '\\';
	size_t i = path.rfind(sep, path.length());
	if (i != std::string::npos)
		return (path.substr(i + 1, path.length() - i));

	return("");
}

