#include "check.hpp"
int main(int argc,char** argv) { if(argc!=2) return 1; return run([&]{ EventStream s;
 switch(test_number(argv)) {
 case 19: CHECK(s.admit({1,10,100,101})); CHECK(s.admit({2,10,100,101})); CHECK(s.admit({3,11,100,101})); CHECK(s.events().size()==3); break;
 case 20: CHECK(s.admit({1,10,100,101})); CHECK(s.admit({3,10,100,101})); CHECK(s.admit({2,9,0,-1}).error==Error::SEQ_NOT_INCREASING); CHECK(s.events().size()==2); CHECK(!s.contains(2)); break;
 case 21: CHECK(s.admit({1,10,100,101})); CHECK(s.admit({2,9,0,-1}).error==Error::TIME_DECREASING); CHECK(s.events().size()==1); break;
 case 22: {auto r=s.admit({8,10,101,100}); CHECK(r.seq==8 && r.error==Error::CROSSED_QUOTE); CHECK(s.events().empty()); break;}
 case 23: CHECK(s.admit({1,10,0,-1}).error==Error::NON_POSITIVE_PRICE); CHECK(s.events().empty()); break;
 default: CHECK(false);
 }
 }); }
