#include "check.hpp"
int main(int argc,char** argv) { if(argc!=2) return 1; return run([&]{
 switch(test_number(argv)) {
 case 13: for(auto budget:{1000,1099,1100,2500}) { auto r=size_for(budget,8,125,50,0); CHECK(r.error==Error::NONE); CHECK(r.risk_per_contract==1100); CHECK(r.size==(budget<1100?0:1)); } break;
 case 14: { auto r=size_for(1000,8,50,50,0); CHECK(r.risk_per_contract==500 && r.size==1 && r.error==Error::NONE); break; }
 case 26: for(auto budget:{1349,1350}) {auto r=size_for(budget,8,125,50,2); CHECK(r.risk_per_contract==1350 && r.size==(budget==1350?1:0));} break;
 case 27: for(auto r:{size_for(1000,hi,125,0,0),size_for(1000,1,125,hi,0),size_for(1000,1,125,0,hi),size_for(hi,hi,1,1,0)}) CHECK(r.error==Error::OVERFLOW && r.size==0); break;
 case 28:
 CHECK(size_for(1000,8,125,-1,0).error==Error::INVALID_COST); CHECK(size_for(1000,8,125,0,-1).error==Error::INVALID_COST);
 CHECK(size_for(1000,8,125,0,0).size==1); CHECK(size_for(1000,8,125,0,0).error==Error::NONE);
 CHECK(size_for(0,0,0,-1,-1,0).error==Error::NON_POSITIVE_INPUT); break;
 default: CHECK(false);
 }
 }); }
