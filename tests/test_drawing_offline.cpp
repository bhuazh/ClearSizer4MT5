// In-memory chart stubs; decision/state functions come from the current .mq5.
#include "DrawingBroker.offline.hpp"
#include "SizerTrade.offline.hpp"
using color=int;
using datetime=long;
using ushort=unsigned short;
const string _Symbol="TEST";
const string InpFont="Arial";
const double _Point=.01,InpStopPoints=300,InpRewardR=2;
const int _Digits=2;
const mql_ulong InpMagic=777;
enum { OBJ_HLINE=2000,OBJ_RECTANGLE,OBJPROP_PRICE,OBJPROP_COLOR,OBJPROP_STYLE,
       OBJPROP_WIDTH,OBJPROP_BACK,OBJPROP_SELECTABLE,OBJPROP_HIDDEN,OBJPROP_ZORDER,
       OBJPROP_TEXT,OBJPROP_TOOLTIP,OBJPROP_FILL,STYLE_DASH,CHART_WIDTH_IN_PIXELS,
       CHART_HEIGHT_IN_PIXELS,CHARTEVENT_KEYDOWN,CHARTEVENT_MOUSE_MOVE,
       CHARTEVENT_CLICK,CHARTEVENT_OBJECT_CLICK,CHARTEVENT_CHART_CHANGE,
       CHARTEVENT_OBJECT_ENDEDIT,CHARTEVENT_OBJECT_DRAG,CHART_EVENT_MOUSE_MOVE,OBJPROP_SELECTED,
       OBJ_LABEL,ANCHOR_RIGHT_LOWER,CORNER_LEFT_UPPER,OBJPROP_XDISTANCE,OBJPROP_YDISTANCE,
       OBJPROP_ANCHOR,OBJPROP_CORNER,OBJPROP_FONTSIZE,OBJPROP_FONT };
struct ChartObject {
 int type=0;double price=0;bool selectable=false;string text;
 datetime times[2]={0,0};double prices[2]={0,0};std::map<int,long> properties;std::map<int,string> strings;
};
struct ChartStub {
 std::map<string,ChartObject> objects;
 int subwindow=0,width=1000,height=800,render_calls=0,rebuild_calls=0;
 double pointer_price=100;
 bool conversion_ok=true;
 datetime span_left=1000,span_right=2000;
 int span_subwindow=0;
 bool span_left_ok=true,span_right_ok=true;
 int invalid_left_pixels=0,invalid_right_pixels=0,span_queries=0;
 double price_top=110,price_bottom=90;
 bool to_xy_ok=true,font_ok=true;
 int measured_font=10,to_xy_calls=0;
 int temp_calls=0,cas_calls=0,remove_calls=0,timer_kills=0;
 bool create_race=false;
 double race_owner=0;
 std::vector<string> lifecycle;
} CH;
double MathMin(double a,double b) {return std::min(a,b);}
double MathCeil(double x) {return std::ceil(x);}
long StringToInteger(const string &s) {try{return std::stol(s);}catch(...){return 0;}}
void StringTrimLeft(string &s) {s.erase(0,s.find_first_not_of(" \t\r\n"));}
void StringTrimRight(string &s) {size_t last=s.find_last_not_of(" \t\r\n");s.erase(last==string::npos?0:last+1);}
string IntegerToString(int x) {return std::to_string(x);}
bool GlobalVariableCheck(const string &s) {return G.globals.count(s)!=0;}
double GlobalVariableGet(const string &s) {return G.globals.count(s)?G.globals.at(s):0;}
bool GlobalVariableTemp(const string &s) {
 CH.temp_calls++;
 if(G.globals.count(s)) return false; // MT5: existing Temp variable => false, error 4502.
 if(CH.create_race) {CH.create_race=false;G.globals[s]=CH.race_owner;return false;}
 G.globals[s]=0;return true;
}
bool GlobalVariableSetOnCondition(const string &s,double next,double expected) {
 CH.cas_calls++;CH.lifecycle.push_back(next==0?"unlock":"lock");
 auto found=G.globals.find(s);
 if(found==G.globals.end()||found->second!=expected)return false;
 found->second=next;return true;
}
void ExpertRemove() {CH.remove_calls++;CH.lifecycle.push_back("remove");}
void EventKillTimer() {CH.timer_kills++;CH.lifecycle.push_back("timer_kill");}
bool ChartSetInteger(long,int,long) {return true;}
int ObjectFind(long,const string &s) {return CH.objects.count(s)?0:-1;}
int ObjectsTotal(long) {return static_cast<int>(CH.objects.size());}
string ObjectName(long,int index) {auto it=CH.objects.begin();std::advance(it,index);return it->first;}
bool ObjectCreate(long,const string &s,int type,int,datetime time,double price) {auto &o=CH.objects[s];o.type=type;o.price=price;o.times[0]=time;o.prices[0]=price;return true;}
bool ObjectCreate(long,const string &s,int type,int,datetime left,double a,datetime right,double b) {auto &o=CH.objects[s];o.type=type;o.price=a;o.times[0]=left;o.times[1]=right;o.prices[0]=a;o.prices[1]=b;return true;}
bool ObjectSetDouble(long,const string &s,int,double x) {CH.objects[s].price=x;CH.objects[s].prices[0]=x;return true;}
bool ObjectSetInteger(long,const string &s,int prop,long value) {CH.objects[s].properties[prop]=value;if(prop==OBJPROP_SELECTABLE)CH.objects[s].selectable=value;return true;}
bool ObjectSetString(long,const string &s,int prop,const string &x) {CH.objects[s].strings[prop]=x;if(prop==OBJPROP_TEXT)CH.objects[s].text=x;return true;}
bool ObjectMove(long,const string &s,int anchor,datetime time,double price) {if(anchor<0||anchor>1)return false;CH.objects[s].times[anchor]=time;CH.objects[s].prices[anchor]=price;return true;}
bool ObjectDelete(long,const string &s) {return CH.objects.erase(s)>0;}
int ObjectsDeleteAll(long,const string &p) {CH.lifecycle.push_back("delete_objects");int n=0;for(auto i=CH.objects.begin();i!=CH.objects.end();)if(i->first.find(p)==0){i=CH.objects.erase(i);n++;}else ++i;return n;}
double ObjectGetDouble(long,const string &s,int) {return CH.objects.at(s).price;}
string ObjectGetString(long,const string &s,int prop) {return prop==OBJPROP_TEXT?CH.objects.at(s).text:CH.objects.at(s).strings[prop];}
long ChartGetInteger(long,int p,int=0) {return p==CHART_WIDTH_IN_PIXELS?CH.width:CH.height;}
bool ChartXYToTimePrice(long,int x,int y,int &sub,datetime &time,double &price) {
 bool span=(y==CH.height/2);
 if(span)CH.span_queries++;
 sub=span?CH.span_subwindow:CH.subwindow;
 time=CH.span_left+(CH.span_right-CH.span_left)*x/std::max(1,CH.width-1);
 price=CH.pointer_price;
 return CH.conversion_ok&&(!span||(x>=CH.invalid_left_pixels&&x<CH.width-CH.invalid_right_pixels&&(x<CH.width/2?CH.span_left_ok:CH.span_right_ok)));
}
bool ChartTimePriceToXY(long,int,datetime time,double price,int &x,int &y) {
 CH.to_xy_calls++;
 if(!CH.to_xy_ok||CH.span_left==CH.span_right||CH.price_top==CH.price_bottom)return false;
 x=(int)std::round((double)(time-CH.span_left)*(CH.width-1)/(CH.span_right-CH.span_left));
 y=(int)std::round((CH.price_top-price)*(CH.height-1)/(CH.price_top-CH.price_bottom));
 return true;
}
bool TextSetFont(const string&,int size=0,uint=0,int=0) {if(size!=0)CH.measured_font=std::max(1,std::abs(size)/10);return CH.font_ok;}
bool TextGetSize(const string &text,uint &width,uint &height) {
 if(!CH.font_ok)return false;
 // Deliberately synthetic equal-width measurement, never a native-font claim.
 width=(uint)std::ceil(text.size()*CH.measured_font*.6);height=(uint)std::ceil(CH.measured_font*1.4);return true;
}
template<class T,size_t N> void ZeroMemory(T (&values)[N]) {for(auto &value:values)ZeroMemory(value);}
void ChartRedraw() {}
datetime TimeCurrent() {return 1;}
int PeriodSeconds(int) {return 60;}
datetime iTime(const string&,int,int) {return 1;}
// UI layout/painting is intentionally stubbed; actual line/zone methods execute.
// Rendering's market-entry refresh is asserted separately in the Python extraction.
void Render();
void Rebuild();
void RefreshEdits() {}
#include "Drawing.offline.hpp"
void Render() {CH.render_calls++;}
void Rebuild() {CH.rebuild_calls++;Render();}

