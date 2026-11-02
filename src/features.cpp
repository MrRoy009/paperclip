#include "fnq/strategy.hpp"
namespace fnq {
Result<Features> features(const std::vector<Bar>& b) {
 if(b.size()<9) return Result<Features>::fail(Error::INVALID_EVENT);
 auto start=b.size()-9;
 Ticks high=0,low=std::numeric_limits<Ticks>::max(),sum=0;
 for(auto i=start;i<b.size();++i) {
  if(!b[i].present || b[i].spans_halt) return Result<Features>::fail(Error::INVALID_EVENT);
  if(i+1<b.size()) { high=std::max(high,b[i].high); low=std::min(low,b[i].low); }
  if(i>start) { auto a=checked_add(sum,b[i].tr); if(!a) return Result<Features>::fail(a.error); sum=*a.value; }
 }
 auto rounded=checked_add<Ticks>(sum,7); if(!rounded) return Result<Features>::fail(rounded.error);
 return Result<Features>::ok({high,low,std::max<Ticks>(8,*rounded.value/8)});
}
}
