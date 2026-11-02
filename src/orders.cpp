#include "fnq/strategy.hpp"
namespace fnq {
Error Strategy::execute(const Event& e) {
 if(ended_) return Error::NONE;
 std::optional<Order> fill;
 if(pending_ && e.seq>pending_->seq && e.t_utc_ns>=pending_->time) fill=pending_;
 else if(state().position && stop_ && e.seq>armed_ && ((state().position>0 && e.bid<=*stop_) || (state().position<0 && e.ask>=*stop_))) fill=Order{state().position>0?Side::SELL:Side::BUY,false,e.t_utc_ns,0,armed_,"STOP",0};
 if(!fill) return Error::NONE;
 auto price=fill->side==Side::BUY?e.ask:e.bid;
 if(fill->entry) {
  auto stop=fill->side==Side::BUY?checked_sub(price,fill->distance):checked_add(price,fill->distance);
  if(!stop) return stop.error;
  stop_=*stop.value; armed_=e.seq; distance_=fill->distance; entry_net_=state().net;
 }
 auto err=ledger_.fill(fill->side,1,price,fill->seq,e.seq,stream_); if(err!=Error::NONE) return err;
 pending_.reset(); record(e.t_utc_ns,fill->reason,price,distance_);
 if(fill->entry) ++entries_;
 else {
  stop_.reset();
  auto boundary=checked_add(session_.open,((e.t_utc_ns-session_.open)/quarter+3)*quarter);
  if(!boundary) return boundary.error;
  cooldown_=*boundary.value;
  auto change=checked_sub(state().net,entry_net_); if(!change) return change.error;
  if(*change.value< -risk_) record(e.t_utc_ns,"RESERVE_EXCEEDED");
 }
 return Error::NONE;
}
}
