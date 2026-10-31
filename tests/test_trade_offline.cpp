// Offline branch tests of the actual engine via mechanical C++ conversion.
// No network or real trading APIs are called. Not a MetaEditor compilation.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using std::string;
using uint = unsigned int;
using mql_ulong = std::uint64_t;

enum ENUM_ORDER_TYPE { ORDER_TYPE_BUY, ORDER_TYPE_SELL, ORDER_TYPE_BUY_LIMIT,
 ORDER_TYPE_SELL_LIMIT, ORDER_TYPE_BUY_STOP, ORDER_TYPE_SELL_STOP,
 ORDER_TYPE_BUY_STOP_LIMIT, ORDER_TYPE_SELL_STOP_LIMIT };
enum ENUM_ORDER_TYPE_FILLING { ORDER_FILLING_FOK, ORDER_FILLING_IOC, ORDER_FILLING_RETURN };
enum { POSITION_SYMBOL=100, POSITION_MAGIC, POSITION_TYPE, POSITION_VOLUME,
 POSITION_IDENTIFIER, POSITION_COMMENT, POSITION_PRICE_OPEN, POSITION_SL,
 POSITION_TP, POSITION_TICKET, ORDER_SYMBOL, ORDER_MAGIC, ORDER_TYPE,
 ORDER_VOLUME_CURRENT, ORDER_TICKET, TERMINAL_CONNECTED, TERMINAL_TRADE_ALLOWED,
 MQL_TRADE_ALLOWED, ACCOUNT_TRADE_ALLOWED, ACCOUNT_TRADE_EXPERT,
 ACCOUNT_MARGIN_MODE, ACCOUNT_LIMIT_ORDERS, ACCOUNT_LOGIN, ACCOUNT_BALANCE,
 ACCOUNT_EQUITY, ACCOUNT_MARGIN_FREE, ACCOUNT_MARGIN, ACCOUNT_SERVER,
 ACCOUNT_CURRENCY, SYMBOL_TRADE_EXEMODE, SYMBOL_FILLING_MODE,
 SYMBOL_EXPIRATION_MODE, SYMBOL_POINT, SYMBOL_TRADE_TICK_SIZE, SYMBOL_DIGITS,
 SYMBOL_TRADE_MODE, SYMBOL_ORDER_MODE, SYMBOL_TRADE_STOPS_LEVEL,
 SYMBOL_TRADE_FREEZE_LEVEL, SYMBOL_VOLUME_MIN, SYMBOL_VOLUME_MAX,
 SYMBOL_VOLUME_STEP, SYMBOL_VOLUME_LIMIT, DEAL_ENTRY, DEAL_MAGIC, DEAL_SYMBOL, DEAL_SL, DEAL_TYPE };
enum { POSITION_TYPE_BUY=0, POSITION_TYPE_SELL=1,
 ACCOUNT_MARGIN_MODE_RETAIL_HEDGING=2, ACCOUNT_MARGIN_MODE_RETAIL_NETTING=0,
 SYMBOL_TRADE_EXECUTION_REQUEST=0, SYMBOL_TRADE_EXECUTION_INSTANT=1,
 SYMBOL_TRADE_EXECUTION_MARKET=2, SYMBOL_TRADE_EXECUTION_EXCHANGE=3,
 SYMBOL_TRADE_MODE_DISABLED=0, SYMBOL_TRADE_MODE_LONGONLY=1,
 SYMBOL_TRADE_MODE_SHORTONLY=2, SYMBOL_TRADE_MODE_CLOSEONLY=3, SYMBOL_TRADE_MODE_FULL=4,
 SYMBOL_FILLING_FOK=1, SYMBOL_FILLING_IOC=2, SYMBOL_EXPIRATION_GTC=1,
 SYMBOL_EXPIRATION_DAY=2, SYMBOL_ORDER_MARKET=1, SYMBOL_ORDER_LIMIT=2,
 SYMBOL_ORDER_STOP=4, SYMBOL_ORDER_SL=8, SYMBOL_ORDER_TP=16,
 ORDER_TIME_GTC=0, ORDER_TIME_DAY=1, TRADE_ACTION_DEAL=1,
 TRADE_ACTION_PENDING=5, TRADE_ACTION_SLTP=6, TRADE_ACTION_REMOVE=8,
 TRADE_RETCODE_PLACED=10008, TRADE_RETCODE_DONE=10009,
 TRADE_RETCODE_DONE_PARTIAL=10010, TRADE_RETCODE_NO_CHANGES=10025,
 TRADE_RETCODE_NO_MONEY=10019, DEAL_ENTRY_IN=0, DEAL_ENTRY_INOUT=2,
 DEAL_TYPE_BUY=0,DEAL_TYPE_SELL=1 };
