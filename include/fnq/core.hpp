#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>
#include <type_traits>
namespace fnq {
using Ticks = std::int64_t;
using Cents = std::int64_t;
using Contracts = std::int32_t;
enum class Error { NONE, OVERFLOW, PARSE, OFF_GRID, FRACTIONAL_QTY, NON_POSITIVE_INPUT, INVALID_COST, CAP_EXCEEDED, SEQ_NOT_INCREASING, TIME_DECREASING, NON_POSITIVE_PRICE, CROSSED_QUOTE, SAME_OR_PAST_EVENT, INVALID_EVENT, INVALID_SIDE };
template<class T> struct Result {
 std::optional<T> value;
 Error error = Error::NONE;
 explicit operator bool() const { return value.has_value(); }
 static Result ok(T v) { return {v, Error::NONE}; }
 static Result fail(Error e) { return {std::nullopt, e}; }
};
template<class T> Result<T> checked_add(T a,T b) {
 static_assert(std::is_same_v<T,std::int64_t> || std::is_same_v<T,std::int32_t>);
 if ((b>0 && a>std::numeric_limits<T>::max()-b) || (b<0 && a<std::numeric_limits<T>::min()-b)) return Result<T>::fail(Error::OVERFLOW);
 return Result<T>::ok(a+b);
}
template<class T> Result<T> checked_sub(T a,T b) {
 if ((b<0 && a>std::numeric_limits<T>::max()+b) || (b>0 && a<std::numeric_limits<T>::min()+b)) return Result<T>::fail(Error::OVERFLOW);
 return Result<T>::ok(a-b);
}
template<class T> Result<T> checked_mul(T a,T b) {
 const auto lo=std::numeric_limits<T>::min(), hi=std::numeric_limits<T>::max();
 if (a!=0 && b!=0 && ((a>0 && b>0 && a>hi/b) || (a>0 && b<0 && b<lo/a) || (a<0 && b>0 && a<lo/b) || (a<0 && b<0 && a<hi/b))) return Result<T>::fail(Error::OVERFLOW);
 return Result<T>::ok(a*b);
}
Result<Ticks> parse_price(std::string_view);
Result<Contracts> parse_quantity(std::string_view);
struct Instrument { std::string_view symbol; std::int64_t ticks_per_point; Cents tick_value_cents; };
inline constexpr Instrument MES{"MES",4,125}, MNQ{"MNQ",4,50};
struct Sizing { Contracts size{}; Cents risk_per_contract{}; Error error{}; };
Sizing size_for(Cents budget,Ticks stop,Cents tick,Cents fee,Ticks slippage,Contracts cap=1);
struct Event { std::uint64_t seq; std::int64_t t_utc_ns; Ticks bid,ask; bool operator==(const Event&) const = default; };
struct EventResult { std::uint64_t seq; Error error; explicit operator bool() const { return error==Error::NONE; } };
class EventStream {
 std::vector<Event> events_;
public:
 EventResult admit(Event);
 bool contains(std::uint64_t seq) const;
 const auto& events() const { return events_; }
 bool operator==(const EventStream&) const = default;
};
enum class Side { BUY, SELL };
struct Lot { Side side; Contracts quantity; Ticks entry_ticks; bool operator==(const Lot&) const = default; };
struct State {
 Contracts position{};
 std::vector<Lot> lots;
 Cents gross{},fees{},net{};
 std::optional<std::uint64_t> last_fill_seq;
 bool operator==(const State&) const = default;
};
class Ledger {
 Instrument instrument_;
 Cents fee_;
 Contracts cap_;
 State state_;
public:
 Ledger(Instrument instrument,Cents fee,Contracts cap=1):instrument_(instrument),fee_(fee),cap_(cap) {}
 const State& state() const { return state_; }
 Error fill(Side,Contracts,Ticks,std::uint64_t decision_seq,std::uint64_t fill_seq,const EventStream&);
};
}
