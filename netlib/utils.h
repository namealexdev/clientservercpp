#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <string>

int getRandomNumber(int from, int to);
void write2file(std::string& sfilename, const char* data, ssize_t size);
std::string generateUuid();

#endif // UTILS_H