struct MqlTick { double bid=0, ask=0; };
struct MqlTradeRequest {
 int action=0; string symbol,comment; mql_ulong magic=0,deviation=0,position=0,order=0;
 ENUM_ORDER_TYPE type=ORDER_TYPE_BUY; double price=0,sl=0,tp=0,volume=0;
 ENUM_ORDER_TYPE_FILLING type_filling=ORDER_FILLING_FOK; int type_time=0;
};
struct MqlTradeCheckResult { uint retcode=0; string comment; double margin=0; };
struct MqlTradeResult { uint retcode=0; string comment; mql_ulong order=0,deal=0; double volume=0; };
struct Exposure {
 mql_ulong ticket=1,magic=777,identifier=1; string symbol="TEST",comment="CS:100.00000000";
 int type=POSITION_TYPE_BUY; double volume=1,open=100,sl=99,tp=110;
};
struct Deal { mql_ulong magic=777;int type=DEAL_TYPE_BUY,entry=DEAL_ENTRY_IN;double sl=99;string symbol="TEST"; };
struct StubState {
 double min=.01,max=100,step=.01,limit=0,point=.01,ticksize=.01;
 double balance=10000,equity=10000,free_margin=100000,margin=0;
 double multiplier=100,margin_per_lot=100,check_margin_extra=0;
 long margin_mode=ACCOUNT_MARGIN_MODE_RETAIL_HEDGING;
 long stops=0,freeze=0,order_limit=0;
 bool allow=true,valid_tick=true,exposure_during_check=false,history_ok=false;
 MqlTick tick{99.9,100};
 std::vector<Exposure> positions,orders;
 std::vector<Deal> deals;
 int selected_pos=-1,selected_order=-1,check_calls=0,fail_check_at=0;
 std::vector<uint> retcodes;
 std::vector<MqlTradeRequest> sent;
 std::vector<double> profit_volumes;
 std::map<string,double> globals;
 mql_ulong now=100000;
} G;

