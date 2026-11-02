// Independent QA probes for MNQ15 candidate. Written from the spec/review, not copied from tests/strategy.cpp.
#include "fnq/strategy.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace fnq;
#define EXPECT(c) do{ if(!(c)){ std::cerr<<"EXPECT failed line "<<__LINE__<<": " #c "\n"; ++fails; } }while(0)
static int fails=0;
static constexpr std::int64_t S=1000000000LL, Q=900*S;
static constexpr std::int64_t OPEN_CST=1793716200LL*S; // 2026-11-03 14:30Z
static constexpr std::int64_t OPEN_CDT=1793107800LL*S; // 2026-10-27 13:30Z
static constexpr Ticks TMAX=std::numeric_limits<Ticks>::max();
struct H {
 Session ss; Strategy s; std::uint64_t seq=0;
 explicit H(std::int64_t open=OPEN_CST):ss{open,open+26*Q,open+22*Q,open+22800*S,{}},s(ss){}
 explicit H(Session x):ss(x),s(x){}
 Error q(std::int64_t t,Ticks b,Ticks a){ return s.quote({++seq,t,b,a}); }
 Error qs(std::uint64_t sq,std::int64_t t,Ticks b,Ticks a){ return s.quote({sq,t,b,a}); }
 std::int64_t L(int i)const{ return ss.open+i*Q; }
 // bar i with m2 values o,h,l,c (m2 = bid+ask) via four quotes of spread 2 ticks
 void bar(int i,Ticks o,Ticks h,Ticks l,Ticks c){ Ticks v[4]={o,h,l,c}; for(int j=0;j<4;++j){ Ticks m=v[j]; q(ss.open+(i-1)*Q+(j+1)*S,(m-2)/2,(m+2)/2);} }
 void flatbars(int n){ for(int i=1;i<=n;++i) bar(i,160000,160020,159980,160000); }
 bool has(const char* r,std::int64_t t=-1)const{ for(auto&x:s.log()) if(x.reason==r&&(t<0||x.time==t)) return true; return false; }
 int count(const char* r)const{ int n=0; for(auto&x:s.log()) if(x.reason==r) ++n; return n; }
 void sig(){ flatbars(8); bar(9,160020,160060,160020,160050); s.advance_time(L(9)); }
};
static bool throws(Session x){ try{ Strategy t(x); return false; }catch(const std::invalid_argument&){ return true; } }

