// SPDX-License-Identifier: Apache-2.0
#include "atomic_store.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
namespace eleos::storage {
namespace {
struct Fd {
    int fd;
    explicit Fd(int f):fd(f){}
    ~Fd(){ if(fd>=0) close(fd); }
    Fd(const Fd&)=delete;
    Fd& operator=(const Fd&)=delete;
};
constexpr std::array<std::uint8_t,8> magic{'E','L','E','S','T','O','R','1'};
constexpr std::size_t headerSize=48;
using Header=std::array<std::uint8_t,headerSize>;
bool Name(const std::string& name) {
    if(name.empty()||name.size()>64||name=="."||name==".."||name[0]=='.') return false;
    for(unsigned char c:name) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
          (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')) return false;
    return true;
}
bool PrivateDir(int fd) {
    struct stat s{};
    return fd>=0 && fstat(fd,&s)==0 && S_ISDIR(s.st_mode) &&
           s.st_uid==geteuid() && (s.st_mode&077)==0;
}
bool PrivateFile(int fd,struct stat& s) {
    return fd>=0&&fstat(fd,&s)==0&&S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&
           (s.st_mode&077)==0;
}
ssize_t Read(int fd,void* buf,std::size_t n) {
    ssize_t result;
    do {result=read(fd,buf,n);} while(result<0&&errno==EINTR);
    return result;
}
bool ReadExact(int fd,std::uint8_t* out,std::size_t n) {
    while(n) {
        auto got=Read(fd,out,n);
        if(got<=0) return false;
        out+=got;n-=static_cast<std::size_t>(got);
    }
    return true;
}
bool MakeHeader(const Bytes& data, Header& header) {
    if(data.size()>kMaxBytes) return false;
    std::copy(magic.begin(),magic.end(),header.begin());
    const auto len=static_cast<std::uint64_t>(data.size());
    for(unsigned i=0;i<8;i++) header[8+i]=static_cast<std::uint8_t>(len>>(56-8*i));
    unsigned hashLen=0;
    const std::uint8_t empty=0;
    return EVP_Digest(data.empty()?&empty:data.data(),data.size(),
        header.data()+16,&hashLen,EVP_sha256(),nullptr)==1&&hashLen==32;
}
std::string Temporary() {
    std::array<unsigned char,16> random{};
    if(RAND_bytes(random.data(),random.size())!=1) return {};
    constexpr char hex[]="0123456789abcdef";
    std::string name=".ele-tmp-";
    for(auto byte:random){name+=hex[byte>>4];name+=hex[byte&15];}
    return name;
}
bool Sync(int fd) {
    int result; do {result=fsync(fd);} while(result<0&&errno==EINTR);
    return result==0;
}
}
bool WriteAll(int fd,const std::uint8_t* data,std::size_t n) {
    while(n) {
        ssize_t got;
        do {got=write(fd,data,n);} while(got<0&&errno==EINTR);
        if(got<=0)return false;
        data+=got;n-=static_cast<std::size_t>(got);
    }
    return true;
}
bool ReadBounded(int fd,Bytes& data) {
    Bytes result;
    std::array<std::uint8_t,8192> buf{};
    for(;;) {
        auto n=Read(fd,buf.data(),buf.size());
        if(n<0)return false;
        if(n==0)break;
        if(static_cast<std::size_t>(n)>kMaxBytes-result.size())return false;
        result.insert(result.end(),buf.begin(),buf.begin()+n);
    }
    data.swap(result);
    return true;
}
Result Put(const std::string& directory,const std::string& name,const Bytes& data) {
    if(!Name(name)||data.size()>kMaxBytes)return Result::Rejected;
    Fd dir(open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if(!PrivateDir(dir.fd))return Result::Rejected;
    struct stat target{};
    if(fstatat(dir.fd,name.c_str(),&target,AT_SYMLINK_NOFOLLOW)==0) {
        if(!S_ISREG(target.st_mode)||target.st_uid!=geteuid()||(target.st_mode&077))return Result::Rejected;
    } else if(errno!=ENOENT) return Result::Rejected;
    Header header{};
    if(!MakeHeader(data,header))return Result::Rejected;
    std::string temp=Temporary();
    if(temp.empty())return Result::Rejected;
    Fd out(openat(dir.fd,temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
    if(out.fd<0)return Result::Rejected;
    if(!WriteAll(out.fd,header.data(),header.size())||!WriteAll(out.fd,data.data(),data.size())||
       !Sync(out.fd)||renameat(dir.fd,temp.c_str(),dir.fd,name.c_str())!=0) {
        unlinkat(dir.fd,temp.c_str(),0);
        return Result::Rejected;
    }
    // After rename, an fsync failure cannot safely be reported as no write.
    return Sync(dir.fd)?Result::Ok:Result::DurabilityUncertain;
}
Result Get(const std::string& directory,const std::string& name,Bytes& data) {
    if(!Name(name))return Result::Rejected;
    Fd dir(open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if(!PrivateDir(dir.fd))return Result::Rejected;
    Fd in(openat(dir.fd,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
    struct stat s{};
    if(!PrivateFile(in.fd,s)||s.st_size<static_cast<off_t>(headerSize)||
        s.st_size>static_cast<off_t>(headerSize+kMaxBytes))return Result::Rejected;
    Header header{};
    if(!ReadExact(in.fd,header.data(),header.size())||
       !std::equal(magic.begin(),magic.end(),header.begin()))return Result::Rejected;
    std::uint64_t len=0;
    for(unsigned i=0;i<8;i++)len=(len<<8)|header[8+i];
    if(len>kMaxBytes||len!=static_cast<std::uint64_t>(s.st_size-headerSize))return Result::Rejected;
    Bytes candidate(static_cast<std::size_t>(len));
    std::uint8_t extra=0;
    Header expected{};
    if(!ReadExact(in.fd,candidate.data(),candidate.size())||Read(in.fd,&extra,1)!=0||
       !MakeHeader(candidate,expected)||CRYPTO_memcmp(header.data(),expected.data(),header.size())!=0)
       return Result::Rejected;
    data.swap(candidate);
    return Result::Ok;
}
}
