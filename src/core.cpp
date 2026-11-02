#include "fnq/core.hpp"
#include <charconv>
namespace fnq {
namespace {
Result<std::int64_t> integer(std::string_view s) {
 std::int64_t v{};
 auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),v);
 if(e==std::errc::result_out_of_range) return Result<std::int64_t>::fail(Error::OVERFLOW);
 if(e!=std::errc{} || p!=s.data()+s.size()) return Result<std::int64_t>::fail(Error::PARSE);
 return Result<std::int64_t>::ok(v);
}
}
Result<Ticks> parse_price(std::string_view s) {
 bool negative=false;
 if(!s.empty() && s.front()=='-') { negative=true; s.remove_prefix(1); }
 const auto dot=s.find('.');
 auto whole=s.substr(0,dot);
 if(whole.empty() || !std::all_of(whole.begin(),whole.end(),[](char c){return c>='0' && c<='9';})) return Result<Ticks>::fail(Error::PARSE);
 auto fraction=dot==s.npos ? std::string_view{} : s.substr(dot+1);
 if((dot!=s.npos && fraction.empty()) || !std::all_of(fraction.begin(),fraction.end(),[](char c){return c>='0' && c<='9';})) return Result<Ticks>::fail(Error::PARSE);
 while(!fraction.empty() && fraction.back()=='0') fraction.remove_suffix(1);
 std::int64_t quarter=0;
 if(!fraction.empty()) {
 if(fraction=="25") quarter=1; else if(fraction=="5") quarter=2; else if(fraction=="75") quarter=3; else return Result<Ticks>::fail(Error::OFF_GRID);
 }
 // Accumulate negatively to support the entire signed tick range.
 std::int64_t v=0;
 for(char c:whole) { auto m=checked_mul<std::int64_t>(v,10); if(!m) return m; auto a=checked_sub<std::int64_t>(*m.value,c-'0'); if(!a) return a; v=*a.value; }
 auto m=checked_mul<std::int64_t>(v,4); if(!m) return m;
 auto a=checked_sub<std::int64_t>(*m.value,quarter); if(!a || negative) return a;
 return checked_sub<std::int64_t>(0,*a.value);
}
Result<Contracts> parse_quantity(std::string_view s) {
 if(s.find('.')!=s.npos) return Result<Contracts>::fail(Error::FRACTIONAL_QTY);
 auto v=integer(s); if(!v) return Result<Contracts>::fail(v.error);
 if(*v.value<=0) return Result<Contracts>::fail(Error::NON_POSITIVE_INPUT);
 if(*v.value>std::numeric_limits<Contracts>::max()) return Result<Contracts>::fail(Error::OVERFLOW);
 return Result<Contracts>::ok(static_cast<Contracts>(*v.value));
}
Sizing size_for(Cents budget,Ticks stop,Cents tick,Cents fee,Ticks slip,Contracts cap) {
 if(budget<=0 || stop<=0 || tick<=0 || cap<=0) return {0,0,Error::NON_POSITIVE_INPUT};
 if(fee<0 || slip<0) return {0,0,Error::INVALID_COST};
 auto a=checked_mul(stop,tick), b=checked_mul<Cents>(fee,2), c=checked_mul(slip,tick);
 if(!a || !b || !c) return {0,0,Error::OVERFLOW};
 auto d=checked_add(*a.value,*b.value); if(!d) return {0,0,d.error};
 auto e=checked_add(*d.value,*c.value); if(!e) return {0,0,e.error};
 return {static_cast<Contracts>(std::min<std::int64_t>(cap,budget/ *e.value)),*e.value,Error::NONE};
}
EventResult EventStream::admit(Event e) {
 if(!events_.empty() && e.seq<=events_.back().seq) return {e.seq,Error::SEQ_NOT_INCREASING};
 if(!events_.empty() && e.t_utc_ns<events_.back().t_utc_ns) return {e.seq,Error::TIME_DECREASING};
 if(e.bid<=0 || e.ask<=0) return {e.seq,Error::NON_POSITIVE_PRICE};
 if(e.bid>e.ask) return {e.seq,Error::CROSSED_QUOTE};
 events_.push_back(e); return {e.seq,Error::NONE};
}
bool EventStream::contains(std::uint64_t seq) const { return std::any_of(events_.begin(),events_.end(),[seq](const Event& e){return e.seq==seq;}); }
Error Ledger::fill(Side side,Contracts qty,Ticks price,std::uint64_t decision,std::uint64_t seq,const EventStream& stream) {
 if(side!=Side::BUY && side!=Side::SELL) return Error::INVALID_SIDE;
 if(qty<=0 || cap_<=0 || instrument_.tick_value_cents<=0 || instrument_.ticks_per_point!=4) return Error::NON_POSITIVE_INPUT;
 if(price<=0) return Error::NON_POSITIVE_PRICE;
 if(fee_<0) return Error::INVALID_COST;
 const std::int64_t delta=side==Side::BUY ? static_cast<std::int64_t>(qty) : -static_cast<std::int64_t>(qty);
 const auto position=static_cast<std::int64_t>(state_.position)+delta;
 if(position>cap_ || position< -static_cast<std::int64_t>(cap_)) return Error::CAP_EXCEEDED;
 if(!stream.contains(seq)) return Error::INVALID_EVENT;
 if(seq<=decision) return Error::SAME_OR_PAST_EVENT;
 if(state_.last_fill_seq && seq<*state_.last_fill_seq) return Error::FILL_EVENT_REGRESSION;
 State next=state_;
 Contracts remaining=qty;
 while(remaining>0 && !next.lots.empty() && next.lots.front().side!=side) {
 auto& lot=next.lots.front();
 const auto closed=std::min(remaining,lot.quantity);
 auto difference=checked_sub(price,lot.entry_ticks); if(!difference) return difference.error;
 auto signed_move=checked_mul<Ticks>(*difference.value,lot.side==Side::BUY ? 1 : -1); if(!signed_move) return signed_move.error;
 auto amount=checked_mul<Ticks>(*signed_move.value,closed); if(!amount) return amount.error;
 auto cash=checked_mul(*amount.value,instrument_.tick_value_cents); if(!cash) return cash.error;
 auto gross=checked_add(next.gross,*cash.value); if(!gross) return gross.error;
 next.gross=*gross.value;
 remaining-=closed; lot.quantity-=closed;
 if(lot.quantity==0) next.lots.erase(next.lots.begin());
 }
 if(remaining>0) next.lots.push_back({side,remaining,price});
 auto fee=checked_mul<Cents>(fee_,qty); if(!fee) return fee.error;
 auto fees=checked_add(next.fees,*fee.value); if(!fees) return fees.error;
 auto net=checked_sub(next.gross,*fees.value); if(!net) return net.error;
 next.position=static_cast<Contracts>(position); next.fees=*fees.value; next.net=*net.value; next.last_fill_seq=seq;
 state_=std::move(next); return Error::NONE;
}
}