int checks=0;
int group_failures=0;
void require(bool condition,const string &what) {checks++;if(!condition)throw std::runtime_error(what);}
void near(double a,double b,const string &what) {require(std::abs(a-b)<1e-8,what);}
bool has(const string &id) {return CH.objects.count(N(id))!=0;}
bool sendeligible() {return plan_active&&direction_known&&draw_stage==CS_DRAW_NONE&&tp_ready&&(plan.mode==CS_MARKET||entry_ready);}
void setup() {
 G=StubState{};CH=ChartStub{};
 engine.Configure(_Symbol,InpMagic,10,0);
 plan=CSPlan{};plan.buy=true;plan.mode=CS_MARKET;plan.targets=1;plan.risk_percent=1;
 prefix="CS_test_";notice="";detail="";armed="";armed_until=0;type_confirm_after=0;
 owns_lock=true;instance_key="test_lock";instance_owner=1;G.globals[instance_key]=1;
 plan_active=false;tp_ready=false;entry_ready=false;direction_known=false;draw_stage=CS_DRAW_NONE;
 closing=false;send_consumed=false;mouse_move_changed=false;
 folded=false;busy=false;last_pointer_render=0;last_pointer_ui=0;last_draw_click=0;
 panel_x=12;panel_y=28;panel_width=300;panel_height=500;geometry=1;
 body_page=0;body_pages=1;ui_zoom=1;
 be_enabled=false;
 ZeroMemory(current_quote);ZeroMemory(tag_quote);
 ZeroMemory(tag_visible);ZeroMemory(tag_left);ZeroMemory(tag_top);ZeroMemory(tag_right);ZeroMemory(tag_bottom);ZeroMemory(tag_price);
}
void pointer(double price,bool click=true,int x=700,int y=200) {
 CH.pointer_price=price;G.now+=200;
 OnChartEvent(click?CHARTEVENT_CLICK:CHARTEVENT_MOUSE_MOVE,x,y,"0");
}
void ready() {NewPlan(CS_MARKET);pointer(98);pointer(104);require(sendeligible(),"fixture ready");}
void run(const string &name,void(*fn)()) {
 try {fn();std::cout<<"PASS "<<name<<'\n';}
 catch(const std::exception &e) {group_failures++;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}
}

