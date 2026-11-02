#include "fnq/strategy.hpp"
namespace fnq {
void Strategy::record(std::int64_t t,std::string r,Ticks p,Ticks d) { log_.push_back({t,stream_.events().empty()?0:stream_.events().back().seq,std::move(r),p,d,state().position}); }
Error Strategy::decision(std::int64_t t) {
 Bar b=forming_.value_or(Bar{}); b.label=t;
 for(auto h:session_.halts) if(h.begin<t && h.end>t-quarter) b.spans_halt=true;
 if(b.present) {
  auto prev=bars_.empty() || !bars_.back().present ? b.close : bars_.back().close;
  auto tr=checked_sub(std::max(b.high,prev),std::min(b.low,prev)); if(!tr) return tr.error; b.tr=*tr.value;
 }
 bars_.push_back(b); forming_.reset();
 if(flat_done_ || pending_) return Error::NONE;
 if(bars_.size()<9) { record(t,"WARMUP"); return Error::NONE; }
 auto f=features(bars_); if(!f) { if(f.error==Error::OVERFLOW) return f.error; record(t,"MISSING_DATA"); return Error::NONE; }
 auto pos=state().position;
 bool buy=b.close>f.value->high, sell=b.close<f.value->low;
 if(pos) {
  if((pos>0 && sell) || (pos<0 && buy)) pending_=Order{pos>0?Side::SELL:Side::BUY,false,t,0,stream_.events().back().seq,"OPPOSITE_SIGNAL",0};
  return Error::NONE;
 }
 std::string reason;
 if(t>session_.cutoff) reason="AFTER_ENTRY_CUTOFF";
 else if(entries_>=3) reason="MAX_ENTRIES";
 else if(t<cooldown_) reason="COOLDOWN";
 else if(stream_.events().back().ask-stream_.events().back().bid>4) reason="SPREAD_WIDE";
 else if(!buy && !sell) reason="NO_SIGNAL";
 if(!reason.empty()) { record(t,reason); return Error::NONE; }
 auto size=size_for(5000,f.value->distance,50,100,8);
 if(size.error!=Error::NONE) return size.error;
 if(!size.size) { record(t,"SIZE_ZERO",0,f.value->distance); return Error::NONE; }
 auto deadline=checked_add(t,60*second); if(!deadline) return deadline.error;
 pending_=Order{buy?Side::BUY:Side::SELL,true,t,*deadline.value,stream_.events().back().seq,"ENTRY",f.value->distance};
 risk_=size.risk_per_contract; record(t,"ENTRY_ORDER",0,f.value->distance); return Error::NONE;
}
Error Strategy::clock(std::int64_t t) {
 if(t<now_) return Error::TIME_DECREASING;
 while(!ended_) {
  auto due=std::min(next_,session_.end);
  if(!flat_done_) due=std::min(due,session_.flat);
  if(pending_ && pending_->entry) due=std::min(due,pending_->deadline);
  if(due>t) break;
  if(pending_ && pending_->entry && pending_->deadline==due) { pending_.reset(); record(due,"ENTRY_EXPIRED"); }
  if(due==session_.flat && !flat_done_) {
   flat_done_=true;
   if(pending_ && pending_->entry) { pending_.reset(); record(due,"ENTRY_CANCELLED"); }
   if(state().position && !pending_) pending_=Order{state().position>0?Side::SELL:Side::BUY,false,due,0,stream_.events().back().seq,"FLAT_BY",0};
   record(due,"FLAT_BY_TIMER");
  }
  if(due==next_) { auto err=decision(due); if(err!=Error::NONE) return err; if(due<session_.end) next_+=quarter; }
  if(due==session_.end) {
   ended_=true; pending_.reset(); stop_.reset();
   if(state().position) { failed_=true; record(due,"FLAT_BY_FAILURE"); }
  }
 }
 now_=t; return Error::NONE;
}
Error Strategy::advance_time(std::int64_t t) { auto copy=*this; auto err=copy.clock(t); if(err==Error::NONE) *this=std::move(copy); return err; }
Error Strategy::quote(Event e) {
 auto copy=*this;
 auto admitted=copy.stream_.admit(e); if(!admitted) return admitted.error;
 if(e.t_utc_ns<now_) return Error::TIME_DECREASING;
 auto mid=checked_add(e.bid,e.ask); if(!mid) return mid.error;
 // Validate with core before clocks, but decisions must see only earlier events.
 copy.stream_=stream_;
 auto err=copy.clock(e.t_utc_ns); if(err!=Error::NONE) return err;
 copy.stream_.admit(e);
 err=copy.execute(e); if(err!=Error::NONE) return err;
 if(e.t_utc_ns>=session_.open && e.t_utc_ns<session_.end) {
  auto& b=copy.forming_;
  if(!b) b=Bar{0,*mid.value,*mid.value,*mid.value,*mid.value,0,true,false};
  else { b->high=std::max(b->high,*mid.value); b->low=std::min(b->low,*mid.value); b->close=*mid.value; }
 }
 *this=std::move(copy); return Error::NONE;
}
}