bool MathIsValidNumber(double x) { return std::isfinite(x); }
double MathAbs(double x) { return std::abs(x); }
double MathFloor(double x) { return std::floor(x); }
double MathRound(double x) { return std::round(x); }
double MathMax(double a,double b) { return std::max(a,b); }
double NormalizeDouble(double x,int d) { double s=std::pow(10.,d); return std::round(x*s)/s; }
template<class T> void ZeroMemory(T &v) { v=T{}; }
int StringLen(const string &s) { return static_cast<int>(s.size()); }
string StringSubstr(const string &s,int p,int n=-1) { return s.substr(p,n<0?string::npos:static_cast<size_t>(n)); }
int StringFind(const string &s,const string &p) { auto n=s.find(p);return n==string::npos?-1:static_cast<int>(n); }
uint StringGetCharacter(const string &s,int i) { return static_cast<unsigned char>(s.at(i)); }
double StringToDouble(const string &s) { try {return std::stod(s);}catch(...){return 0;} }
string DoubleToString(double v,int d) { std::ostringstream o;o<<std::fixed<<std::setprecision(d)<<v;return o.str(); }
// Formatting is a harmless placeholder: tests assert engine decisions, requests,
// prices and error text literals, not the platform's formatting implementation.
template<class... T> string StringFormat(const string &s,const T&... args) { return s; }
template<class... T> void Print(const T&... args) {}
template<class... T> void PrintFormat(const T&... args) {}
template<class T> int ArraySize(const std::vector<T>& v) {return static_cast<int>(v.size());}
template<class T> void ArrayResize(std::vector<T>& v,int n) {v.resize(n);}
void ResetLastError() {}
int GetLastError() {return 0;}
mql_ulong GetTickCount64() {return G.now;}
bool SymbolSelect(const string&,bool) {return true;}
bool SymbolInfoTick(const string&,MqlTick &t) {t=G.tick;return G.valid_tick;}
long TerminalInfoInteger(int) {return G.allow;}
long MQLInfoInteger(int) {return G.allow;}
long AccountInfoInteger(int prop) {
 if(prop==ACCOUNT_MARGIN_MODE)return G.margin_mode;
 if(prop==ACCOUNT_LIMIT_ORDERS)return G.order_limit;
 if(prop==ACCOUNT_LOGIN)return 1234;
 return G.allow;
}
double AccountInfoDouble(int prop) {
 switch(prop) {case ACCOUNT_BALANCE:return G.balance;case ACCOUNT_EQUITY:return G.equity;
 case ACCOUNT_MARGIN_FREE:return G.free_margin;case ACCOUNT_MARGIN:return G.margin;default:return 0;}
}
string AccountInfoString(int p) {return p==ACCOUNT_CURRENCY?"USD":"StubServer";}
double SymbolInfoDouble(const string&,int p) {
 switch(p) {case SYMBOL_POINT:return G.point;case SYMBOL_TRADE_TICK_SIZE:return G.ticksize;
 case SYMBOL_VOLUME_MIN:return G.min;case SYMBOL_VOLUME_MAX:return G.max;
 case SYMBOL_VOLUME_STEP:return G.step;case SYMBOL_VOLUME_LIMIT:return G.limit;default:return 0;}
}
long SymbolInfoInteger(const string&,int p) {
 switch(p) {case SYMBOL_TRADE_EXEMODE:return SYMBOL_TRADE_EXECUTION_MARKET;
 case SYMBOL_FILLING_MODE:return SYMBOL_FILLING_FOK;case SYMBOL_EXPIRATION_MODE:return SYMBOL_EXPIRATION_GTC;
 case SYMBOL_DIGITS:return 2;case SYMBOL_TRADE_MODE:return SYMBOL_TRADE_MODE_FULL;
 case SYMBOL_ORDER_MODE:return SYMBOL_ORDER_MARKET|SYMBOL_ORDER_LIMIT|SYMBOL_ORDER_STOP|SYMBOL_ORDER_SL|SYMBOL_ORDER_TP;
 case SYMBOL_TRADE_STOPS_LEVEL:return G.stops;case SYMBOL_TRADE_FREEZE_LEVEL:return G.freeze;default:return 0;}
}
bool legal_volume(double v) {return v>=G.min-1e-9&&v<=G.max+1e-9&&std::abs(v/G.step-std::round(v/G.step))<1e-6;}
bool OrderCalcProfit(ENUM_ORDER_TYPE t,const string&,double v,double a,double b,double &result) {
 G.profit_volumes.push_back(v);if(!legal_volume(v))return false;
 result=(b-a)*(t==ORDER_TYPE_BUY?1.:-1.)*v*G.multiplier;return true;
}
bool OrderCalcMargin(ENUM_ORDER_TYPE,const string&,double v,double,double &out) {out=v*G.margin_per_lot;return legal_volume(v);}
int PositionsTotal() {return static_cast<int>(G.positions.size());}
int OrdersTotal() {return static_cast<int>(G.orders.size());}
mql_ulong PositionGetTicket(int i) {if(i<0||i>=PositionsTotal())return 0;G.selected_pos=i;return G.positions[i].ticket;}
mql_ulong OrderGetTicket(int i) {if(i<0||i>=OrdersTotal())return 0;G.selected_order=i;return G.orders[i].ticket;}
bool PositionSelectByTicket(mql_ulong t) {for(int i=0;i<PositionsTotal();i++)if(G.positions[i].ticket==t){G.selected_pos=i;return true;}return false;}
bool OrderSelect(mql_ulong t) {for(int i=0;i<OrdersTotal();i++)if(G.orders[i].ticket==t){G.selected_order=i;return true;}return false;}
Exposure &pos() {return G.positions.at(G.selected_pos);}
Exposure &ord() {return G.orders.at(G.selected_order);}
string PositionGetString(int p) {return p==POSITION_SYMBOL?pos().symbol:pos().comment;}
long PositionGetInteger(int p) {switch(p){case POSITION_MAGIC:return pos().magic;case POSITION_TYPE:return pos().type;
 case POSITION_IDENTIFIER:return pos().identifier;case POSITION_TICKET:return pos().ticket;default:return 0;}}
