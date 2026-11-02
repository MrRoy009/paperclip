#include "check.hpp"
int main(int argc,char** argv) { if(argc!=2) return 1; return run([&]{
 switch(test_number(argv)) {
 case 7:
 CHECK(*parse_price("5000.25").value==20001); CHECK(*parse_price("5000.00").value==20000); CHECK(*parse_price("0.25").value==1);
 CHECK(parse_price("abc").error==Error::PARSE); CHECK(*parse_quantity("1").value==1);
 CHECK(parse_price("2305843009213693952").error==Error::OVERFLOW);
 CHECK(*parse_price("-2305843009213693952").value==lo); break;
 case 8: CHECK(parse_price("5000.10").error==Error::OFF_GRID); CHECK(parse_price("5000.125").error==Error::OFF_GRID); break;
 case 9: CHECK(parse_quantity("1.5").error==Error::FRACTIONAL_QTY); CHECK(parse_quantity("-0.5").error==Error::FRACTIONAL_QTY); CHECK(!parse_quantity("0")); CHECK(!parse_quantity("-1")); break;
 case 10: CHECK(!checked_mul<std::int64_t>(hi/2+1,2)); CHECK(!checked_mul<std::int64_t>(lo,-1)); CHECK(*checked_mul<std::int64_t>(lo,1).value==lo); CHECK(!checked_mul<std::int32_t>(std::numeric_limits<std::int32_t>::min(),-1)); break;
 case 11: CHECK(!checked_add<std::int64_t>(hi,1)); CHECK(!checked_sub<std::int64_t>(lo,1)); CHECK(!checked_add<std::int32_t>(std::numeric_limits<std::int32_t>::max(),1)); CHECK(!checked_sub<std::int32_t>(std::numeric_limits<std::int32_t>::min(),1)); break;
 default: CHECK(false);
 }
 }); }
