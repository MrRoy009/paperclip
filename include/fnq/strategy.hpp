#pragma once
#include "fnq/core.hpp"
#include <string>
namespace fnq {
inline constexpr std::int64_t second=1000000000, quarter=900*second;
struct Halt { std::int64_t begin,end; };
struct Session { std::int64_t open,end,cutoff,flat; std::vector<Halt> halts; };
// Bounded fixture: only the two reviewed Tuesdays; no inferred holiday calendar.
Result<Session> session_2026(std::string_view date);
struct Bar { std::int64_t label{},open{},high{},low{},close{},tr{}; bool present{},spans_halt{}; };
struct Features { Ticks high,low,distance; };
Result<Features> features(const std::vector<Bar>&);
enum class CalendarScope { PINNED_FIXTURE, EXPLICIT_UTC };
struct Record { std::int64_t time; std::uint64_t seq; std::string reason; Ticks price{},distance{}; Contracts position{}; };
struct Order { Side side; bool entry; std::int64_t time,deadline; std::uint64_t seq; std::string reason; Ticks distance; };
class Strategy {
 Session session_;
 EventStream stream_;
 Ledger ledger_{MNQ,100,1};
 std::int64_t now_,next_,cooldown_{};
 std::optional<Bar> forming_;
 std::vector<Bar> bars_;
 std::optional<Order> pending_;
 std::optional<Ticks> stop_;
 std::uint64_t armed_{};
 Ticks distance_{};
 Cents entry_net_{},risk_{};
 int entries_{};
 bool flat_done_{},ended_{},failed_{};
 std::vector<Record> log_;
 void record(std::int64_t,std::string,Ticks=0,Ticks=0);
 Error clock(std::int64_t);
 Error decision(std::int64_t);
 Error execute(const Event&);
public:
 explicit Strategy(Session, CalendarScope scope=CalendarScope::PINNED_FIXTURE);
 Error quote(Event); // transactional: all errors leave every field unchanged
 Error advance_time(std::int64_t);
 const auto& bars() const { return bars_; }
 const auto& log() const { return log_; }
 const auto& state() const { return ledger_.state(); }
 const auto& pending() const { return pending_; }
 const auto& stop() const { return stop_; }
 const auto& events() const { return stream_.events(); }
 auto now() const { return now_; }
 auto entries() const { return entries_; }
 bool failed() const { return failed_; }
};
}
