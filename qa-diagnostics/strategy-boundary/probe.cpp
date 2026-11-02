#include "fnq/strategy.hpp"
#include <cassert>
#include <iostream>
using namespace fnq;
int main() {
 for (Ticks d : {88,89}) {
  auto ss=*session_2026("2026-11-03").value; Strategy s(ss); std::uint64_t seq=0;
  auto quote_mid=[&](std::int64_t t,Ticks m) { assert(s.quote({++seq,t,m/2,m-m/2})==Error::NONE); };
  auto bar=[&](int i,Ticks o,Ticks h,Ticks l,Ticks c) {
   Ticks values[]={o,h,l,c};
   for(int j=0;j<4;++j) quote_mid(ss.open+(i-1)*quarter+(j+1)*second,values[j]);
  };
  for(int i=1;i<=8;++i) bar(i,160000,160044,160044-d,160000);
  bar(9,160044,160000+d,160000,160080);
  auto label=ss.open+9*quarter;
  assert(s.advance_time(label)==Error::NONE);
  if(d==88) {
   assert(s.pending() && s.pending()->entry && s.pending()->distance==88);
   assert(s.quote({++seq,label,80040,80041})==Error::NONE);
   assert(s.state().position==1 && s.entries()==1 && s.stop()==79953);
   assert(s.quote({++seq,label+second,79953,79954})==Error::NONE);
   assert(s.state().position==0 && s.state().net==-4600);
  } else {
   assert(!s.pending() && s.state().position==0 && s.entries()==0);
   bool rejected=false; for(const auto& r:s.log()) if(r.reason=="SIZE_ZERO") rejected=true;
   assert(rejected);
  }
  std::cout<<"D="<<d<<" boundary PASS\n";
 }
}
