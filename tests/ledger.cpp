#include "check.hpp"
struct Fixture {
 EventStream stream;
 Ledger ledger;
 std::uint64_t seq=0;
 Fixture(Instrument i=MES,Cents fee=50,Contracts cap=1):ledger(i,fee,cap) { for(std::uint64_t n=1;n<=10;++n) CHECK(stream.admit({n,10,1,2})); }
 Error fill(Side s,Contracts q,Ticks p) { ++seq; return ledger.fill(s,q,p,seq-1,seq,stream); }
 const State& state() const {return ledger.state();}
};
void round_trip(Instrument i,Side side,Ticks entry,Ticks exit,Cents fee,Cents gross,Cents net) {
 Fixture f(i,fee); CHECK(f.fill(side,1,entry)==Error::NONE); CHECK(f.state().net==-fee);
 CHECK(f.fill(side==Side::BUY?Side::SELL:Side::BUY,1,exit)==Error::NONE);
 CHECK(f.state().gross==gross && f.state().fees==fee*2 && f.state().net==net && f.state().position==0 && f.state().lots.empty());
}
void rejected(Fixture& f,Side side,Contracts q,Ticks p,std::uint64_t decision,std::uint64_t seq,Error expected) {
 const auto before=f.state(); const auto events=f.stream;
 CHECK(f.ledger.fill(side,q,p,decision,seq,f.stream)==expected); CHECK(f.state()==before); CHECK(f.stream==events);
}
int main(int argc,char** argv) { if(argc!=2) return 1; return run([&]{
 switch(test_number(argv)) {
 case 1: round_trip(MES,Side::BUY,20000,20004,50,500,400); break;
 case 2: round_trip(MES,Side::BUY,20000,19992,50,-1000,-1100); break;
 case 3: round_trip(MNQ,Side::SELL,80000,79990,50,500,400); break;
 case 4: round_trip(MNQ,Side::SELL,80000,80010,50,-500,-600); break;
 case 5: round_trip(MES,Side::BUY,20000,20000,0,0,0); break; // explicit synthetic zero fees
 case 6: round_trip(MES,Side::BUY,100,101,0,125,125); round_trip(MNQ,Side::BUY,100,101,0,50,50); break;
 case 12: { Fixture f; CHECK(f.fill(Side::BUY,1,1)==Error::NONE); rejected(f,Side::SELL,1,hi,1,2,Error::OVERFLOW); break; }
 case 15: {Fixture f; rejected(f,Side::BUY,2,20000,0,1,Error::CAP_EXCEEDED); break;}
 case 16: {Fixture f; CHECK(f.fill(Side::BUY,1,20000)==Error::NONE); rejected(f,Side::BUY,1,20000,1,2,Error::CAP_EXCEEDED); break;}
 case 17: {Fixture f; CHECK(f.fill(Side::BUY,1,20000)==Error::NONE); CHECK(f.fill(Side::SELL,1,20000)==Error::NONE); CHECK(f.state().position==0); break;}
 case 18: {Fixture f; CHECK(f.fill(Side::BUY,1,20000)==Error::NONE); CHECK(f.state().position==1 && f.state().gross==0 && f.state().fees==50 && f.state().net==-50);
 CHECK(f.fill(Side::SELL,2,20004)==Error::NONE); CHECK(f.state().position==-1 && f.state().gross==500 && f.state().fees==150 && f.state().net==350); CHECK(f.state().lots==std::vector<Lot>{{Side::SELL,1,20004}});
 CHECK(f.fill(Side::BUY,1,20000)==Error::NONE); CHECK(f.state().position==0 && f.state().gross==1000 && f.state().fees==200 && f.state().net==800); break;}
 case 24: {Fixture f; rejected(f,Side::BUY,1,20000,5,5,Error::SAME_OR_PAST_EVENT); CHECK(f.ledger.fill(Side::BUY,1,20000,5,6,f.stream)==Error::NONE); break;}
 case 29: {Fixture f(MES,50,3); CHECK(f.fill(Side::BUY,1,20000)==Error::NONE); CHECK(f.fill(Side::BUY,2,20004)==Error::NONE); CHECK(f.fill(Side::SELL,2,20008)==Error::NONE); CHECK(f.state().gross==1500 && f.state().fees==250 && f.state().net==1250 && f.state().position==1); CHECK(f.state().lots==std::vector<Lot>{{Side::BUY,1,20004}}); break;}
 case 30: {Fixture f; CHECK(f.fill(Side::BUY,1,1)==Error::NONE);
 for(Contracts q:{0,-1,std::numeric_limits<Contracts>::min()}) rejected(f,Side::SELL,q,hi,1,2,Error::NON_POSITIVE_INPUT);
 rejected(f,Side::BUY,1,hi,1,2,Error::CAP_EXCEEDED); rejected(f,Side::SELL,1,hi,1,99,Error::INVALID_EVENT);
 rejected(f,Side::SELL,2,hi,1,2,Error::OVERFLOW);
 rejected(f,Side::SELL,1,0,1,2,Error::NON_POSITIVE_PRICE); rejected(f,Side::SELL,1,-1,1,2,Error::NON_POSITIVE_PRICE);
 Fixture fees(MES,hi); CHECK(fees.fill(Side::BUY,1,100)==Error::NONE); rejected(fees,Side::SELL,2,100,1,2,Error::OVERFLOW);
 break;}
 default: CHECK(false);
 }
 }); }