double PositionGetDouble(int p) {switch(p){case POSITION_VOLUME:return pos().volume;case POSITION_PRICE_OPEN:return pos().open;
 case POSITION_SL:return pos().sl;case POSITION_TP:return pos().tp;default:return 0;}}
string OrderGetString(int) {return ord().symbol;}
long OrderGetInteger(int p) {switch(p){case ORDER_MAGIC:return ord().magic;case ORDER_TYPE:return ord().type;
 case ORDER_TICKET:return ord().ticket;default:return 0;}}
double OrderGetDouble(int) {return ord().volume;}
bool HistorySelectByPosition(mql_ulong) {return G.history_ok;}
int HistoryDealsTotal() {return static_cast<int>(G.deals.size());}
mql_ulong HistoryDealGetTicket(int i) {return i>=0&&i<HistoryDealsTotal()?400+i:0;}
long HistoryDealGetInteger(mql_ulong t,int p) {auto &d=G.deals.at(t-400);if(p==DEAL_MAGIC)return d.magic;return p==DEAL_ENTRY?d.entry:d.type;}
string HistoryDealGetString(mql_ulong t,int) {return G.deals.at(t-400).symbol;}
double HistoryDealGetDouble(mql_ulong t,int) {return G.deals.at(t-400).sl;}
bool GlobalVariableGet(const string &s,double &v) {auto it=G.globals.find(s);if(it==G.globals.end())return false;v=it->second;return true;}
long GlobalVariableSet(const string &s,double v) {G.globals[s]=v;return 1;}
void GlobalVariablesFlush() {}
bool OrderCheck(MqlTradeRequest &r,MqlTradeCheckResult &c) {
 G.check_calls++;c.margin=G.margin+r.volume*G.margin_per_lot+G.check_margin_extra;
 if(G.exposure_during_check&&G.positions.empty())G.positions.push_back(Exposure{});
 if(G.fail_check_at==G.check_calls){c.retcode=TRADE_RETCODE_NO_MONEY;c.comment="No money";return false;}
 c.retcode=TRADE_RETCODE_DONE;return true;
}
bool OrderSend(MqlTradeRequest &r,MqlTradeResult &out) {
 size_t idx=G.sent.size();G.sent.push_back(r);out.retcode=idx<G.retcodes.size()?G.retcodes[idx]:static_cast<uint>(TRADE_RETCODE_DONE);
 out.volume=out.retcode==TRADE_RETCODE_DONE_PARTIAL?r.volume*.5:r.volume;
 out.order=100+idx;out.deal=200+idx;return true;
}

#include "SizerTrade.offline.hpp"

int checks=0;
void require(bool v,const string &s) {checks++;if(!v)throw std::runtime_error(s);}
void near(double a,double b,const string &s) {require(std::abs(a-b)<1e-8,s);}
CSPlan plan() {CSPlan p{};p.buy=true;p.mode=CS_MARKET;p.sl=99;p.tp[0]=102;p.tp[1]=103;p.tp[2]=104;p.targets=1;p.risk_percent=1;return p;}
CSizerTrade reset() {G=StubState{};CSizerTrade e;e.Configure("TEST",777,10,0);return e;}
void run(const string &name,void(*fn)()) {fn();std::cout<<"PASS "<<name<<'\n';}

