// SPDX-License-Identifier: Apache-2.0
#include "atomic_store.h"
#include <iostream>
#include <string>
#include <unistd.h>
int main(int argc,char** argv) {
    using namespace eleos::storage;
    if(argc!=4){std::cerr<<"usage: ele-store put|get|verify PRIVATE_DIR NAME\n";return 2;}
    std::string op=argv[1];
    Bytes data;
    Result result=Result::Rejected;
    if(op=="put") {
        if(ReadBounded(STDIN_FILENO,data))result=Put(argv[2],argv[3],data);
    } else if(op=="get"||op=="verify") {
        result=Get(argv[2],argv[3],data);
        if(result==Result::Ok&&op=="get"&&!WriteAll(STDOUT_FILENO,data.data(),data.size()))return 1;
    } else return 2;
    if(result==Result::Ok)return 0;
    if(result==Result::DurabilityUncertain){std::cerr<<"Renamed but durability uncertain\n";return 3;}
    std::cerr<<"Storage request rejected or integrity check failed\n";return 1;
}
