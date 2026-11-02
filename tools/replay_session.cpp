#include "fnq/strategy.hpp"
#include <charconv>
#include <iostream>
#include <sstream>
#include <string>
using namespace fnq;
template<class T> T integer(const std::string& s) {
 T v{}; auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),v);
 if(e!=std::errc{} || p!=s.data()+s.size() || s.empty()) throw std::runtime_error("invalid integer");
 return v;
}
int main(int argc,char**argv) {
 try {
  if(argc!=2) throw std::runtime_error("usage: fnq_replay_session UTC_OPEN_NS < normalized.csv");
  auto open=integer<std::int64_t>(argv[1]);
  auto end=checked_add(open,26*quarter),cutoff=checked_add(open,22*quarter),flat=checked_add(open,22800*second);
  if(!end || !cutoff || !flat) throw std::runtime_error("calendar overflow");
  Strategy s({open,*end.value,*cutoff.value,*flat.value,{}},CalendarScope::EXPLICIT_UTC);
  std::string line; std::size_t count=0; std::int64_t previous=open,maxgap=0;
  while(std::getline(std::cin,line)) {
   if(++count>25000) throw std::runtime_error("prototype limit 25000 quotes/session exceeded; no truncation permitted");
   std::stringstream input(line); std::string a,b,c,d,extra;
   if(std::count(line.begin(),line.end(),',')!=3 || !std::getline(input,a,',') || !std::getline(input,b,',') || !std::getline(input,c,',') || !std::getline(input,d,',') || std::getline(input,extra,',')) throw std::runtime_error("invalid normalized row");
   Event e{integer<std::uint64_t>(a),integer<std::int64_t>(b),integer<Ticks>(c),integer<Ticks>(d)};
   if(e.t_utc_ns<open || e.t_utc_ns>=*end.value) throw std::runtime_error("quote outside selected session");
   if(s.quote(e)!=Error::NONE) throw std::runtime_error("strategy rejected row "+std::to_string(count));
   maxgap=std::max(maxgap,e.t_utc_ns-previous);previous=e.t_utc_ns;
  }
  if(std::cin.bad()) throw std::runtime_error("input read failure");
  if(s.advance_time(*end.value)!=Error::NONE) throw std::runtime_error("terminal clock error");
  maxgap=std::max(maxgap,*end.value-previous);
  int missing=0;for(auto&b:s.bars())if(!b.present||b.spans_halt)++missing;
  std::cout<<"{\"quotes\":"<<count<<",\"entries\":"<<s.entries()<<",\"gross_cents\":"<<s.state().gross<<",\"fees_cents\":"<<s.state().fees<<",\"net_cents\":"<<s.state().net<<",\"unclosed_position\":"<<s.state().position<<",\"flat_by_failure\":"<<(s.failed()?"true":"false")<<",\"missing_bars\":"<<missing<<",\"max_quote_gap_ns\":"<<maxgap<<",\"events\":[";
  bool first=true; for(auto&r:s.log()) {
   if(!first)std::cout<<',';
   first=false;
   std::cout<<"{\"timestamp_ns\":"<<r.time<<",\"seq\":"<<r.seq<<",\"reason\":\""<<r.reason<<"\",\"price_ticks\":"<<r.price<<",\"distance_ticks\":"<<r.distance<<",\"position_after\":"<<r.position<<'}';
  }
  std::cout<<"]}\n";
 } catch(const std::exception&e) {std::cerr<<e.what()<<'\n';return 1;}
}