void idle_and_sl() {
 setup();SyncLines();require(CH.objects.empty(),"idle has no draft lines or zones");
 Action("send");require(armed.empty()&&G.sent.empty(),"idle cannot arm or send");
 NewPlan();require(plan.mode==CS_LIMIT&&plan_active&&draw_stage==CS_DRAW_SL&&!tp_ready&&!direction_known,"new order starts LIMIT SL drawing with automatic direction");
 require(has("line_sl")&&!has("line_entry"),"LIMIT SL preview is visible before entry is selected");
 require(!has("line_tp0")&&!has("line_tp1")&&!has("line_tp2")&&!has("zone_gain"),"new draft has no TP or profit zone");
 require(!CH.objects.at(N("line_sl")).selectable,"SL drawing uses pointer, not object dragging");
 pointer(98.5,false);near(plan.sl,98.5,"SL preview follows pointer");
 require(draw_stage==CS_DRAW_SL&&!sendeligible(),"move cannot commit SL");
 Action("send");require(armed.empty()&&G.sent.empty(),"SL stage cannot arm or send");
}
void sl_tp_ready_send() {
 setup();NewPlan(CS_MARKET);pointer(98);
 require(draw_stage==CS_DRAW_TP&&!tp_ready,"SL click advances only to TP stage");
 near(plan.sl,98,"SL fixed after first click");
 require(has("line_tp0")&&!has("line_tp1")&&!has("line_tp2"),"only TP1 preview appears");
 pointer(103,false);near(plan.tp[0],103,"TP1 preview follows pointer");near(plan.sl,98,"TP preview preserves SL");
 Action("send");require(armed.empty()&&G.sent.empty(),"TP stage cannot arm or send");
 pointer(104);require(sendeligible(),"TP click finishes drawing");
 require(CH.objects.at(N("line_sl")).selectable&&CH.objects.at(N("line_tp0")).selectable,"ready lines can be selected for dragging");
 require(has("zone_gain"),"ready has profit zone");
 double old_sl=plan.sl,old_tp=plan.tp[0];pointer(109,false);pointer(110,true);
 near(plan.sl,old_sl,"ready pointer cannot change SL");near(plan.tp[0],old_tp,"ready pointer cannot change TP");
 Action("send");require(G.sent.size()==1&&send_consumed,"ready single confirm click executes one trade and consumes draft");
 Action("send");require(G.sent.size()==1,"queued second confirm click cannot duplicate the trade");
}
void wrong_side() {
 setup();NewPlan(CS_MARKET);pointer(99.95);require(draw_stage==CS_DRAW_SL&&!tp_ready,"SL inside spread cannot choose direction or advance");
 pointer(98);pointer(99);require(draw_stage==CS_DRAW_TP&&!tp_ready,"buy TP below entry cannot advance");
 pointer(103);require(sendeligible(),"correct TP advances");
 setup();NewPlan(CS_MARKET);pointer(102);require(!plan.buy&&direction_known,"SL above Ask automatically chooses sell");
 pointer(101);require(draw_stage==CS_DRAW_TP,"sell TP above entry cannot advance");
 pointer(97);require(sendeligible(),"correct sell drawing advances");
 setup();G.stops=100;NewPlan(CS_MARKET);pointer(99.5);require(draw_stage==CS_DRAW_SL,"broker min distance enforced");
}
void ignore_regions_and_locks() {
 setup();NewPlan();double old=plan.sl;
 pointer(98,true,100,100);near(plan.sl,old,"panel click ignored");require(draw_stage==CS_DRAW_SL,"panel click does not advance");
 CH.subwindow=1;pointer(98);near(plan.sl,old,"indicator subwindow ignored");require(draw_stage==CS_DRAW_SL,"subwindow does not advance");
 CH.subwindow=0;pointer(98,true,-1,200);near(plan.sl,old,"outside chart ignored");
 CH.conversion_ok=false;pointer(98);near(plan.sl,old,"failed coordinate mapping ignored");
 CH.conversion_ok=true;owns_lock=false;pointer(98);near(plan.sl,old,"ownership lock required for drawing");
 owns_lock=true;folded=true;pointer(98);near(plan.sl,old,"folded drawing ignored");
}
void duplicate_events() {
 setup();NewPlan(CS_MARKET);CH.pointer_price=98;G.now+=200;
 OnChartEvent(CHARTEVENT_CLICK,700,200,"");require(draw_stage==CS_DRAW_TP,"chart click commits SL");
 double preview=plan.tp[0];CH.pointer_price=105;
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,700,200,N("line_sl"));
 require(draw_stage==CS_DRAW_TP&&!tp_ready,"same physical object click cannot also commit TP");
 near(plan.tp[0],preview,"duplicate event cannot move TP preview");
 G.now+=181;OnChartEvent(CHARTEVENT_OBJECT_CLICK,700,200,N("line_tp0"));
 require(sendeligible(),"later object click can commit TP");
}
void cancel_and_switches() {
 setup();NewPlan();pointer(98);OnChartEvent(CHARTEVENT_KEYDOWN,27,0,"");
 require(!plan_active&&!tp_ready&&draw_stage==CS_DRAW_NONE,"ESC cancels unfinished draft");require(CH.objects.empty(),"ESC removes draft objects");
 NewPlan();CancelDraft();require(CH.objects.empty()&&!sendeligible(),"CancelDraft clears all lines and readiness");
 NewPlan();pointer(98);OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("quick_order"));
 require(plan.mode==CS_MARKET&&draw_stage==CS_DRAW_SL&&!direction_known,"QUICK switches to fresh market SL drawing");
 require(!has("line_tp0"),"QUICK removes previous TP preview");
 pointer(102);OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("new_order"));
 require(plan.mode==CS_LIMIT&&draw_stage==CS_DRAW_SL&&!direction_known,"NEW ORDER switches to fresh limit SL drawing");
 require(!has("line_tp0"),"NEW ORDER removes TP preview");
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("discard"));require(!plan_active&&CH.objects.empty(),"discard cancels unfinished drawing");
}
void extras() {
 setup();ToggleTarget(true);require(plan.targets==3&&!tp_ready,"three targets may be selected before drawing");
 NewPlan(CS_MARKET);pointer(98);require(has("line_tp0")&&!has("line_tp1")&&!has("line_tp2"),"extras hidden until TP1 commit");
 pointer(104);require(plan.tp[1]>plan.tp[0]&&plan.tp[2]>plan.tp[1],"automatic buy TP2/3 strictly progress");
 require(has("line_tp1")&&has("line_tp2"),"extras appear after TP1 commit");
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp2"));
 require(plan.targets==1&&!has("line_tp1")&&!has("line_tp2"),"disabling TP2 removes both extra lines");
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp3"));
 require(plan.targets==3&&plan.tp[2]>plan.tp[1]&&plan.tp[1]>plan.tp[0],"reenabling TP3 regenerates both valid extras");
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp3"));
 require(plan.targets==2&&has("line_tp1")&&!has("line_tp2"),"TP3 off removes only third line");
 setup();plan.targets=3;NewPlan(CS_MARKET);pointer(102);pointer(96);
 require(plan.tp[1]<plan.tp[0]&&plan.tp[2]<plan.tp[1],"automatic sell targets strictly progress downward");
 setup();G.margin_mode=ACCOUNT_MARGIN_MODE_RETAIL_NETTING;ToggleTarget(true);require(plan.targets==1,"netting cannot enable extras");
}
void live_and_pending_entries() {
 setup();NewPlan(CS_MARKET);G.tick={100.9,101};SyncLines(true);near(plan.entry,101,"market entry follows current Ask");near(CH.objects.at(N("line_entry")).price,101,"live market entry line updated");
 pointer(99);G.tick={101.9,102};pointer(105);near(plan.entry,102,"market entry refreshed again when TP is committed");
 setup();NewPlan(CS_MARKET);pointer(102);G.tick={98.9,99};SyncLines(true);near(plan.entry,98.9,"sell market entry follows Bid");
 setup();NewPlan();pointer(98);pointer(99);double entry=plan.entry;G.tick={110,110.1};SyncLines(true);near(plan.entry,entry,"pending entry remains fixed as market moves");
}
void automatic_direction_boundaries() {
 setup();bool buy=false;string why;
 require(DirectionForSL(98,buy,why)&&buy,"below Bid chooses BUY");
 require(DirectionForSL(102,buy,why)&&!buy,"above Ask chooses SELL");
 for(double stop:{99.9,99.95,100.}) require(!DirectionForSL(stop,buy,why),"spread and Bid/Ask boundaries cannot choose direction");
 NewPlan(CS_MARKET);pointer(102,false);
 require(!direction_known&&!plan.buy&&draw_stage==CS_DRAW_SL,"preview may suggest sell but does not lock direction");
 pointer(98);require(direction_known&&plan.buy&&draw_stage==CS_DRAW_TP,"SL click locks auto BUY and skips entry stage for MARKET");
 setup();G.valid_tick=false;NewPlan();require(!plan_active,"missing market quote cannot create draft");
 setup();NewPlan(CS_STOP);require(!plan_active,"unsupported STOP mode cannot create draft");
}
void automatic_pending_workflow() {
 struct PendingCase {bool buy;CSOrderMode mode;double sl,entry,tp;ENUM_ORDER_TYPE broker;};
 const std::vector<PendingCase> cases={
  {true,CS_LIMIT,98,99,102,ORDER_TYPE_BUY_LIMIT},
  {true,CS_STOP,100.5,101,103,ORDER_TYPE_BUY_STOP},
  {false,CS_LIMIT,102,101,98,ORDER_TYPE_SELL_LIMIT},
  {false,CS_STOP,99.5,99,97,ORDER_TYPE_SELL_STOP}
 };
 for(const auto &c:cases) {
  setup();OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("new_order"));
  require(draw_stage==CS_DRAW_SL&&!entry_ready&&!direction_known,"NEW ORDER starts unclassified pending SL drawing");
  pointer(c.sl);require(draw_stage==CS_DRAW_ENTRY&&!entry_ready&&!direction_known,"pending SL alone cannot lock BUY/SELL before entry");
  require(has("line_entry")&&!has("line_tp0"),"pending entry preview precedes TP1");
  Action("send");require(G.sent.empty()&&!send_consumed,"unclassified entry stage cannot send or consume draft");
  pointer(c.entry);require(draw_stage==CS_DRAW_TP&&entry_ready&&direction_known,"entry commit classifies pending and advances to TP");
  require(plan.buy==c.buy&&plan.mode==c.mode,"side uses SL versus entry; type uses entry versus live quote");
  require(OrderTypeText().find(c.buy?"BUY":"SELL")!=string::npos&&OrderTypeText().find(c.mode==CS_LIMIT?"LIMIT":"STOP")!=string::npos,"order type text identifies full pending type");
  pointer(c.tp);require(sendeligible(),"pending setup becomes Ready only after SL, Entry and TP");
  Action("send");require(G.sent.size()==1&&G.sent[0].type==c.broker,"single confirm sends exact classified pending order type");
 }
 setup();NewPlan();pointer(99.95);require(draw_stage==CS_DRAW_ENTRY,"pending SL inside spread is allowed before entry");pointer(101);pointer(103);
 require(sendeligible()&&plan.buy&&plan.mode==CS_STOP,"SL inside spread can define valid BUY STOP above it");
 setup();NewPlan();pointer(99.95);pointer(99);pointer(97);
 require(sendeligible()&&!plan.buy&&plan.mode==CS_STOP,"same inside-spread SL can define SELL STOP below it");
 setup();G.stops=10;G.tick={100,100.2};NewPlan();pointer(99.95);pointer(100.05);pointer(101);
 require(sendeligible()&&plan.mode==CS_LIMIT,"near-spread SL supports valid BUY LIMIT when entry gaps satisfy stops");
 CSQuote q{};require(engine.Quote(plan,q),"actual engine accepts near-spread pending geometry");
 setup();bool buy=false;CSOrderMode mode=CS_MARKET;string why;
 for(const auto &c:cases)require(ClassifyPending(c.entry,c.sl,buy,mode,why)&&buy==c.buy&&mode==c.mode,"ClassifyPending agrees with four workflow types");
 require(!ClassifyPending(100,99,buy,mode,why),"BUY entry equal to Ask cannot classify");
 require(!ClassifyPending(99.9,101,buy,mode,why),"SELL entry equal to Bid cannot classify");
 require(!ClassifyPending(101,101,buy,mode,why),"SL equal to entry cannot classify");
 G.stops=10;
 require(!ClassifyPending(100.05,99,buy,mode,why),"BUY STOP too close to Ask is rejected");
 require(!ClassifyPending(99.85,101,buy,mode,why),"SELL STOP too close to Bid is rejected");
 require(!ClassifyPending(101,100.95,buy,mode,why),"entry-SL distance below broker minimum is rejected");
 setup();NewPlan();pointer(98);pointer(100);require(draw_stage==CS_DRAW_ENTRY&&!entry_ready&&!tp_ready,"entry at Ask stays in entry drawing");
}
void crossed_market_and_edit_flip() {
 setup();NewPlan(CS_MARKET);pointer(98);require(plan.buy&&direction_known,"lock BUY");
 G.tick={96.9,97};SyncLines(true);require(plan.buy&&direction_known,"market movement cannot auto-flip locked direction");
 pointer(105);require(draw_stage==CS_DRAW_SL&&!direction_known&&!tp_ready&&plan.buy,"SL crossing during TP confirmation resets to SL, not SELL");
 require(G.sent.empty()&&!has("line_tp0"),"crossed SL cannot place order or leave TP preview");
 setup();ready();require(G.sent.empty(),"ready draft has no automatic trade");
 G.tick={96.9,97};Action("send");
 require(G.sent.empty()&&armed.empty()&&draw_stage==CS_DRAW_SL&&!direction_known&&plan.buy,"market crossing before confirm resets and refuses send without flipping");
 setup();ready();CH.objects[N("line_sl")].price=102;
 OnChartEvent(CHARTEVENT_OBJECT_DRAG,0,0,N("line_sl"));
 require(!plan.buy&&direction_known&&!tp_ready&&draw_stage==CS_DRAW_TP,"dragging SL across market intentionally chooses SELL and redraws MARKET TP");
 require(G.sent.empty()&&armed.empty(),"intentional direction edit cannot execute directly");
 pointer(97);require(sendeligible()&&!plan.buy,"edited MARKET side becomes ready only after new TP click");
 setup();ready();OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("discard"));require(!plan_active&&CH.objects.empty(),"discard button clears completed draft");
}
void lock_reload_and_races() {
 setup();owns_lock=false;G.globals.erase(instance_key);
 require(AcquireInstanceLock()&&owns_lock,"first instance creates and acquires absent lock");
 require(CH.temp_calls==1&&G.globals[instance_key]==instance_owner,"first Temp starts at zero then CAS sets owner");
 ReleaseInstanceLock();require(!owns_lock&&G.globals[instance_key]==0,"normal release clears owner without deleting Temp variable");
 require(!GlobalVariableTemp(instance_key),"existing Temp reproduces MT5 false/4502 behavior");
 int attempts=CH.temp_calls;
 require(AcquireInstanceLock()&&owns_lock,"second attach acquires existing zero Temp despite Temp's existing-variable failure behavior");
 require(CH.temp_calls==attempts,"existing zero lock does not need Temp recreation");
 ReleaseInstanceLock();require(AcquireInstanceLock(),"third reload can acquire same released variable");
 setup();owns_lock=false;G.globals.erase(instance_key);CH.create_race=true;
 require(AcquireInstanceLock()&&owns_lock,"Temp creation race accepts concurrently created zero variable");
 require(CH.temp_calls==1&&G.globals[instance_key]==instance_owner,"creation race still acquires only through CAS");
 setup();owns_lock=false;G.globals.erase(instance_key);CH.create_race=true;CH.race_owner=99;
 require(!AcquireInstanceLock()&&!owns_lock&&G.globals[instance_key]==99,"creation race with active owner refuses acquisition");
 OnDeinit(1);require(G.globals[instance_key]==99,"failed init deinit cannot release competing owner");
 setup();owns_lock=false;G.globals[instance_key]=instance_owner;
 require(!AcquireInstanceLock()&&!owns_lock,"same chart-number owner value cannot be stolen without ownership proof");
 OnDeinit(1);require(G.globals[instance_key]==instance_owner,"failed same-chart acquisition does not release old active instance");
 setup();G.globals[instance_key]=99;ReleaseInstanceLock();
 require(!owns_lock&&G.globals[instance_key]==99,"release CAS cannot clear an owner that changed");
 setup();NewPlan();CH.lifecycle.clear();OnDeinit(1);
 auto unlock=std::find(CH.lifecycle.begin(),CH.lifecycle.end(),"unlock");
 auto deleted=std::find(CH.lifecycle.begin(),CH.lifecycle.end(),"delete_objects");
 require(unlock!=CH.lifecycle.end()&&deleted!=CH.lifecycle.end()&&unlock<deleted,"actual OnDeinit releases lock before synchronous object queue cleanup");
 require(!owns_lock&&G.globals[instance_key]==0,"actual deinit leaves reusable zero lock");
}
void remove_closing_gates() {
 setup();ready();armed="close";armed_until=G.now+8000;
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("remove_ea"));
 require(closing&&CH.remove_calls==1&&CH.timer_kills>=1,"X marks closing, stops timer and requests ExpertRemove");
 require(armed.empty()&&G.sent.empty(),"X disarms without placing, closing or canceling broker orders");
 int renders=CH.render_calls;double oldsl=plan.sl;
 G.positions.push_back(Exposure{});G.tick={102,102.1};be_enabled=true;
 Action("send");Action("close");Action("cancel");
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("send"));
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("quick_order"));
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("remove_ea"));
 OnTick();OnTimer();
 require(G.sent.empty(),"closing gates reject queued trade, close, cancel and management operations");
 require(CH.render_calls==renders,"closing timer/events cannot redraw queued UI");
 near(plan.sl,oldsl,"closing event cannot replace draft");
 require(CH.remove_calls==1,"repeated X cannot request removal twice");
 OnDeinit(1);require(!owns_lock&&G.globals[instance_key]==0,"removal deinit releases owned instance lock");
 setup();NewPlan(CS_MARKET);RemovePanel();double before=plan.sl;pointer(97,false);
 near(plan.sl,before,"closing drawing event cannot move preview");
}
void single_send_and_consumption() {
 setup();ready();OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("send"));
 require(G.sent.size()==1&&send_consumed,"single READY confirm event executes immediately");
 for(int i=0;i<3;i++) OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("send"));
 require(G.sent.size()==1,"rapid queued confirm events cannot duplicate successful send");
 ready();require(!send_consumed,"new setup resets consumption guard");Action("send");
 require(G.sent.size()==2&&send_consumed,"new completed setup can send another explicitly confirmed order");
 setup();ready();G.retcodes={TRADE_RETCODE_DONE_PARTIAL};Action("send");Action("send");
 require(G.sent.size()==1&&send_consumed,"partially filled attempt consumes draft and cannot retry implicitly");
 setup();ready();G.retcodes={TRADE_RETCODE_NO_MONEY};Action("send");Action("send");
 require(G.sent.size()==1&&send_consumed,"server rejected attempt consumes draft and cannot retry implicitly");
 setup();ready();G.fail_check_at=1;Action("send");int checks_after=G.check_calls;Action("send");
 require(G.sent.empty()&&send_consumed&&G.check_calls==checks_after,"Execute preflight failure also consumes attempted setup and blocks repeated execution");
 setup();ready();plan.risk_percent=0;Action("send");
 require(G.sent.empty()&&G.check_calls==0,"invalid quote cannot enter Execute or send orders");
 setup();NewPlan();Action("send");require(G.sent.empty()&&G.check_calls==0,"incomplete setup cannot Execute");
 setup();ready();busy=true;Action("send");require(G.sent.empty(),"busy gate refuses send");
}
void destructive_actions_still_confirm() {
 setup();G.positions.push_back(Exposure{});Action("close");
 require(armed=="close"&&G.sent.empty(),"close positions still needs two clicks");
 Action("close");require(G.sent.size()==1&&G.sent[0].action==TRADE_ACTION_DEAL&&G.sent[0].position==1,"second close click closes owned ticket");
 setup();G.orders.push_back(Exposure{});G.orders[0].type=ORDER_TYPE_BUY_LIMIT;Action("cancel");
 require(armed=="cancel"&&G.sent.empty(),"cancel pending still needs two clicks");
 Action("cancel");require(G.sent.size()==1&&G.sent[0].action==TRADE_ACTION_REMOVE&&G.sent[0].order==1,"second cancel click removes owned pending ticket");
 setup();G.positions.push_back(Exposure{});Action("close");G.now+=8001;Action("close");
 require(G.sent.empty()&&armed=="close","expired close confirmation rearms without executing");
}
void reinitialization_state_regression() {
 // MQL may preserve globals when the chart's timeframe/symbol reinitializes EA.
 for(const string &id:{string("close"),string("cancel")}) {
  setup();ready();G.positions.push_back(Exposure{});
  G.orders.push_back(Exposure{});G.orders.back().type=ORDER_TYPE_BUY_LIMIT;
  Action(id);require(armed==id&&G.sent.empty(),"previous session has an unconfirmed destructive action");
  OnDeinit(3); // Intentionally preserve all remaining C++ globals, like MQL reinit.
  busy=true;send_consumed=true;mouse_move_changed=true;
  last_draw_click=4321;last_pointer_render=5432;last_pointer_ui=6543;
  body_page=4;folded=true;be_enabled=true;
  current_quote.valid=true;current_quote.volume=123;current_quote.error="old quote";
  ResetSessionState();
  require(!closing&&!busy&&!owns_lock&&!send_consumed&&!mouse_move_changed,"session reset clears lifecycle and ownership flags before init");
  require(armed.empty()&&armed_until==0,"old close/cancel confirmation cannot survive initialization");
  require(last_draw_click==0&&last_pointer_render==0&&last_pointer_ui==0,"session reset clears pointer debounce clocks");
  require(body_page==0&&!folded,"session reset restores first open page");
  require(!be_enabled,"session reset keeps BE OFF until explicitly enabled again");
  require(!plan_active&&!tp_ready&&!direction_known&&!entry_ready&&draw_stage==CS_DRAW_NONE,"session reset discards prior drawing/readiness state");
  require(!current_quote.valid&&current_quote.volume==0&&current_quote.error.empty(),"session reset clears prior executable quote");
  require(AcquireInstanceLock(),"reinitialized session can acquire lock released by prior deinit");
  G.tick={102,102.1};OnTick();require(G.sent.empty(),"reinitialized BE OFF cannot manage orders automatically");
  Action(id);require(armed==id&&G.sent.empty(),"first post-init close/cancel click only confirms; it cannot execute old armed request");
 }
}
void removed_management_events_and_tp_toggles() {
 setup();ready();require(!be_enabled,"simplified BE starts OFF");
 CSPlan before=plan;CSDrawStage stage=draw_stage;
 for(const string &id:{string("trail"),string("management_settings"),string("manage_tab"),string("trade_tab"),string("trigger"),string("distance"),string("offset")}) {
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N(id));
  CH.objects[N(id)].text="0.01";
  OnChartEvent(CHARTEVENT_OBJECT_ENDEDIT,0,0,N(id));
  require(!be_enabled&&G.sent.empty(),"removed control events cannot enable management or send orders");
  require(draw_stage==stage&&direction_known&&plan.buy==before.buy,"removed controls cannot change draft state or direction");
  near(plan.entry,before.entry,"legacy fields cannot alter entry");near(plan.sl,before.sl,"legacy fields cannot alter SL");
  near(plan.risk_percent,before.risk_percent,"legacy fields cannot alter risk");
 }
 armed="close";armed_until=G.now+8000;notice="keep status";
 CH.objects[N("unknown_field")].text="999";Commit("unknown_field");
 require(armed=="close"&&notice=="keep status"&&G.sent.empty(),"unknown Commit is ignored before changing confirmation or status");
 for(int drawing=0;drawing<4;drawing++) {
  setup();NewPlan();if(drawing>=1)pointer(98);if(drawing>=2)pointer(99);if(drawing>=3)pointer(103);
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("be"));require(be_enabled&&G.sent.empty(),"BE toggle cannot open a trade in any drawing stage");
  CSPlan snapshot=plan;CSDrawStage old=draw_stage;bool direction=direction_known,ready_before=tp_ready;
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp2"));
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp3"));
  require(plan.targets==3&&be_enabled&&draw_stage==old&&direction_known==direction&&tp_ready==ready_before,"TP2/TP3 enablement preserves BE and drawing state");
  require(plan.buy==snapshot.buy&&G.sent.empty(),"TP toggles preserve direction and do not trade");
  near(plan.entry,snapshot.entry,"TP toggles preserve entry");near(plan.sl,snapshot.sl,"TP toggles preserve SL");near(plan.tp[0],snapshot.tp[0],"TP toggles preserve primary TP");
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp2"));require(plan.targets==1&&be_enabled,"disabling extra TPs preserves BE");
 }
 NewPlan(CS_LIMIT);require(be_enabled,"new LIMIT plan preserves explicitly enabled BE");
 NewPlan(CS_MARKET);require(be_enabled,"new QUICK plan preserves explicitly enabled BE");
 ResetSessionState();require(!be_enabled,"EA reinitialization restores BE OFF");
}
void fixed_break_even_only() {
 setup();ready();OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("be"));OnTick();
 require(be_enabled&&G.sent.empty(),"BE ON without an existing position cannot open a trade");
 G.positions.push_back(Exposure{});G.tick={100.99,101.09};G.now+=1100;OnTick();
 require(G.sent.empty(),"fixed BE does not trigger before initial 1R");
 G.tick={101,101.1};G.now+=1100;OnTick();
 require(G.sent.size()==1&&G.sent[0].action==TRADE_ACTION_SLTP,"fixed BE triggers at initial 1R using SL modification only");
 near(G.sent[0].sl,100,"fixed BE target is exact entry price with zero point offset");near(G.sent[0].tp,110,"BE keeps the broker TP");
 G.positions[0].sl=100; // Simulate broker applying the successful modification.
 G.tick={103,103.1};G.now+=1100;OnTick();
 require(G.sent.size()==1,"BE cannot trail the SL as profit grows beyond 1R");near(G.positions[0].sl,100,"BE remains at entry after a further price rise");
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("be"));require(!be_enabled&&G.sent.size()==1,"BE OFF does not modify an already moved SL");
 G.tick={104,104.1};G.now+=1100;OnTick();near(G.positions[0].sl,100,"BE OFF cannot restore the old wider SL");
 require(G.sent.size()==1,"BE OFF prevents further modifications");
 setup();G.positions.push_back(Exposure{});G.positions[0].type=POSITION_TYPE_SELL;G.positions[0].sl=101;G.positions[0].tp=90;
 G.tick={98.9,99};OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("be"));OnTick();
 require(G.sent.size()==1&&G.sent[0].action==TRADE_ACTION_SLTP,"SELL BE also triggers at fixed initial 1R");near(G.sent[0].sl,100,"SELL BE target is exact entry");
}
void pending_type_reconfirm_and_explicit_edits() {
 struct Change {double sl,entry,tp,bid,ask;CSOrderMode before,after;ENUM_ORDER_TYPE broker;};
 for(const auto &c:std::vector<Change>{
  {98,99,102,98.7,98.8,CS_LIMIT,CS_STOP,ORDER_TYPE_BUY_STOP},
  {100.5,101,103,101.2,101.3,CS_STOP,CS_LIMIT,ORDER_TYPE_BUY_LIMIT},
  {102,101,98,101.2,101.3,CS_LIMIT,CS_STOP,ORDER_TYPE_SELL_STOP},
  {99.5,99,97,98.7,98.8,CS_STOP,CS_LIMIT,ORDER_TYPE_SELL_LIMIT}
 }) {
  setup();NewPlan();pointer(c.sl);pointer(c.entry);pointer(c.tp);
  require(sendeligible()&&plan.mode==c.before,"type-change fixture ready with initial type");
  bool buy=plan.buy;G.tick={c.bid,c.ask};OnTimer();
  require(plan.mode==c.before&&plan.buy==buy&&G.sent.empty(),"Timer cannot silently reclassify reviewed pending type");
  Action("send");require(plan.mode==c.after&&!send_consumed&&G.sent.empty()&&G.check_calls==0,"first confirm after quote crosses entry only updates type");
  require(entry_ready&&tp_ready&&draw_stage==CS_DRAW_NONE&&plan.buy==buy,"type update preserves Entry, TP and direction");
  Action("send");require(G.sent.empty()&&!send_consumed,"immediate queued second click cannot reconfirm updated type");
  G.now+=601;Action("send");
  require(G.sent.size()==1&&G.sent[0].type==c.broker&&send_consumed,"later explicit confirm executes newly reviewed pending type");
 }
 setup();NewPlan();pointer(98);pointer(99);pointer(103);
 CH.objects[N("entry")].text="101";OnChartEvent(CHARTEVENT_OBJECT_ENDEDIT,0,0,N("entry"));
 require(plan.buy&&plan.mode==CS_STOP&&entry_ready&&tp_ready,"explicit same-side Entry edit reclassifies BUY LIMIT to BUY STOP");
 require(draw_stage==CS_DRAW_NONE&&G.sent.empty(),"same-side pending edit preserves Ready without executing");
 CH.objects[N("line_sl")].price=102;OnChartEvent(CHARTEVENT_OBJECT_DRAG,0,0,N("line_sl"));
 require(!plan.buy&&plan.mode==CS_LIMIT&&entry_ready&&!tp_ready&&draw_stage==CS_DRAW_TP,"SL crossing selected Entry reclassifies SELL LIMIT and requires a new TP");
 near(plan.entry,101,"SL direction edit preserves selected Entry");
 pointer(97);require(sendeligible(),"new TP completes explicitly edited SELL setup");
 CH.objects[N("line_entry")].price=99;OnChartEvent(CHARTEVENT_OBJECT_DRAG,0,0,N("line_entry"));
 require(!plan.buy&&plan.mode==CS_STOP&&entry_ready&&tp_ready,"dragging SELL Entry below Bid reclassifies SELL STOP without changing side");
 CH.objects[N("sl")].text="98";OnChartEvent(CHARTEVENT_OBJECT_ENDEDIT,0,0,N("sl"));
 require(plan.buy&&plan.mode==CS_LIMIT&&draw_stage==CS_DRAW_TP&&!tp_ready,"SL edited below Entry changes to BUY and redraws TP");
 pointer(103);require(sendeligible(),"new TP completes second explicit side change");
 CH.objects[N("entry")].text="98";OnChartEvent(CHARTEVENT_OBJECT_ENDEDIT,0,0,N("entry"));
 require(draw_stage==CS_DRAW_ENTRY&&!entry_ready&&!direction_known&&!tp_ready,"invalid equal Entry/SL edit clears readiness and requests Entry again");
 require(!has("line_tp0")&&G.sent.empty(),"invalid edited Entry clears TP preview without sending");
}
void escape_cancels_all_unsubmitted_drafts() {
 auto clean_draft=[]() {
  require(plan_active&&!send_consumed,"ESC fixture is an unsubmitted draft");
  bool old_be=be_enabled;double old_risk=plan.risk_percent;int old_targets=plan.targets;
  G.positions.push_back(Exposure{});G.orders.push_back(Exposure{});G.orders.back().type=ORDER_TYPE_BUY_LIMIT;
  OnChartEvent(CHARTEVENT_KEYDOWN,27,0,"");
  require(!plan_active&&!tp_ready&&!entry_ready&&!direction_known&&draw_stage==CS_DRAW_NONE,"ESC clears readiness at every unsubmitted stage including Ready");
  near(plan.entry,0,"ESC clears draft Entry");near(plan.sl,0,"ESC clears draft SL");
  for(int i=0;i<3;i++)near(plan.tp[i],0,"ESC clears all draft TP values");
  for(const auto &object:CH.objects)
   require(object.first.find(N("line_"))!=0&&object.first.find(N("zone_"))!=0,"ESC removes every draft line and zone");
  require(armed.empty()&&armed_until==0&&type_confirm_after==0,"ESC clears confirmation and pending-type reconfirmation delay");
  require(be_enabled==old_be&&plan.risk_percent==old_risk&&plan.targets==old_targets,"ESC preserves BE, risk and target-count preferences");
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("send"));
  require(G.sent.empty()&&G.check_calls==0&&!send_consumed,"queued send after ESC cannot touch broker or consume a canceled setup");
  require(G.positions.size()==1&&G.orders.size()==1,"ESC cannot close existing positions or cancel broker orders");
 };
 // SL, Entry and TP stages across pending/market flows.
 for(int flow=0;flow<2;flow++) {
  int unfinished=flow==0?3:2;
  for(int stage=0;stage<unfinished;stage++) {
   setup();plan.targets=3;plan.risk_percent=.73;be_enabled=true;NewPlan(flow==0?CS_LIMIT:CS_MARKET);
   if(stage>=1)pointer(98);if(flow==0&&stage>=2)pointer(99);
   clean_draft();
  }
 }
 // All four pending types and both market sides with TP2/3 lines and zones.
 struct ReadyCase {CSOrderMode start;double sl,entry,tp;};
 for(const auto &c:std::vector<ReadyCase>{
  {CS_LIMIT,98,99,103},{CS_LIMIT,100.5,101,103},
  {CS_LIMIT,102,101,97},{CS_LIMIT,99.5,99,97},
  {CS_MARKET,98,0,104},{CS_MARKET,102,0,97}
 }) {
  setup();plan.targets=3;plan.risk_percent=.73;be_enabled=true;NewPlan(c.start);pointer(c.sl);
  if(c.start!=CS_MARKET)pointer(c.entry);pointer(c.tp);
  require(sendeligible()&&has("line_tp1")&&has("line_tp2")&&has("zone_gain"),"Ready ESC fixture includes TP2/3 and gain zone");
  clean_draft();
 }
 setup();NewPlan();pointer(98);pointer(99);pointer(103);G.tick={98.7,98.8};Action("send");
 require(type_confirm_after>G.now&&!send_consumed&&G.sent.empty(),"type-change wait is still an unsubmitted draft");
 clean_draft();
 setup();OnChartEvent(CHARTEVENT_KEYDOWN,27,0,"");require(!plan_active&&G.sent.empty(),"idle ESC cannot touch broker");
}
void escape_preserves_submitted_results_and_disarms_actions() {
 for(uint code:{static_cast<uint>(TRADE_RETCODE_DONE),static_cast<uint>(TRADE_RETCODE_NO_MONEY),static_cast<uint>(TRADE_RETCODE_DONE_PARTIAL),0u}) {
  setup();ready();if(code==0)G.fail_check_at=1;else G.retcodes={code};Action("send");
  require(send_consumed,"submitted success/failure attempt is consumed before ESC");
  string status=notice,extra=detail;double sl=plan.sl,entry=plan.entry;size_t objects=CH.objects.size(),sent=G.sent.size();int checked=G.check_calls;
  bool active=plan_active;CSDrawStage stage=draw_stage;armed="close";armed_until=G.now+8000;
  OnChartEvent(CHARTEVENT_KEYDOWN,27,0,"");
  require(send_consumed&&plan_active==active&&draw_stage==stage,"ESC cannot clear submitted result or reopen its submission guard");
  require(notice==status&&detail==extra&&CH.objects.size()==objects,"ESC preserves submitted result message and draft display");
  near(plan.sl,sl,"ESC preserves submitted SL display");near(plan.entry,entry,"ESC preserves submitted Entry display");
  require(armed.empty()&&armed_until==0,"ESC still disarms pending close/cancel confirmation after submission");
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("send"));
  require(G.sent.size()==sent&&G.check_calls==checked,"queued confirm after ESC cannot duplicate submitted success or failure attempt");
 }
 for(const string &action:{string("close"),string("cancel")}) {
  setup();G.positions.push_back(Exposure{});G.orders.push_back(Exposure{});G.orders.back().type=ORDER_TYPE_BUY_LIMIT;
  Action(action);require(armed==action&&G.sent.empty(),"close/cancel fixture awaits second confirmation");
  OnChartEvent(CHARTEVENT_KEYDOWN,27,0,"");require(armed.empty()&&G.sent.empty(),"idle ESC only disarms close/cancel request");
  Action(action);require(armed==action&&G.sent.empty(),"next close/cancel click after ESC is a new first confirmation");
 }
 setup();ready();busy=true;armed="close";OnChartEvent(CHARTEVENT_KEYDOWN,27,0,"");
 require(plan_active&&tp_ready&&busy&&armed.empty()&&G.sent.empty(),"busy ESC disarms without deleting a setup involved in an in-progress action");
}
void require_zone_span(const string &id,double a,double b) {
 require(has("zone_"+id),"expected preview zone exists");
 auto &z=CH.objects.at(N("zone_"+id));
 datetime first=CH.span_left+(CH.span_right-CH.span_left)*CH.invalid_left_pixels/std::max(1,CH.width-1);
 datetime last=CH.span_left+(CH.span_right-CH.span_left)*(CH.width-1-CH.invalid_right_pixels)/std::max(1,CH.width-1);
 require(z.type==OBJ_RECTANGLE&&z.times[0]==std::min(first,last)&&z.times[1]==std::max(first,last),"zone anchors cover exact valid plot-edge times including border fallback");
 near(std::min(z.prices[0],z.prices[1]),std::min(a,b),"zone spans lower draft price");
 near(std::max(z.prices[0],z.prices[1]),std::max(a,b),"zone spans upper draft price");
 require(z.properties[OBJPROP_BACK]&&z.properties[OBJPROP_FILL]&&!z.selectable&&!z.properties[OBJPROP_SELECTABLE]&&!z.properties[OBJPROP_SELECTED],"zone is filled behind candles and cannot intercept dragging");
 require(z.properties[OBJPROP_ZORDER]==0&&z.strings[OBJPROP_TOOLTIP]=="\n","zone stays below draft lines and suppresses its tooltip");
}
void viewport_zone_coordinates_and_failures() {
 setup();ready();require_zone_span("loss",plan.entry,plan.sl);require_zone_span("gain",plan.entry,plan.tp[0]);
 CH.span_queries=0;SyncLines();require(CH.span_queries==2,"both risk and gain reuse one normal-edge VisibleSpan snapshot");
 CH.span_left=500;CH.span_right=1700;OnChartEvent(CHARTEVENT_CHART_CHANGE,0,0,"");
 require_zone_span("loss",plan.entry,plan.sl);require_zone_span("gain",plan.entry,plan.tp[0]);
 CH.width=1423;CH.height=676;CH.span_left=750;CH.span_right=2600;
 OnChartEvent(CHARTEVENT_CHART_CHANGE,0,0,"");require_zone_span("gain",plan.entry,plan.tp[0]);
 require(G.sent.empty(),"scroll and resize update zones without broker actions");
 CH.invalid_left_pixels=11;CH.invalid_right_pixels=27;
 OnChartEvent(CHARTEVENT_CHART_CHANGE,0,0,"");require_zone_span("loss",plan.entry,plan.sl);require_zone_span("gain",plan.entry,plan.tp[0]);
 require(CH.objects.at(N("line_sl")).properties[OBJPROP_ZORDER]==10,"draft SL has higher hit-test order than shaded bands");
 CH.invalid_left_pixels=0;CH.invalid_right_pixels=0;
 for(int failure=0;failure<5;failure++) {
  CH.width=1423;CH.height=676;CH.span_left=1000;CH.span_right=2000;CH.conversion_ok=true;CH.span_subwindow=0;
  OnChartEvent(CHARTEVENT_CHART_CHANGE,0,0,"");require(has("zone_loss")&&has("zone_gain"),"valid viewport restores both zones");
  if(failure==0)CH.conversion_ok=false;
  if(failure==1)CH.span_subwindow=1;
  if(failure==2)CH.span_right=CH.span_left;
  if(failure==3)CH.width=1;
  if(failure==4)CH.height=1;
  OnChartEvent(CHARTEVENT_CHART_CHANGE,0,0,"");
  require(!has("zone_loss")&&!has("zone_gain"),"entirely failed/non-main/zero-time/tiny viewport deletes stale zones immediately");
 }
 CH.width=1423;CH.height=676;CH.conversion_ok=true;CH.span_subwindow=0;CH.span_left=2000;CH.span_right=1000;
 OnChartEvent(CHARTEVENT_CHART_CHANGE,0,0,"");require_zone_span("loss",plan.entry,plan.sl);
 CH.span_left=1200;CH.span_right=3200;OnTimer();require_zone_span("loss",plan.entry,plan.sl);require_zone_span("gain",plan.entry,plan.tp[0]);
 double old_pointer=CH.pointer_price;datetime left=0,right=0;
 require(VisibleSpan(left,right)&&left==CH.span_left&&right==CH.span_right,"VisibleSpan returns both exact chart-edge times");
 near(CH.pointer_price,old_pointer,"viewport coordinate conversion cannot alter pointer-price stub state");
}
void zones_during_drawing_and_farthest_targets() {
 setup();plan.targets=3;NewPlan();require(draw_stage==CS_DRAW_SL&&plan.entry==0,"pending zone fixture starts before Entry");
 require_zone_span("loss",LiveEntry(),plan.sl);require(!has("zone_gain"),"SL preview has risk band and no profit band");
 pointer(98,false);require_zone_span("loss",LiveEntry(),98);
 pointer(98);require(draw_stage==CS_DRAW_ENTRY,"pending SL commit advances to Entry preview");
 require_zone_span("loss",plan.entry,plan.sl);require(!has("zone_gain"),"Entry preview has risk band and no profit band");
 pointer(99);require(draw_stage==CS_DRAW_TP,"Entry commit advances to TP preview");
 require_zone_span("loss",plan.entry,plan.sl);require_zone_span("gain",plan.entry,plan.tp[0]);
 auto ink=CH.objects.at(N("zone_gain")).properties[OBJPROP_COLOR];
 require(((ink>>16)&255)>(ink&255)&&((ink>>16)&255)>((ink>>8)&255),"profit preview uses a blue fill");
 pointer(103,false);require_zone_span("gain",plan.entry,103);
 pointer(103);require(sendeligible()&&plan.targets==3,"ready zone fixture includes TP2 and TP3");
 require_zone_span("gain",plan.entry,plan.tp[2]);
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp3"));require_zone_span("gain",plan.entry,plan.tp[1]);
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp2"));require_zone_span("gain",plan.entry,plan.tp[0]);
 OnChartEvent(CHARTEVENT_KEYDOWN,27,0,"");require(!has("zone_loss")&&!has("zone_gain"),"ESC removes full-viewport risk and profit bands");
 setup();NewPlan(CS_MARKET);require_zone_span("loss",LiveEntry(),plan.sl);pointer(102);require(!plan.buy&&draw_stage==CS_DRAW_TP,"SELL market preview fixture");
 require_zone_span("loss",plan.entry,plan.sl);require_zone_span("gain",plan.entry,plan.tp[0]);
 CancelDraft();require(!has("zone_loss")&&!has("zone_gain")&&G.sent.empty(),"Cancel removes preview bands without a broker action");
}
void tag_cash_data_and_validity() {
 for(bool buy:{true,false})for(int targets:{2,3}) {
  setup();G.min=.25;G.step=.25;engine.Configure(_Symbol,InpMagic,10,5);
  plan.buy=buy;plan.mode=CS_MARKET;plan.targets=targets;plan.risk_percent=1.4;
  plan.entry=buy?G.tick.ask:G.tick.bid;double sign=buy?1.:-1.;plan.sl=plan.entry-sign;
  for(int i=0;i<targets;i++)plan.tp[i]=plan.entry+sign*(2+2*i);
  plan_active=true;direction_known=true;entry_ready=true;tp_ready=true;draw_stage=CS_DRAW_NONE;SyncLines();
  CSQuote q{};require(DisplayQuote(q)&&q.valid,"tag fixture obtains real BUY/SELL split cash forecast");
  string full,compact;TagText("entry",plan.entry,q,full,compact);
  require(full.find(PriceText(plan.entry))!=string::npos&&full.find(LotsText(q.volume)+" lots")!=string::npos,"Entry tag has exact price and total lots");
  require(full.find("Est. SL "+SignedCash(-q.risk_cash)+" / TP "+SignedCash(q.reward_cash)+" USD")!=string::npos,"Entry tag summarizes total expected loss and reward");
  TagText("sl",plan.sl,q,full,compact);
  require(full.find(LotsText(q.volume)+" lots")!=string::npos&&full.find(SignedCash(-q.risk_cash)+" USD")!=string::npos,"SL tag displays total lots and negative total risk");
  for(int i=0;i<targets;i++) {
   string id="tp"+IntegerToString(i);TagText(id,plan.tp[i],q,full,compact);
   require(full.find(PriceText(plan.tp[i]))!=string::npos&&full.find(LotsText(q.leg_volume[i])+" lots")!=string::npos,"TP tag uses that target's price and unequal split volume");
   require(full.find(SignedCash(q.leg_reward_cash[i])+" USD")!=string::npos,"TP tag uses its own fee-adjusted reward");
   require(CH.objects.at(N("tag_"+id)).strings[OBJPROP_TOOLTIP].find(full)!=string::npos,"right label tooltip preserves full target forecast");
  }
  require(CH.objects.at(N("line_sl")).text.empty(),"native HLINE text is empty to avoid right-label duplication");
  G.free_margin=1;require(!DisplayQuote(q)&&!q.valid,"margin failure makes display forecast invalid");
  TagText("sl",plan.sl,q,full,compact);require(full.find("-- lots | Est. -- USD")!=string::npos,"invalid forecast cannot show partially calculated SL amounts");
  TagText("entry",plan.entry,q,full,compact);require(full.find("SL -- / TP -- USD")!=string::npos,"invalid Entry forecast hides both aggregate monetary amounts");
 }
}
void tag_preview_quotes_and_frozen_submission() {
 setup();plan.targets=3;NewPlan();CSQuote q{};require(!DisplayQuote(q)&&!q.valid,"pending SL before Entry has no lots/cash estimate");
 string full,compact;TagText("sl",plan.sl,q,full,compact);require(full.find("-- lots")!=string::npos,"initial pending SL label shows unknown volume");
 pointer(98);CSPlan before=plan;bool direction=direction_known,entry_done=entry_ready;CSDrawStage stage=draw_stage;
 G.tick={98.7,98.8};require(DisplayQuote(q)&&q.valid,"ENTRY preview classifies only a local plan for an estimate");
 require(plan.mode==before.mode&&plan.buy==before.buy&&direction_known==direction&&entry_ready==entry_done&&draw_stage==stage,"display-only classification cannot change actual direction, order type or drawing state");
 near(plan.entry,before.entry,"display forecast cannot move pending Entry");near(plan.tp[0],0,"synthetic TP used for size cannot enter actual draft");
 TagText("entry",plan.entry,q,full,compact);require(full.find(" / TP -- USD")!=string::npos,"synthetic targets cannot appear as aggregate Entry reward before TP confirmation");
 pointer(99);pointer(100.5,false);require(draw_stage==CS_DRAW_TP&&DisplayQuote(q),"actual TP preview obtains cash estimate");
 near(q.leg_reward_cash[0],(100.5-plan.entry)*G.multiplier*q.leg_volume[0],"visible TP0 cash uses actual pointer TP rather than synthetic reward multiple");
 setup();NewPlan(CS_MARKET);plan.sl=102;before=plan;
 require(!direction_known&&plan.buy&&DisplayQuote(q),"Quick SL preview can estimate locally inferred SELL");
 require(plan.buy==before.buy&&!direction_known&&draw_stage==CS_DRAW_SL,"Quick display-only side inference cannot lock actual draft direction");
 near(q.entry,G.tick.bid,"local SELL preview sizes at Bid");
 setup();ready();Action("send");require(send_consumed&&tag_quote.valid,"submitted forecast is frozen before broker request");
 CSQuote frozen=tag_quote;double entry=plan.entry,sl=plan.sl,tp=plan.tp[0];G.tick={105,105.1};OnTimer();
 require(DisplayQuote(q)&&q.valid,"consumed setup keeps its frozen estimate");
 near(q.entry,frozen.entry,"frozen estimate Entry stays unchanged");near(q.volume,frozen.volume,"frozen forecast volume stays unchanged");near(q.risk_cash,frozen.risk_cash,"frozen risk stays unchanged");near(q.reward_cash,frozen.reward_cash,"frozen reward stays unchanged");
 near(plan.entry,entry,"submitted market Entry line does not become a new live quote");
 require_zone_span("loss",entry,sl);require_zone_span("gain",entry,tp);near(tag_price[0],entry,"submitted Entry tag price stays frozen");
 require(CH.objects.at(N("tag_entry")).strings[OBJPROP_TOOLTIP].find("not current floating P/L")!=string::npos,"forecast tooltip distinguishes it from current floating P/L");
 NewPlan();require(!send_consumed&&!tag_quote.valid,"new setup clears submitted forecast cache");
}
void tag_layout_fallback_and_cleanup() {
 setup();ready();CSQuote q{};require(DisplayQuote(q),"layout fixture quote");
 string full,compact;TagText("entry",plan.entry,q,full,compact);int fw=TextWidth(full,8),cw=TextWidth(compact,8);
 require(fw>cw,"full forecast is wider than compact alternative under synthetic font metrics");
 int left_limit=panel_x+Px(panel_width)+Px(8);
 CH.width=left_limit+9+(fw+cw)/2;SyncTags(CH.span_right);
 require(tag_visible[0]&&CH.objects.at(N("tag_entry")).text==compact,"label falls back to compact forecast when full form cannot fit");
 string price="Entry "+PriceText(plan.entry);int pw=TextWidth(price,8);
 CH.width=left_limit+9+(cw+pw)/2;SyncTags(CH.span_right);
 require(tag_visible[0]&&CH.objects.at(N("tag_entry")).text==price,"label falls back to exact price when compact cash forecast cannot fit");
 CH.width=left_limit+9+pw-1;SyncTags(CH.span_right);require(!tag_visible[0]&&!has("tag_entry"),"label hides when even exact price would collide with panel");
 setup();plan.targets=3;NewPlan(CS_MARKET);pointer(99.89);pointer(100.01);
 plan.tp[1]=100.02;plan.tp[2]=100.03;SyncLines();std::vector<std::pair<int,int>> bounds;
 for(int i=0;i<5;i++) {
  require(tag_visible[i],"nearby Entry/SL/TP levels all have visible stacked labels");
  require(tag_left[i]>=0&&tag_right[i]<CH.width&&tag_top[i]>=0&&tag_bottom[i]<CH.height,"tag bounding box is clipped within chart");
  require(tag_left[i]>=panel_x+Px(panel_width)||tag_bottom[i]<panel_y||tag_top[i]>panel_y+Px(panel_height),"tag cannot overlap control panel");
  bounds.push_back({tag_top[i],tag_bottom[i]});
 }
 std::sort(bounds.begin(),bounds.end());for(size_t i=1;i<bounds.size();i++)require(bounds[i].first>=bounds[i-1].second+2,"nearby tags are separated rather than printed over one another");
 CH.objects[N("background")].text="UI";DeleteBody();require(!has("background")&&has("tag_entry")&&has("line_sl")&&has("zone_loss"),"body redraw preserves labels, lines and zones");
 CH.to_xy_ok=false;SyncTags(CH.span_right);for(int i=0;i<5;i++)require(!tag_visible[i],"failed price-to-pixel conversion clears stale tag hit boxes");
 require(!has("tag_entry")&&!has("tag_sl")&&!has("tag_tp0"),"failed conversion removes stale right-side labels");
 CH.to_xy_ok=true;CH.price_top=90;CH.price_bottom=80;SyncTags(CH.span_right);require(!has("tag_entry")&&!has("tag_sl"),"prices outside viewport cannot leave stale labels");
 CH.price_top=110;CH.price_bottom=90;SyncTags(CH.span_right);require(has("tag_entry"),"tags return when prices are visible again");
 CancelDraft();for(int i=0;i<5;i++)require(!tag_visible[i],"draft cancellation clears tag hit boxes");require(!has("tag_entry")&&!has("tag_sl")&&!has("tag_tp0"),"draft cancellation removes forecast labels");
 require(G.sent.empty(),"tag layout and cleanup cannot cause broker actions");
}
void tag_click_uses_displayed_price() {
 setup();NewPlan(CS_MARKET);pointer(98,false);require(tag_visible[1],"active SL preview tag is visible");
 int x=(tag_left[1]+tag_right[1])/2,y=(tag_top[1]+tag_bottom[1])/2;double exact=tag_price[1];
 CH.pointer_price=999;G.now+=200;OnChartEvent(CHARTEVENT_MOUSE_MOVE,x,y,"0");
 near(plan.sl,exact,"moving onto displaced SL tag cannot move the candidate line away");
 require(tag_visible[1]&&x>=tag_left[1]&&x<=tag_right[1]&&y>=tag_top[1]&&y<=tag_bottom[1],"active SL label stays beneath pointer until click");
 OnChartEvent(CHARTEVENT_CLICK,x,y,"");
 near(plan.sl,exact,"clicking offset SL label commits its displayed exact price, not chart-y price");
 require(draw_stage==CS_DRAW_TP&&plan.buy,"SL tag click advances correct BUY drawing stage");
 double tp=plan.tp[0];OnChartEvent(CHARTEVENT_OBJECT_CLICK,x,y,N("tag_sl"));near(plan.tp[0],tp,"queued object click after label click cannot alter TP preview");
 G.now+=200;x=(tag_left[1]+tag_right[1])/2;y=(tag_top[1]+tag_bottom[1])/2;CH.pointer_price=104;
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,x,y,N("tag_sl"));require(draw_stage==CS_DRAW_TP&&!tp_ready,"clicking a fixed SL label cannot commit active TP stage");
 require(tag_visible[2],"active TP tag remains visible");x=(tag_left[2]+tag_right[2])/2;y=(tag_top[2]+tag_bottom[2])/2;exact=tag_price[2];CH.pointer_price=90;G.now+=200;
 OnChartEvent(CHARTEVENT_MOUSE_MOVE,x,y,"0");near(plan.tp[0],exact,"moving onto offset TP tag preserves its displayed candidate price");
 OnChartEvent(CHARTEVENT_OBJECT_CLICK,x,y,N("tag_tp0"));near(plan.tp[0],exact,"offset TP tag click commits displayed target price");require(sendeligible()&&G.sent.empty(),"TP tag click finishes drawing without sending a trade");
 OnChartEvent(CHARTEVENT_KEYDOWN,27,0,"");require(!has("tag_sl")&&!has("tag_tp0")&&!tag_visible[1]&&!tag_visible[2],"ESC removes labels and click metadata");
}
void submitted_target_tags_remain_frozen() {
 for(int targets:{1,3}) {
  setup();plan.targets=targets;ready();Action("send");CSPlan frozen=plan;CSQuote quote=tag_quote;size_t submitted_requests=G.sent.size();
  require(send_consumed&&quote.valid,"submitted target fixture has frozen forecast");
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp2"));
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp3"));
  require(plan.targets==frozen.targets,"consumed setup cannot add or remove targets from frozen quote");
  for(int i=0;i<3;i++)near(plan.tp[i],frozen.tp[i],"consumed target toggles preserve displayed TP prices");
  CSQuote shown{};require(DisplayQuote(shown),"submitted target forecast remains available");
  for(int i=0;i<targets;i++){near(shown.leg_volume[i],quote.leg_volume[i],"consumed target toggle preserves leg lots");near(shown.leg_reward_cash[i],quote.leg_reward_cash[i],"consumed target toggle preserves leg reward");}
  for(int i=0;i<3;i++)require(tag_visible[i+2]==(i<targets),"submitted labels retain exactly the originally quoted target count");
  OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("be"));require(be_enabled,"BE remains available while submitted target estimates stay fixed");
  NewPlan();OnChartEvent(CHARTEVENT_OBJECT_CLICK,0,0,N("toggle_tp2"));
  require(!send_consumed&&plan.targets!=targets,"new setup unlocks target-count editing");
  require(G.sent.size()==submitted_requests,"target freeze guards and new setup cannot submit additional broker requests");
 }
}
int main() {
 try {
  run("idle / New Order SL-only preview",idle_and_sl);
  run("SL -> TP1 -> ready / single send / ready pointer frozen",sl_tp_ready_send);
  run("spread rejection, wrong-side targets and minimum stops",wrong_side);
  run("panel / subwindow / chart bounds / lock guards",ignore_regions_and_locks);
  run("CLICK + OBJECT_CLICK deduplication",duplicate_events);
  run("ESC / Cancel / NEW ORDER / QUICK transitions",cancel_and_switches);
  run("automatic TP2/3 ordering / toggles / no residual lines",extras);
  run("live Bid/Ask entry / pending entry stability",live_and_pending_entries);
  run("automatic BUY/SELL and spread boundary decisions",automatic_direction_boundaries);
  run("automatic BUY/SELL LIMIT/STOP workflow and pending stop distances",automatic_pending_workflow);
  run("QUICK market crossing resets without flip / intentional SL edit redraws",crossed_market_and_edit_flip);
  run("reload-safe Temp variable lock / races / release-before-cleanup lifecycle",lock_reload_and_races);
  run("X removal / closing gates / no broker operations",remove_closing_gates);
  run("single confirm / consumed attempts / queued duplicate suppression",single_send_and_consumption);
  run("close and cancel retain two-click confirmation",destructive_actions_still_confirm);
  run("reinitialization resets preserved globals and destructive confirmations",reinitialization_state_regression);
  run("removed controls ignored / TP toggles preserve BE and drawing",removed_management_events_and_tp_toggles);
  run("fixed 1R / zero-offset break-even without trailing",fixed_break_even_only);
  run("pending type changes require reconfirmation / explicit edits reclassify",pending_type_reconfirm_and_explicit_edits);
  run("ESC cancels every unsubmitted stage and all Ready order types",escape_cancels_all_unsubmitted_drafts);
  run("ESC preserves submitted results and disarms destructive confirmations",escape_preserves_submitted_results_and_disarms_actions);
  run("full-viewport zone anchors / scroll-resize updates / failure cleanup",viewport_zone_coordinates_and_failures);
  run("risk and blue profit preview bands / farthest target / cancellation",zones_during_drawing_and_farthest_targets);
  run("per-target right-label cash data / fees / invalid quote guards",tag_cash_data_and_validity);
  run("display-only preview quotes / synthetic TP hiding / frozen forecast",tag_preview_quotes_and_frozen_submission);
  run("label compact fallback / clipping / stacking / cleanup",tag_layout_fallback_and_cleanup);
  run("offset label click exact-price commit and event deduplication",tag_click_uses_displayed_price);
  run("submitted target-count and per-leg tag estimates remain frozen",submitted_target_tags_remain_frozen);
  if(group_failures) {std::cerr<<group_failures<<" drawing group(s) failed.\n";return 1;}
  std::cout<<"SUCCESS: "<<checks<<" assertions across 28 drawing groups.\n";
 } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
