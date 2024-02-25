#pragma once

#include <iostream>
#include <fstream>
#include "thirdparty/cjson/cJSON.h"

static cJSON *parse_json_file(std::string filename) {
    std::ifstream fs(filename);

    // assert(fs.open());

    fs.seekg(0, std::ios::end);
    std::streampos file_size = fs.tellg();
    fs.seekg(0, std::ios::beg);

    char* json_content = new char[file_size];

    fs.read(json_content, file_size);
    fs.close();

    std::cout << "===== json file content =====" << std::endl;
    std::cout << json_content;

    cJSON *res = cJSON_Parse(json_content);

    delete[] json_content;
    return res;
}