static void run(int n){
 switch(n){
 case 1:{ // boundary quote must not enter closing bar; decision precedes it; it is first eligible fill
  H h; h.flatbars(8); h.bar(9,160020,160060,160020,160050);
  EXPECT(h.q(h.L(9),999990,999992)==Error::NONE);
  EXPECT(h.s.bars().size()==9 && h.s.bars()[8].close==160050 && h.s.bars()[8].high==160060);
  EXPECT(h.s.state().position==1);
  break; }
 case 2:{ // exact half-open TTL
  H a; a.sig(); EXPECT(a.q(a.L(9)+60*S-1,80025,80026)==Error::NONE && a.s.state().position==1);
  H b; b.sig(); EXPECT(b.q(b.L(9)+60*S,80025,80026)==Error::NONE && b.s.state().position==0 && b.has("ENTRY_EXPIRED") && b.s.entries()==0);
  H c; c.sig(); EXPECT(c.s.advance_time(c.L(9)+60*S-1)==Error::NONE && c.s.pending());
  EXPECT(c.s.advance_time(c.L(9)+60*S)==Error::NONE && !c.s.pending() && c.has("ENTRY_EXPIRED",c.L(9)+60*S));
  break; }
 case 3:{ // stale / equal timestamps
  H h; h.sig(); h.s.advance_time(h.L(9)+10*S);
  auto lg=h.s.log().size(), ev=h.s.events().size();
  EXPECT(h.q(h.L(9)+10*S-1,1,2)!=Error::NONE);
  EXPECT(h.s.log().size()==lg && h.s.events().size()==ev && h.s.now()==h.L(9)+10*S);
  EXPECT(h.s.advance_time(h.L(9)+10*S-1)==Error::TIME_DECREASING);
  EXPECT(h.s.advance_time(h.L(9)+10*S)==Error::NONE);
  EXPECT(h.q(h.L(9)+10*S,80025,80026)==Error::NONE);
  EXPECT(h.qs(h.seq+1,h.L(9)+10*S,80026,80027)==Error::NONE);
  EXPECT(h.qs(h.seq,h.L(9)+11*S,80026,80027)==Error::SEQ_NOT_INCREASING);
  break; }
 case 4:{ // two quotes at the boundary: one decision, neither in closing bar
  H h; h.flatbars(8); h.bar(9,160020,160060,160020,160050);
  h.q(h.L(9),80025,80026); auto k=h.count("ENTRY_ORDER"); h.q(h.L(9),70000,70001);
  EXPECT(k==1 && h.count("ENTRY_ORDER")==1 && h.s.bars().size()==9 && h.s.bars()[8].low==160020);
  break; }
 case 5:{ // stop arming and gap fill
  H h; h.sig(); h.q(h.L(9)+S/10,80025,80026);
  EXPECT(h.s.state().position==1 && h.s.stop() && *h.s.stop()==80026-43);
  h.q(h.L(9)+S/5,79970,79972);
  EXPECT(h.s.state().position==0 && h.has("STOP"));
  bool ok=false; for(auto&r:h.s.log()) if(r.reason=="STOP"&&r.price==79970) ok=true; EXPECT(ok);
  H g; g.sig(); g.q(g.L(9)+S/10,79000,80026); // entry event's own bid is below future stop
  EXPECT(g.s.state().position==1 && !g.has("STOP"));
  break; }
 case 6:{ // exact session end with pending opposite exit
  H h; h.sig(); h.q(h.L(9)+S/10,80025,80026);
  h.bar(10,160000,160000,159970,159970); h.s.advance_time(h.L(10));
  EXPECT(h.s.pending() && h.s.pending()->reason=="OPPOSITE_SIGNAL");
  EXPECT(h.s.advance_time(h.ss.flat)==Error::NONE && h.s.pending() && h.s.pending()->reason=="OPPOSITE_SIGNAL");
  EXPECT(h.q(h.ss.end,79000,79002)==Error::NONE);
  EXPECT(h.s.failed() && h.s.state().position==1 && !h.s.pending() && !h.s.stop() && h.has("FLAT_BY_FAILURE"));
  EXPECT(h.q(h.ss.end+Q,1000,1002)==Error::NONE && h.s.state().position==1);
  break; }
 case 7:{ // flat-by at first quote >= 14:50; 1ns before end still fills
  H h; h.sig(); h.q(h.L(9)+S/10,80025,80026);
  EXPECT(h.q(h.ss.flat-1,80100,80101)==Error::NONE && h.s.state().position==1);
  EXPECT(h.q(h.ss.end-1,80100,80101)==Error::NONE && h.s.state().position==0 && h.has("FLAT_BY") && !h.s.failed());
  break; }
 case 8:{ // offset / boundary validation
  Session ok{OPEN_CST,OPEN_CST+26*Q,OPEN_CST+22*Q,OPEN_CST+22800*S,{}};
  EXPECT(!throws(ok));
  Session w=ok; w.open-=3600*S; w.end-=3600*S; w.cutoff-=3600*S; w.flat-=3600*S; EXPECT(throws(w));
  Session mis=ok; mis.open+=S; mis.end+=S; EXPECT(throws(mis));
  Session wide=ok; wide.end+=Q; EXPECT(throws(wide));
  Session neg=ok; neg.cutoff=neg.flat; EXPECT(throws(neg));
  Session late=ok; late.flat=ok.end+1; EXPECT(throws(late));
  Session hz=ok; hz.halts={{ok.open-1,ok.open+5}}; EXPECT(throws(hz));
  Session hz2=ok; hz2.halts={{ok.open+5,ok.open+5}}; EXPECT(throws(hz2));
  EXPECT(!session_2026("2026-11-02") && !session_2026("2026-10-28") && !session_2026(""));
  auto a=session_2026("2026-10-27"), b=session_2026("2026-11-03");
  EXPECT(a && b && a.value->open==OPEN_CDT && b.value->open==OPEN_CST && b.value->open-a.value->open==608400*S);
  break; }
 case 9:{ // rollback on overflow inside clock
  H z; for(int i=1;i<=9;++i) z.bar(i,4,TMAX-5,4,4);
  std::size_t bars=z.s.bars().size(), lg=z.s.log().size(); std::int64_t n2=z.s.now();
  EXPECT(z.s.advance_time(z.L(9))==Error::OVERFLOW && z.s.bars().size()==bars && z.s.now()==n2 && z.s.log().size()==lg);
  EXPECT(z.s.advance_time(z.L(9))==Error::OVERFLOW); // wedged (observation, see report)
  // bid+ask overflow quote rejected without effect
  H y; y.sig(); auto ev=y.s.events().size();
  EXPECT(y.q(y.L(9)+S,TMAX-1,TMAX)!=Error::NONE && y.s.events().size()==ev && y.s.pending());
  break; }
 case 10:{ // cooldown: exit exactly at boundary belongs to next bar
  H h; h.sig(); h.q(h.L(9)+S/10,80025,80026);
  h.bar(10,160050,160060,160020,160050);
  h.q(h.L(10),79900,79901); EXPECT(h.s.state().position==0);
  for(int i=11;i<=13;++i){ h.bar(i,160000,160300+i,160000,160290+i); h.s.advance_time(h.L(i)); }
  EXPECT(h.has("COOLDOWN",h.L(11)) && h.has("COOLDOWN",h.L(12)) && !h.has("COOLDOWN",h.L(13)));
  break; }
 case 11:{ // missing bar 1: decision 9 MISSING, decision 10 works (window 2..10)
  H h; for(int i=2;i<=8;++i) h.bar(i,160000,160020,159980,160000);
  h.bar(9,160020,160060,160020,160050); h.s.advance_time(h.L(9));
  EXPECT(h.has("MISSING_DATA",h.L(9)) && !h.s.pending());
  h.bar(10,160050,160090,160050,160080); h.s.advance_time(h.L(10));
  EXPECT(h.s.pending() && h.s.pending()->entry);
  break; }
 case 12:{ // bar 2 missing blocks decisions 2..10 windows, recovers at 11
  H h; h.bar(1,160000,160020,159980,160000); for(int i=3;i<=9;++i) h.bar(i,160000,160020,159980,160000);
  h.bar(10,160000,160100,160000,160090); h.s.advance_time(h.L(10)); EXPECT(h.has("MISSING_DATA",h.L(10)));
  h.bar(11,160090,160200,160090,160190); h.s.advance_time(h.L(11));
  EXPECT(h.s.pending()&&h.s.pending()->entry);
  break; }
 case 13:{ // no quotes at all across session: must not crash (sanitizer)
  H h; EXPECT(h.s.advance_time(h.ss.end)==Error::NONE); EXPECT(h.s.bars().size()==26);
  EXPECT(h.has("WARMUP")&&h.has("MISSING_DATA")); break; }
 case 14:{ // OCO: pending exit + triggered stop on same quote
  H h; h.sig(); h.q(h.L(9)+S/10,80025,80026);
  h.bar(10,160000,160000,159970,159970); h.s.advance_time(h.L(10));
  h.q(h.L(10)+S,79900,79901);
  EXPECT(h.s.state().position==0 && h.has("OPPOSITE_SIGNAL") && !h.has("STOP")); break; }
 case 15:{ // short symmetry; buy stop triggers at ask>=stop
  H h; h.flatbars(8); h.bar(9,159980,159980,159940,159950); h.s.advance_time(h.L(9));
  h.q(h.L(9)+S/10,79975,79976); EXPECT(h.s.state().position==-1 && h.s.stop() && *h.s.stop()==79975+43);
  h.q(h.L(9)+S/5,80016,80017); EXPECT(h.s.state().position==-1); // ask 80017 < 80018
  h.q(h.L(9)+S/4,80017,80018); EXPECT(h.s.state().position==0 && h.has("STOP")); break; }
 case 16:{ // flat-by cancels entry; position without quotes gets FLAT_BY pending; no bar decisions afterwards
  auto sx=*session_2026("2026-11-03").value; sx.flat=sx.open+9*Q+30*S; sx.cutoff=sx.open+9*Q;
  H h(sx); h.sig(); EXPECT(h.s.advance_time(sx.flat)==Error::NONE && !h.s.pending() && h.has("ENTRY_CANCELLED"));
  H g; g.sig(); g.q(g.L(9)+S/10,80025,80026); g.s.advance_time(g.ss.flat);
  EXPECT(g.s.pending() && g.s.pending()->reason=="FLAT_BY");
  auto c=g.s.log().size(); g.s.advance_time(g.ss.end-S); EXPECT(g.s.log().size()==c); break; }
 case 17:{ // opposite signal skipped when window has a gap
  H h; h.sig(); h.q(h.L(9)+S/10,80025,80026);
  // bar 10 holds the entry-fill quote; bar 11 is empty; bar 12 closes below the channel
  h.s.advance_time(h.L(11));
  h.bar(12,160000,160000,159970,159970); h.s.advance_time(h.L(12));
  EXPECT(h.s.state().position==1 && !h.s.pending() && h.has("MISSING_DATA",h.L(12))); break; }
 case 18:{ // MAX_ENTRIES: never more than 3 fills across a session
  H h; h.flatbars(8);
  for(int i=9;i<=22;++i){ Ticks c=160000+(i-8)*60; h.bar(i,c,c+10,c-10,c); h.s.advance_time(h.L(i));
   if(h.s.pending()&&h.s.pending()->entry){ h.q(h.L(i)+S/2,c/2-1,c/2+1); if(h.s.stop()) h.q(h.L(i)+S*2/3,*h.s.stop()-1,*h.s.stop()); } }
  std::cout<<"  entries="<<h.s.entries()<<" max_entries_logged="<<h.count("MAX_ENTRIES")<<"\n";
  EXPECT(h.s.entries()==3 && h.has("MAX_ENTRIES")); break; }
 case 19:{ // spread judged on last event strictly before the label
  H a; a.flatbars(8); a.bar(9,160020,160060,160020,160050); a.q(a.L(9)-S/2,80023,80028); a.s.advance_time(a.L(9)); EXPECT(a.has("SPREAD_WIDE"));
  H b; b.flatbars(8); b.bar(9,160020,160060,160020,160050); b.q(b.L(9)-S/2,80023,80027); b.q(b.L(9),80000,80100);
  EXPECT(b.count("SPREAD_WIDE")==0 && b.s.state().position==1); break; }
 case 20:{ // sizing boundary through real quotes: D=88 sizes, D=89 SIZE_ZERO (TR(m2)=d on every bar)
  for(int d:{88,89}){ H h; Ticks base=160000;
   for(int i=1;i<=8;++i) h.bar(i,base,base+d,base,base);
   h.bar(9,base,base+d+10,base,base+d+10); h.s.advance_time(h.L(9));
   // TR9=max(high9,close8)-min(low9,close8)=d+10 ; sum=7d+d+10 -> D=ceil((8d+10)/8)=d+2 ; adjust: use target below
   std::cout<<"  d="<<d<<" pending="<<h.s.pending().has_value()<<" sizezero="<<h.has("SIZE_ZERO")<<" dist="<<(h.s.pending()?h.s.pending()->distance:-1)<<"\n"; }
  break; }
 case 21:{ // CDT date behaves identically
  H h(OPEN_CDT); h.sig(); h.q(h.L(9)+S/10,80025,80026); EXPECT(h.s.state().position==1);
  h.q(h.ss.flat,80100,80101); h.s.advance_time(h.ss.end); EXPECT(h.s.bars().size()==26 && h.s.state().position==0 && !h.s.failed()); break; }
 case 22:{ // huge distance: sizing overflow wedges strategy (informational)
  H h; for(int i=1;i<=8;++i) h.bar(i,10,TMAX/16,10,10);
  h.bar(9,10,10,10,TMAX/16);
  auto e=h.s.advance_time(h.L(9)); std::cout<<"  advance_time(L9) -> "<<(int)e<<"\n";
  auto f=h.q(h.L(9)+S,100,102); std::cout<<"  later valid quote -> "<<(int)f<<"\n"; break; }
 case 23:{ // non-positive stop (informational)
  H h; for(int i=1;i<=8;++i) h.bar(i,20,24,16,20);
  h.bar(9,40,44,40,44); h.s.advance_time(h.L(9));
  if(h.s.pending()){ h.q(h.L(9)+S/10,21,22); if(h.s.stop()) std::cout<<"  stop="<<*h.s.stop()<<" (long fill 22)\n"; }
  break; }
 case 24:{ // pre-open quote excluded from bar 1
  H h; h.q(h.ss.open-S,1000,1002); h.flatbars(8); h.bar(9,160020,160060,160020,160050);
  h.s.advance_time(h.L(9)); EXPECT(h.s.bars()[0].open==160000 && h.s.state().position==0);
  break; }
 }
}
int main(int c,char**v){ if(c!=2) return 2; try{ run(std::atoi(v[1])); }catch(const std::exception&e){ std::cerr<<"EXC "<<e.what()<<"\n"; return 1; } return fails?1:0; }
