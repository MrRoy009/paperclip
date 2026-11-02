#include "check.hpp"
#include "fnq/strategy.hpp"
#include <array>
using Quad=std::array<Ticks,4>;
struct Fixture {
 Session session=*session_2026("2026-11-03").value;
 Strategy s;
 std::uint64_t seq=0;
 Fixture():s(session) {}
 explicit Fixture(Session v):session(v),s(v) {}
 void q(std::int64_t t,Ticks bid,Ticks ask) { CHECK(s.quote({++seq,t,bid,ask})==Error::NONE); }
 void mid(std::int64_t t,Ticks m,int spread=2) { // use parity-compatible quotes
  if((m-spread)%2) spread=1;
  q(t,(m-spread)/2,(m+spread)/2);
 }
 void bar(int i,Quad b) { auto start=session.open+(i-1)*quarter; for(int j=0;j<4;++j) mid(start+(j+1)*second,b[j]); }
 void base(int end=8,Quad b={160000,160020,159980,160000}) { for(int i=1;i<=end;++i) bar(i,b); }
 void signal(bool short_side=false) { base(); bar(9,short_side?Quad{159980,159980,159940,159950}:Quad{160020,160060,160020,160050}); CHECK(s.advance_time(label(9))==Error::NONE); }
 std::int64_t label(int i) const { return session.open+i*quarter; }
 void enter(bool sh=false) { signal(sh); q(label(9)+second/10,sh?79974:80025,sh?79975:80026); }
 bool reason(std::string_view r,std::int64_t t=-1) const { for(auto& x:s.log()) if(x.reason==r && (t<0 || x.time==t)) return true; return false; }
};
void check_case(int n) {
 Fixture f;
 switch(n) {
 case 1: case 14: f.enter(); CHECK(f.s.stop()==79983); f.q(f.label(9)+second,79983,79984); CHECK(f.s.state().net==-2350); CHECK(f.reason("STOP")); break;
 case 2: f.enter(true); CHECK(f.s.stop()==80017); f.q(f.session.flat+second/5,79960,79961); CHECK(f.s.state().net==450); CHECK(f.reason("FLAT_BY")); break;
 case 3: case 4: f.base(); f.bar(9,n==3?Quad{160000,160020,159990,160020}:Quad{160000,160010,159980,159980}); CHECK(f.s.advance_time(f.label(9))==Error::NONE); CHECK(f.reason("NO_SIGNAL")); CHECK(!f.s.pending()); break;
 case 5: f.base(7); f.bar(8,{160020,160060,160020,160050}); CHECK(f.s.advance_time(f.label(8))==Error::NONE); CHECK(f.reason("WARMUP",f.label(8))); CHECK(!f.s.pending()); break;
 case 6: case 29: {
  if(n==29) { auto session=f.session; session.halts={{f.session.open+4*quarter+second,f.session.open+4*quarter+2*second}}; f=Fixture(session); }
  for(int i=1;i<=13;++i) { if(n==29 || i!=4) f.bar(i,{160000,160020,159980,i==9 || i==12 || i==13?160050:160000}); CHECK(f.s.advance_time(f.label(i))==Error::NONE); }
  CHECK(f.reason("MISSING_DATA",f.label(9)));
  if(n==6) { CHECK(f.reason("MISSING_DATA",f.label(12))); CHECK(!f.reason("MISSING_DATA",f.label(13))); }
  else CHECK(f.s.bars()[4].spans_halt);
  break;
 }
 case 7: case 8: case 9: {
  if(n==9) { f.base(8,{160000,160002,159998,160000}); f.bar(9,{160002,160006,160000,160004}); }
  else { f.base(8,{160000,160044,n==7?159955:159956,160000}); f.bar(9,{160044,n==7?160089:160088,160000,160080}); }
  CHECK(f.s.advance_time(f.label(9))==Error::NONE);
  if(n==7) CHECK(f.reason("SIZE_ZERO")); else { CHECK(f.s.pending()); CHECK(f.s.pending()->distance==(n==8?88:8)); }
  break;
 }
 case 10: {
  for(int spread:{5,4}) { Fixture x; x.base(); x.bar(9,{160020,160060,160020,160050}); x.q(x.label(9)-second,80023,80023+spread); CHECK(x.s.advance_time(x.label(9))==Error::NONE); CHECK(x.reason("SPREAD_WIDE")== (spread==5)); CHECK(x.s.pending().has_value()==(spread==4)); } break;
 }
 case 11: {
  for(int last:{22,23}) { Fixture x; x.base(last-1); x.bar(last,{160020,160060,160020,160050}); CHECK(x.s.advance_time(x.label(last))==Error::NONE); CHECK(x.s.pending().has_value()==(last==22)); CHECK(x.reason("AFTER_ENTRY_CUTOFF")== (last==23)); } break;
 }
 case 12: {
  f.signal(); CHECK(f.s.pending()); f.q(f.label(9)+60*second,80025,80026); CHECK(f.reason("ENTRY_EXPIRED")); CHECK(f.s.entries()==0 && f.s.state().fees==0 && !f.s.pending());
  Fixture x; x.signal(); CHECK(x.s.advance_time(x.label(9)+60*second)==Error::NONE); CHECK(x.reason("ENTRY_EXPIRED"));
  Fixture y; y.signal(); y.q(y.label(9)+60*second-1,80025,80026); CHECK(y.s.entries()==1); break;
 }
 case 13: f.base(); f.bar(9,{160020,160060,160020,160050}); f.q(f.label(9),80025,80026); CHECK(f.s.state().position==1); CHECK(f.s.bars().back().close==160050); f.q(f.label(9),80026,80027); CHECK(f.s.entries()==1); break;
 case 15: f.enter(); f.q(f.label(9)+second,79970,79972); CHECK(f.s.state().net==-3000); CHECK(f.reason("RESERVE_EXCEEDED")); break;
 case 16: f.enter(true); f.q(f.label(9)+second,80020,80022); CHECK(f.s.state().net==-2600); CHECK(!f.reason("RESERVE_EXCEEDED")); break;
 case 17: f.signal(); f.q(f.label(9)+second/10,79980,80026); CHECK(f.s.state().position==1); f.q(f.label(9)+second,80020,80022); CHECK(f.s.state().position==1); break;
 case 18: case 23: {
  f.enter(); f.bar(10,{160000,160000,159970,159970}); CHECK(f.s.advance_time(f.label(10))==Error::NONE); CHECK(f.s.pending() && f.s.pending()->reason=="OPPOSITE_SIGNAL");
  f.q(f.label(10)+second,n==23?79980:79985,n==23?79982:79986); CHECK(f.s.state().position==0 && !f.s.stop() && !f.s.pending()); CHECK(f.reason("OPPOSITE_SIGNAL")); CHECK(!f.reason("STOP")); break;
 }
 case 19: {
  for(bool exact:{false,true}) {
   Fixture x; x.enter(); x.bar(10,{160050,160060,160020,160050});
   if(exact) x.mid(x.label(10)+second,160050);
   x.q(x.label(10)+(exact?quarter:7*60*second),79983,79984);
   int first=exact?12:11;
   for(int i=first;i<=first+2;++i) { if(i==first) { x.mid(x.label(i)-second,160180+i*2); } else x.bar(i,{160000,160200+i*2,160000,160180+i*2}); CHECK(x.s.advance_time(x.label(i))==Error::NONE); }
   CHECK(x.reason("COOLDOWN",x.label(first))); CHECK(x.reason("COOLDOWN",x.label(first+1))); CHECK(!x.reason("COOLDOWN",x.label(first+2)));
  } break;
 }
 case 20: f.enter(); f.q(f.session.flat-second/10,80100,80101); f.q(f.session.flat+second/4,80100,80101); CHECK(f.reason("FLAT_BY",f.session.flat+second/4)); CHECK(f.s.state().position==0); break;
 case 21: {
  f.enter(); f.q(f.session.end,80100,80101); CHECK(f.s.failed() && f.s.state().position==1); CHECK(f.reason("FLAT_BY_FAILURE")); CHECK(!f.s.stop() && !f.s.pending()); auto net=f.s.state().net; f.q(f.session.end+quarter,81000,81001); CHECK(f.s.state().net==net);
  Fixture x; x.enter(); CHECK(x.s.advance_time(x.session.end)==Error::NONE); CHECK(x.s.failed()); break;
 }
 case 22: {
  // Rising disjoint ranges yield entries at 9,12,15; each is stopped in its entry bar.
  f.base();
  for(int i=9;i<=18;++i) {
   Ticks c=160000+(i-8)*40;
   f.bar(i,{c,c+10,c-10,c}); CHECK(f.s.advance_time(f.label(i))==Error::NONE);
   if(f.s.pending() && f.s.pending()->entry) { f.mid(f.label(i)+second/10,c); auto stop=*f.s.stop(); f.q(f.label(i)+second/5,stop,stop+1); }
  }
  CHECK(f.s.entries()==3); CHECK(f.reason("MAX_ENTRIES")); break;
 }
 case 24: {
  auto summer=*session_2026("2026-10-27").value; Fixture x(summer); x.enter(); f.enter(); CHECK(f.label(9)-x.label(9)==608400*second); CHECK(f.s.stop()==x.s.stop()); CHECK(f.s.advance_time(f.session.end)==Error::NONE); CHECK(x.s.advance_time(x.session.end)==Error::NONE); CHECK(f.s.bars().size()==26 && x.s.bars().size()==26); CHECK(!session_2026("2026-11-04")); break;
 }
 case 25: f.enter(); f.bar(12,{160050,160050,160040,160050}); CHECK(f.s.advance_time(f.label(12))==Error::NONE); CHECK(f.reason("MISSING_DATA")); CHECK(!f.s.pending()); f.q(f.session.flat,80000,80001); CHECK(f.reason("FLAT_BY")); break;
 case 26: {
  Fixture x; x.q(x.session.open-second,1000,1001); x.bar(1,{160000,160020,159980,160000}); CHECK(x.s.advance_time(x.label(1))==Error::NONE); CHECK(x.s.bars()[0].tr==40); CHECK(!x.s.bars()[0].spans_halt); break;
 }
 case 27: f.enter(true); f.bar(10,{160000,160020,159980,160020}); CHECK(f.s.advance_time(f.label(10))==Error::NONE); CHECK(f.s.state().position==-1 && !f.s.pending()); break;
 case 28: f.q(f.session.open-1,1000,1001); f.bar(1,{160000,160020,159980,160000}); CHECK(f.s.advance_time(f.session.end)==Error::NONE); f.q(f.session.end,90000,90001); CHECK(f.s.bars()[0].open==160000); CHECK(!f.s.bars()[25].present); break;
 case 30: { EventStream stream; CHECK(stream.admit({1,0,1,2})); Ledger ledger(MNQ,100); CHECK(ledger.fill(Side::BUY,1,2,1,1,stream)==Error::SAME_OR_PAST_EVENT); f.signal(); auto count=f.s.log().size(); CHECK(f.s.quote({f.seq,f.label(9),80025,80026})==Error::SEQ_NOT_INCREASING); CHECK(f.s.log().size()==count && f.s.pending()); break; }
 case 31: {
  f.signal(); auto before=f.s.now(); auto logs=f.s.log().size(); auto count=f.s.events().size();
  for(Event e:std::vector<Event>{{f.seq,f.session.end,1,2},{f.seq+1,f.session.end,0,2},{f.seq+1,f.session.end,3,2},{f.seq+1,f.session.end,hi-1,hi},{f.seq+1,before-1,1,2}}) { CHECK(f.s.quote(e)!=Error::NONE); CHECK(f.s.now()==before && f.s.log().size()==logs && f.s.events().size()==count && f.s.pending()); }
  CHECK(f.s.advance_time(before-1)==Error::TIME_DECREASING);
  // No-quote timer preserves an earlier opposite exit and its original sequence.
  Fixture x; x.enter(); x.bar(10,{160000,160000,159970,159970}); CHECK(x.s.advance_time(x.label(10))==Error::NONE); auto original=x.s.pending()->seq; CHECK(x.s.advance_time(x.session.flat)==Error::NONE); CHECK(x.s.pending()->reason=="OPPOSITE_SIGNAL" && x.s.pending()->seq==original); x.q(x.session.flat,79980,79982); CHECK(x.reason("OPPOSITE_SIGNAL"));
  // A failed ledger calculation must roll back the timer and OCO state too.
  Fixture pnl; pnl.enter(); auto net=pnl.s.state().net; auto clock=pnl.s.now();
  CHECK(pnl.s.quote({pnl.seq+1,pnl.session.flat,hi/2-1,hi/2})==Error::OVERFLOW);
  CHECK(pnl.s.state().net==net && pnl.s.state().position==1 && pnl.s.now()==clock && pnl.s.stop() && !pnl.s.pending());
  auto early=f.session; early.flat=f.label(9)+30*second; early.cutoff=f.label(9);
  Fixture cancel(early); cancel.signal(); CHECK(cancel.s.advance_time(early.flat)==Error::NONE); CHECK(!cancel.s.pending() && cancel.reason("ENTRY_CANCELLED"));
  auto aligned=f.session; aligned.flat=f.label(10); aligned.cutoff=f.label(9);
  Fixture timer(aligned); timer.enter(); timer.bar(10,{160000,160000,159970,159970}); timer.q(aligned.flat,79980,79982); CHECK(timer.reason("FLAT_BY") && !timer.reason("OPPOSITE_SIGNAL") && timer.s.state().position==0);
  // Bounded session arithmetic rejects overflow; feature sum rejects overflow.
  bool rejected=false; try { Strategy bad({hi-hi%quarter,hi,hi-2,hi-1,{}}); } catch(const std::invalid_argument&) { rejected=true; } CHECK(rejected);
  Fixture z; for(int i=1;i<=9;++i) { z.bar(i,{4,hi-5,4,4}); if(i<9) CHECK(z.s.advance_time(z.label(i))==Error::NONE); } auto t=z.s.now(); CHECK(z.s.advance_time(z.label(9))==Error::OVERFLOW); CHECK(z.s.now()==t && z.s.bars().size()==8);
  break;
 }
 default: CHECK(false);
 }
}
int main(int argc,char** argv) { return run([&]{ CHECK(argc==2); check_case(test_number(argv)); }); }
