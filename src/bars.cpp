#include "fnq/strategy.hpp"
#include <stdexcept>
namespace fnq {
Result<Session> session_2026(std::string_view date) {
 std::int64_t open;
 if(date=="2026-10-27") open=1793107800LL*second;
 else if(date=="2026-11-03") open=1793716200LL*second;
 else return Result<Session>::fail(Error::INVALID_EVENT);
 return Result<Session>::ok({open,open+26*quarter,open+22*quarter,open+22800*second,{}});
}
Strategy::Strategy(Session s, CalendarScope scope):session_(std::move(s)),now_(0),next_(session_.open) {
 auto end=checked_add(session_.open,26*quarter);
 if(session_.open<0 || session_.open%quarter || !end || session_.end!=*end.value || session_.cutoff<session_.open || session_.cutoff>=session_.flat || session_.flat>session_.end) throw std::invalid_argument("invalid synthetic session boundaries");
 if(scope==CalendarScope::PINNED_FIXTURE && (session_.open!=session_2026("2026-10-27").value->open && session_.open!=session_2026("2026-11-03").value->open)) throw std::invalid_argument("unsupported date or CT offset");
 next_+=quarter;
 for(auto h:session_.halts) if(h.begin<h.end && h.begin>=session_.open && h.end<=session_.end) {} else throw std::invalid_argument("invalid halt");
}
}
