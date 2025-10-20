#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <unistd.h>
#include <string>
#include <vector>

int getRandomNumber(int from, int to);
std::vector<uint8_t> generateRandomData(size_t size);
void write2file(std::string& sfilename, const char* data, ssize_t size);
std::string generateUuid();

bool write_session_uuid(const std::string& client_session_uuid, const std::string& filename);
bool read_session_uuid(const std::string& filename, std::string& result);

#endif // UTILS_H
