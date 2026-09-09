#include "Engine.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#if defined(_MSC_VER)
#include <malloc.h>
#endif
namespace {
thread_local bool measuring=false;
thread_local std::size_t allocations=0;
void* allocate(std::size_t size) {
    if(measuring) ++allocations;
    if(void* p=std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* alignedAllocate(std::size_t size,std::size_t alignment) {
    if(measuring) ++allocations;
    void* p=nullptr;
#if defined(_MSC_VER)
    p=_aligned_malloc(size ? size : 1,alignment);
#else
    if(posix_memalign(&p,alignment,size ? size : 1)!=0) p=nullptr;
#endif
    if(!p) throw std::bad_alloc();
    return p;
}
void alignedFree(void* p) noexcept {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
}
void* operator new(std::size_t n) {return allocate(n);}
void* operator new[](std::size_t n) {return allocate(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
void* operator new(std::size_t n,std::align_val_t a) {return alignedAllocate(n,static_cast<std::size_t>(a));}
void* operator new[](std::size_t n,std::align_val_t a) {return alignedAllocate(n,static_cast<std::size_t>(a));}
void operator delete(void* p,std::align_val_t) noexcept {alignedFree(p);}
void operator delete[](void* p,std::align_val_t) noexcept {alignedFree(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept {alignedFree(p);}
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept {alignedFree(p);}
int main() {
    using namespace geiger;
    Engine engine;
    double energy=0;
    for(double sr:{1000.25,44100.,48000.,96000.,192000.}) {
        if(!engine.prepare(sr)) return 1;
        auto values=defaults();
        measuring=true;
        for(int n=0;n<96000;++n) {
            if(n%127==0) {
                const auto i=static_cast<std::size_t>(n/127)%Count;
                values[i]=(n/127)%2 ? definitions[i].minimum : definitions[i].maximum;
                engine.configure(values);
            }
            if(n%4096==0) {engine.reset(); engine.setGate(true,0.7,12); engine.testClick();}
            const auto f=engine.tick(); energy+=f.left*f.left+f.right*f.right;
        }
        measuring=false;
    }
    if(allocations!=0 || !std::isfinite(energy) || energy<=0) {
        std::cerr<<"Realtime allocation/finite-output regression: "<<allocations<<" C++ allocations\n";
        return 2;
    }
    std::cout<<"PASS zero C++ heap allocations in configure, reset, gate, testClick and tick under extreme automation\n";
}