void rounding() {
 auto e=reset();auto p=plan();CSQuote q{};G.min=.25;G.step=.25;p.risk_percent=1.13;
 require(e.Quote(p,q),".25 quote rejected");near(q.volume,1,".25 must floor 1.13 -> 1");require(q.risk_cash<=113,".25 exceeds risk");
 G.min=.001;G.step=.001;p.risk_percent=1.2349;
 require(e.Quote(p,q),".001 quote rejected");near(q.volume,1.234,".001 must floor 1.2349 -> 1.234");require(q.risk_cash<=123.49,".001 exceeds risk");
}
void minimum_and_reference() {
 auto e=reset();auto p=plan();CSQuote q{};G.min=.25;G.step=.25;p.risk_percent=.2;
 require(!e.Quote(p,q),"below min must refuse");require(q.error.find("below")!=string::npos,"below min error");
 e=reset();p=plan();G.min=2;G.step=1;G.max=10;p.risk_percent=2.1;
 require(!legal_volume(1),"reference test setup");require(e.Quote(p,q),"legal min reference must succeed when 1 lot illegal");near(q.volume,2,"volume 2");
 require(std::all_of(G.profit_volumes.begin(),G.profit_volumes.end(),[](double v){return v==2;}),"profit reference must be legal min 2");
}
void splitting() {
 auto e=reset();auto p=plan();CSQuote q{};G.min=.25;G.step=.25;p.targets=3;p.risk_percent=1.13;
 require(e.Quote(p,q),"split quote");near(q.leg_volume[0],.25,"leg1");near(q.leg_volume[1],.25,"leg2");near(q.leg_volume[2],.5,"remainder leg3");
 near(q.leg_volume[0]+q.leg_volume[1]+q.leg_volume[2],q.volume,"split sum");
 for(int i=0;i<3;i++)require(legal_volume(q.leg_volume[i]),"split legality");
 p.risk_percent=.5;require(!e.Quote(p,q),"three sub-min legs must reject");
 p.risk_percent=4;G.max=1;require(!e.Quote(p,q),"leg above max must reject");
}
void prices() {
 auto e=reset();auto p=plan();CSQuote q{};
 p.sl=100.1;require(!e.Quote(p,q),"buy SL wrong side");p=plan();p.tp[0]=99;require(!e.Quote(p,q),"buy TP wrong side");
 p=plan();p.mode=CS_LIMIT;p.entry=101;require(!e.Quote(p,q),"buy limit above ask");
 p=plan();p.mode=CS_STOP;p.entry=99.5;p.sl=98;require(!e.Quote(p,q),"buy stop below ask");
 p=plan();p.buy=false;p.sl=101;p.tp[0]=98;require(e.Quote(p,q),"valid sell");
 p.sl=99;require(!e.Quote(p,q),"sell SL wrong side");
 p=plan();G.stops=100;p.sl=99.5;require(!e.Quote(p,q),"minimum stops must use Bid for market buy");
}
void netting_and_limits() {
 auto e=reset();auto p=plan();CSQuote q{};G.margin_mode=ACCOUNT_MARGIN_MODE_RETAIL_NETTING;
 G.positions.push_back(Exposure{});G.positions.back().magic=999;
 require(!e.Quote(p,q),"netting must refuse foreign symbol position");
 G.positions.clear();G.orders.push_back(Exposure{});G.orders.back().magic=999;
 require(!e.Quote(p,q),"netting must refuse foreign symbol pending");
 G.orders.clear();p.targets=2;require(!e.Quote(p,q),"netting multiple targets");
 p.targets=1;require(e.Quote(p,q),"empty netting single target");
 G.exposure_during_check=true;string msg;require(!e.Execute(p,msg),"netting race exposure");require(G.sent.empty(),"netting race sent no order");
 e=reset();p=plan();G.limit=1.5;G.positions.push_back(Exposure{});G.positions.back().magic=999;
 require(!e.Quote(p,q),"aggregate directional volume must count foreign magic");
}
void margin_and_preflight() {
 auto e=reset();auto p=plan();CSQuote q{};G.free_margin=99;
 require(!e.Quote(p,q),"insufficient aggregate margin");string msg;require(!e.Execute(p,msg)&&G.sent.empty(),"margin refuses sends");
 e=reset();p=plan();p.targets=3;G.fail_check_at=2;
 require(!e.Execute(p,msg),"second preflight reject");require(G.check_calls==2&&G.sent.empty(),"all preflight before sending");
 e=reset();p=plan();p.targets=3;G.free_margin=200;G.check_margin_extra=100;
 require(!e.Execute(p,msg)&&G.sent.empty(),"broker checked aggregate margin refuse");
}
void execution_stops() {
 auto e=reset();auto p=plan();p.targets=3;string msg;G.retcodes={TRADE_RETCODE_DONE_PARTIAL};
 require(!e.Execute(p,msg),"partial result must report incomplete");require(G.sent.size()==1,"partial must stop later target sends");
 e=reset();p=plan();p.targets=3;G.retcodes={TRADE_RETCODE_PLACED};
 require(!e.Execute(p,msg),"market PLACED must report waiting");require(G.sent.size()==1,"market PLACED stops later sends");
 e=reset();p=plan();p.targets=3;require(e.Execute(p,msg),"all done succeeds");require(G.sent.size()==3,"all done sends 3");
 for(const auto&r:G.sent)require(legal_volume(r.volume),"execution legal leg");
 near(G.sent[0].volume+G.sent[1].volume+G.sent[2].volume,1,"sent sum");
}
void management() {
 auto e=reset();G.positions.push_back(Exposure{});G.positions.back().magic=999;
 G.tick={102,102.1};e.Manage(true,false,1,1,0);require(G.sent.empty(),"manage ignores foreign magic");
 e=reset();G.positions.push_back(Exposure{});G.tick={102,102.1};
 e.Manage(true,false,1,1,0);require(G.sent.size()==1,"BE emits modify");
 require(G.sent[0].action==TRADE_ACTION_SLTP,"BE SLTP action");near(G.sent[0].sl,100,"BE open price");near(G.sent[0].tp,110,"preserve TP");
 e.Manage(true,false,1,1,0);require(G.sent.size()==1,"manage throttled within 1s");
 e=reset();G.positions.push_back(Exposure{});G.positions.back().sl=101;G.tick={102,102.1};
 e.Manage(true,false,1,1,0);require(G.sent.empty(),"BE never loosens existing stop");
 e=reset();G.positions.push_back(Exposure{});G.tick={103,103.1};
 e.Manage(true,true,1,1,0);require(G.sent.size()==1,"trail emits modify");near(G.sent[0].sl,102,"trail based on original R");
 e=reset();G.margin_mode=ACCOUNT_MARGIN_MODE_RETAIL_NETTING;G.positions.push_back(Exposure{});G.tick={102,102.1};
 e.Manage(true,false,1,1,0);require(G.sent.empty(),"netting missing history must skip manage");
 e=reset();G.margin_mode=ACCOUNT_MARGIN_MODE_RETAIL_NETTING;G.positions.push_back(Exposure{});G.tick={102,102.1};
 G.history_ok=true;G.deals.push_back(Deal{});G.deals.push_back(Deal{});G.deals.back().magic=999;
 e.Manage(true,false,1,1,0);require(G.sent.empty(),"netting mixed provenance must skip manage");
 string msg;require(!e.ClosePositions(msg),"excluded mixed netting position reports incomplete close");require(G.sent.empty(),"netting mixed provenance must skip close");
 e=reset();G.margin_mode=ACCOUNT_MARGIN_MODE_RETAIL_NETTING;G.positions.push_back(Exposure{});G.tick={102,102.1};
 G.history_ok=true;G.deals.push_back(Deal{});
 e.Manage(true,false,1,1,0);require(G.sent.size()==1,"verified sole-owner netting manage");near(G.sent[0].sl,100,"verified netting BE");
}
void per_target_cash_estimates() {
 for(bool buy:{true,false})for(int targets:{2,3})for(double fees:{0.,5.}) {
  auto e=reset();G.min=.25;G.step=.25;e.Configure("TEST",777,10,fees);
  auto p=plan();p.buy=buy;p.targets=targets;p.risk_percent=fees==0?1.3:1.4;
  double entry=buy?G.tick.ask:G.tick.bid,sign=buy?1.:-1.;
  p.sl=entry-sign;for(int i=0;i<3;i++)p.tp[i]=entry+sign*(2+2*i);
  CSQuote q{};require(e.Quote(p,q)&&q.valid,"BUY/SELL multi-target cash quote must succeed");
  near(q.volume,1.25,"per-target cash quote retains existing floored total volume");
  double risk=0,reward=0;
  for(int i=0;i<targets;i++) {
   double volume=(i==targets-1?(targets==2?.75:.75):(targets==2?.5:.25));
   near(q.leg_volume[i],volume,"cash figures use actual unequal leg split");
   near(q.leg_risk_cash[i],volume*(100+fees),"each leg risk includes its share of estimated round-trip fees");
   near(q.leg_reward_cash[i],volume*((2+2*i)*100-fees),"each leg reward uses its own TP and subtracts its own estimated fees");
   risk+=q.leg_risk_cash[i];reward+=q.leg_reward_cash[i];
  }
  near(risk,q.risk_cash,"leg risks sum to unchanged aggregate risk");near(reward,q.reward_cash,"leg rewards sum to unchanged aggregate reward");
  near(q.risk_cash,1.25*(100+fees),"aggregate risk semantics are unchanged");near(q.rr,reward/risk,"R:R uses aggregate estimated target payouts");
  for(int i=targets;i<3;i++){near(q.leg_risk_cash[i],0,"unused risk slot resets to zero");near(q.leg_reward_cash[i],0,"unused reward slot resets to zero");}
 }
 auto e=reset();G.min=2;G.step=1;G.max=10;e.Configure("TEST",777,10,5);
 auto p=plan();p.targets=3;p.risk_percent=7.5;p.tp[0]=102;p.tp[1]=104;p.tp[2]=106;CSQuote q{};
 require(e.Quote(p,q)&&q.valid,"cash quote supports a legal minimum reference above one lot");
 near(q.leg_volume[0],2,"large-reference first leg");near(q.leg_volume[1],2,"large-reference second leg");near(q.leg_volume[2],3,"large-reference uneven last leg");
 near(q.leg_risk_cash[2],315,"last-leg risk uses its actual three lots");near(q.leg_reward_cash[2],1785,"last-leg reward uses its TP6 price and five-per-lot fees");
 require(std::all_of(G.profit_volumes.begin(),G.profit_volumes.end(),[](double volume){return volume==2;}),"all cash calculations use broker-legal min reference rather than assumed one lot");
 p.sl=101;require(!e.Quote(p,q)&&!q.valid,"invalid SL cannot expose a valid cash quote");
 for(int i=0;i<3;i++){near(q.leg_risk_cash[i],0,"failed early quote clears prior leg-risk output");near(q.leg_reward_cash[i],0,"failed early quote clears prior leg-reward output");}
 p.sl=99;G.free_margin=1;require(!e.Quote(p,q)&&!q.valid&&!q.error.empty(),"late margin failure leaves quote invalid even if intermediate cash arrays were calculated");
 G.free_margin=100000;p.targets=1;require(e.Quote(p,q)&&q.valid,"quote can recover after failure");
 for(int i=1;i<3;i++){near(q.leg_risk_cash[i],0,"one-target quote cannot retain prior extra-leg risk");near(q.leg_reward_cash[i],0,"one-target quote cannot retain prior extra-leg reward");}
}
int main() {
 try {
 run("risk flooring step 0.25 / 0.001",rounding);
 run("below minimum refusal / legal non-1-lot reference",minimum_and_reference);
 run("target split total, min, max, step",splitting);
 run("wrong-side prices and broker stop distance",prices);
 run("netting exposure isolation and aggregate direction limit",netting_and_limits);
 run("aggregate margin and no-send preflight failure",margin_and_preflight);
 run("partial and market PLACED stop later sends",execution_stops);
 run("BE/trail ownership, tightening, original R, throttle",management);
 run("per-target BUY/SELL cash estimates, unequal splits, fees and reset validity",per_target_cash_estimates);
 std::cout<<"SUCCESS: "<<checks<<" assertions across 9 test groups.\n";
 } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
