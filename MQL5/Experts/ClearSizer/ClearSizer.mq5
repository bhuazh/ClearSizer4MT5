#property copyright "ClearSizer - original source, 2026"
#property version   "1.80"
#property description "Manual risk-sizing panel. Draw the setup, then click Confirm Order once."
#property strict

#include "SizerTrade.mqh"

input group "Panel"
input double InpUIScale=1.00;       // Size multiplier; geometry also follows system DPI
input double InpDpiOverride=0;      // 0=terminal DPI; try 96 if Wine reports wrong DPI
input string InpFont="Arial";
input int    InpPanelX=12;
input int    InpPanelY=28;
input group "Order defaults"
input double InpRiskPercent=1.0;
input bool   InpUseEquity=true;
input double InpStopPoints=300;
input double InpRewardR=2.0;
input ulong  InpMagic=2601029;      // Use a different ID for each independently managed panel
input ulong  InpDeviationPoints=20;
input double InpCostPerLot=0;       // Estimated round-trip fees, account currency per lot

CSizerTrade engine;
CSPlan plan;
CSQuote current_quote;
CSQuote tag_quote;
bool tag_visible[5];
int tag_left[5],tag_top[5],tag_right[5],tag_bottom[5];
double tag_price[5];
string prefix="";
string notice="Draw SL / TP, then click CONFIRM ORDER.";
string detail="";
const int UI_WIDTH=300,UI_BODY_TOP=50,UI_ROW=28;
int body_rows=0;
enum CSDrawStage { CS_DRAW_NONE=0,CS_DRAW_SL=1,CS_DRAW_TP=2,CS_DRAW_ENTRY=3 };
CSDrawStage draw_stage=CS_DRAW_NONE;
bool old_mouse_move=false,mouse_move_changed=false;
ulong last_pointer_render=0,last_draw_click=0,last_pointer_ui=0;
ulong type_confirm_after=0;
bool tp_ready=false,direction_known=false,entry_ready=false;
bool folded=false,be_enabled=false;
bool busy=false,plan_active=false,owns_lock=false,chart_too_small=false;
bool closing=false,send_consumed=false;
bool quote_status=true;
string armed="";
ulong armed_until=0;
double ui_zoom=1.0;
double geometry=1.0,effective_dpi=96,font_zoom=1.0;
int panel_x=12,panel_y=28,body_page=0,body_pages=1,body_capacity=320;
int panel_width=380,panel_height=514,viewport_height=320;
string instance_key="";
double instance_owner=0;
color C_BG=C'20,27,38',C_CARD=C'30,40,55',C_BORDER=C'53,68,86';
color C_TEXT=C'234,240,247',C_MUTED=C'147,164,185',C_ACCENT=C'57,167,245';
color C_BUY=C'34,169,130',C_SELL=C'226,91,110',C_WARN=C'245,183,80';

int Px(const double logical) { return (int)MathRound(logical*geometry); }
int FontSize(const int base=10) { return (int)MathMax(6,MathRound(base*font_zoom)); }
string N(const string suffix) { return prefix+suffix; }
string PriceText(const double price) { return DoubleToString(price,_Digits); }
string ModeText() { if(plan.mode==CS_MARKET) return "MARKET"; if(!entry_ready) return "PENDING"; return plan.mode==CS_STOP ? "STOP" : "LIMIT"; }
string OrderTypeText() { return (plan.buy ? "BUY " : "SELL ")+ModeText(); }
double LiveEntry()
  {
   MqlTick tick={};
   if(!SymbolInfoTick(_Symbol,tick)) return plan.entry;
   return engine.Price(plan.buy ? tick.ask : tick.bid);
  }
void Disarm() { armed=""; armed_until=0; }
void Say(const string text,const string extra="") { notice=text; detail=extra; quote_status=false; }
string Base36(ulong value)
  {
   string digits="0123456789abcdefghijklmnopqrstuvwxyz",out="";
   do { out=StringSubstr(digits,(int)(value%36),1)+out; value/=36; } while(value>0);
   return out;
  }
uint HashText(const string text)
  {
   uint hash=2166136261;
   for(int i=0;i<StringLen(text);i++) { hash^=(uint)StringGetCharacter(text,i); hash*=16777619; }
   return hash;
  }
bool HaveLock()
  {
   return !closing && owns_lock && GlobalVariableCheck(instance_key) && GlobalVariableGet(instance_key)==instance_owner;
  }
bool AcquireInstanceLock()
  {
   owns_lock=false;
   // Temp returns false if the variable already exists. An existing zero value is reusable.
   // Another chart may create it between Check and Temp, so recheck before reporting an error.
   if(!GlobalVariableCheck(instance_key) && !GlobalVariableTemp(instance_key) && !GlobalVariableCheck(instance_key))
     { Print("ClearSizer: cannot create instance lock. Error ",GetLastError()); return false; }
   if(!GlobalVariableSetOnCondition(instance_key,instance_owner,0))
     { Print("ClearSizer: this symbol / ID is already owned by another panel. Error ",GetLastError()); return false; }
   owns_lock=true;
   return true;
  }
void ReleaseInstanceLock()
  {
   if(owns_lock && instance_key!="")
     {
      if(!GlobalVariableSetOnCondition(instance_key,0,instance_owner))
         Print("ClearSizer: instance lock release failed or ownership changed. Error ",GetLastError());
     }
   owns_lock=false;
  }
void RemovePanel()
  {
   if(closing || busy) return;
   closing=true;
   Disarm();
   EventKillTimer();
   ExpertRemove();
  }
void ResetSessionState()
  {
   closing=false; busy=false; owns_lock=false; send_consumed=false;
   mouse_move_changed=false;
   Disarm();
   last_draw_click=0; last_pointer_render=0; last_pointer_ui=0; type_confirm_after=0;
   body_page=0; folded=false;
   be_enabled=false;
   plan_active=false; tp_ready=false; direction_known=false; entry_ready=false; draw_stage=CS_DRAW_NONE;
   ZeroMemory(current_quote);
   ZeroMemory(tag_quote);
   for(int i=0;i<5;i++) tag_visible[i]=false;
  }

// Short strings are measured using the same font/DPI convention as native objects.
int TextWidth(const string text,const int base=10)
  {
   uint width=0,height=0;
   if(TextSetFont(InpFont,-10*FontSize(base)) && TextGetSize(text,width,height)) return (int)width;
   return Px(StringLen(text)*base*0.72);
  }
string Fit(const string text,const int pixels,const int base=10)
  {
   if(TextWidth(text,base)<=pixels) return text;
   int length=StringLen(text);
   while(length>0 && TextWidth(StringSubstr(text,0,length)+"...",base)>pixels) length--;
   return StringSubstr(text,0,length)+"...";
  }

bool ObjectBase(const string suffix,const ENUM_OBJECT type,const double x,const double y,const double width,const double height,const bool absolute=false)
  {
   string name=N(suffix);
   if(ObjectFind(0,name)<0 && !ObjectCreate(0,name,type,0,0,0)) return false;
   ObjectSetInteger(0,name,OBJPROP_CORNER,CORNER_LEFT_UPPER);
   ObjectSetInteger(0,name,OBJPROP_XDISTANCE,panel_x+Px(x));
   ObjectSetInteger(0,name,OBJPROP_YDISTANCE,panel_y+Px(y));
   if(type!=OBJ_LABEL) { ObjectSetInteger(0,name,OBJPROP_XSIZE,Px(width)); ObjectSetInteger(0,name,OBJPROP_YSIZE,Px(height)); }
   ObjectSetInteger(0,name,OBJPROP_BACK,false);
   ObjectSetInteger(0,name,OBJPROP_SELECTABLE,false);
   ObjectSetInteger(0,name,OBJPROP_HIDDEN,true);
   ObjectSetInteger(0,name,OBJPROP_ZORDER,type==OBJ_RECTANGLE_LABEL ? 50 : 100);
   ObjectSetString(0,name,OBJPROP_TOOLTIP,"\n");
   return true;
  }
void Box(const string id,const double x,const double y,const double w,const double h,const color fill)
  {
   if(!ObjectBase(id,OBJ_RECTANGLE_LABEL,x,y,w,h)) return;
   ObjectSetInteger(0,N(id),OBJPROP_BGCOLOR,fill);
   ObjectSetInteger(0,N(id),OBJPROP_BORDER_TYPE,BORDER_FLAT);
   ObjectSetInteger(0,N(id),OBJPROP_COLOR,C_BORDER);
  }
void Label(const string id,const double x,const double y,const string text,const color ink,const int size=9,const double max_width=280)
  {
   if(!ObjectBase(id,OBJ_LABEL,x,y,0,0)) return;
   ObjectSetInteger(0,N(id),OBJPROP_ANCHOR,ANCHOR_LEFT_UPPER);
   ObjectSetInteger(0,N(id),OBJPROP_FONTSIZE,FontSize(size));
   ObjectSetInteger(0,N(id),OBJPROP_COLOR,ink);
   ObjectSetString(0,N(id),OBJPROP_FONT,InpFont);
   ObjectSetString(0,N(id),OBJPROP_TEXT,Fit(text,Px(max_width),size));
   ObjectSetString(0,N(id),OBJPROP_TOOLTIP,text);
  }
void Button(const string id,const double x,const double y,const double w,const double h,const string text,const color fill,const int size=9)
  {
   if(!ObjectBase(id,OBJ_BUTTON,x,y,w,h)) return;
   ObjectSetString(0,N(id),OBJPROP_FONT,InpFont);
   ObjectSetString(0,N(id),OBJPROP_TEXT,Fit(text,Px(w-8),size));
   ObjectSetInteger(0,N(id),OBJPROP_FONTSIZE,FontSize(size));
   ObjectSetInteger(0,N(id),OBJPROP_COLOR,C_TEXT);
   ObjectSetInteger(0,N(id),OBJPROP_BGCOLOR,fill);
   ObjectSetInteger(0,N(id),OBJPROP_BORDER_COLOR,C_BORDER);
   ObjectSetInteger(0,N(id),OBJPROP_STATE,false);
  }
void Edit(const string id,const double x,const double y,const double w,const double h,const string text,const bool readonly=false)
  {
   bool created=(ObjectFind(0,N(id))<0);
   if(!ObjectBase(id,OBJ_EDIT,x,y,w,h)) return;
   ObjectSetString(0,N(id),OBJPROP_FONT,InpFont);
   ObjectSetInteger(0,N(id),OBJPROP_FONTSIZE,FontSize(9));
   ObjectSetInteger(0,N(id),OBJPROP_ALIGN,ALIGN_RIGHT);
   ObjectSetInteger(0,N(id),OBJPROP_COLOR,readonly ? C_MUTED : C_TEXT);
   ObjectSetInteger(0,N(id),OBJPROP_BGCOLOR,C_CARD);
   ObjectSetInteger(0,N(id),OBJPROP_BORDER_COLOR,C_BORDER);
   ObjectSetInteger(0,N(id),OBJPROP_READONLY,readonly);
   // Never overwrite a live edit on timer/tick updates. Refresh explicitly after commit.
   if(created || readonly) ObjectSetString(0,N(id),OBJPROP_TEXT,text);
  }
string DisplayPrice(const string id,const double price)
  {
   if(!plan_active || !MathIsValidNumber(price) || price<=0) return "--";
   if(id=="entry" && draw_stage==CS_DRAW_SL) return "--";
   if(StringFind(id,"tp")==0 && !tp_ready && !(id=="tp0" && draw_stage==CS_DRAW_TP)) return "--";
   return PriceText(price);
  }
void RefreshEdits()
  {
   if(ObjectFind(0,N("risk"))>=0) ObjectSetString(0,N("risk"),OBJPROP_TEXT,DoubleToString(plan.risk_percent,2));
   if(ObjectFind(0,N("entry"))>=0) ObjectSetString(0,N("entry"),OBJPROP_TEXT,DisplayPrice("entry",plan.entry));
   if(ObjectFind(0,N("sl"))>=0) ObjectSetString(0,N("sl"),OBJPROP_TEXT,DisplayPrice("sl",plan.sl));
   for(int i=0;i<3;i++)
     {
      string id="tp"+IntegerToString(i);
      if(ObjectFind(0,N(id))>=0) ObjectSetString(0,N(id),OBJPROP_TEXT,DisplayPrice(id,plan.tp[i]));
     }
  }

bool BodyRow(const double y,const double h=24)
  {
   double top=body_page*body_capacity;
   return y>=top && y+h<=top+viewport_height;
  }
double BY(const double y) { return UI_BODY_TOP+y-body_page*body_capacity; }
void PriceRow(const string id,const double y,const string text,const double price,const bool disabled=false)
  {
   if(!BodyRow(y)) return;
   bool readonly=disabled || !plan_active || draw_stage!=CS_DRAW_NONE || send_consumed;
   Label(id+"_caption",10,BY(y)+5,text,readonly ? C_MUTED : C_TEXT,9,94);
   Edit(id,112,BY(y),178,24,DisplayPrice(id,price),readonly);
  }
void ToggleButtons(const int row)
  {
   if(!BodyRow(row*UI_ROW)) return;
   double y=BY(row*UI_ROW);
   Button("toggle_tp2",10,y,88,24,plan.targets>=2 ? "TP2 ON" : "TP2",plan.targets>=2 ? C_BUY : C_CARD,8);
   Button("toggle_tp3",106,y,88,24,plan.targets>=3 ? "TP3 ON" : "TP3",plan.targets>=3 ? C_BUY : C_CARD,8);
   Button("be",202,y,88,24,be_enabled ? "BE ON" : "BE",be_enabled ? C_BUY : C_CARD,8);
   ObjectSetString(0,N("toggle_tp2"),OBJPROP_TOOLTIP,"Show or hide the second take-profit target");
   ObjectSetString(0,N("toggle_tp3"),OBJPROP_TOOLTIP,"Show or hide the third target; enabling TP3 also enables TP2");
   ObjectSetString(0,N("be"),OBJPROP_TOOLTIP,"Break-even: after +1R, move SL to entry. 1R is the original entry-to-SL price distance. No fee adjustment. Keep this EA online; only this symbol and ID are managed.");
  }
void DeleteBody()
  {
   // Recreate only UI objects, keeping draft price lines in place.
   for(int i=ObjectsTotal(0)-1;i>=0;i--)
     {
      string name=ObjectName(0,i);
      if(StringFind(name,prefix)!=0) continue;
      if(StringFind(name,N("line_"))==0 || StringFind(name,N("zone_"))==0 || StringFind(name,N("tag_"))==0) continue;
      ObjectDelete(0,name);
     }
  }

void Layout(const bool has_own_exposure)
  {
   const int UI_FOOTER=80,UI_PAGE_NAV=20;
   effective_dpi=InpDpiOverride>0 ? InpDpiOverride : (double)TerminalInfoInteger(TERMINAL_SCREEN_DPI);
   if(effective_dpi<72 || effective_dpi>384) effective_dpi=96;
   long cw=ChartGetInteger(0,CHART_WIDTH_IN_PIXELS),ch=ChartGetInteger(0,CHART_HEIGHT_IN_PIXELS);
   double dpi_ratio=effective_dpi/96.0;
   font_zoom=MathMin(ui_zoom,(cw-8)/(UI_WIDTH*dpi_ratio));
   chart_too_small=(font_zoom<0.6);
   font_zoom=MathMax(0.6,font_zoom);
   geometry=dpi_ratio*font_zoom;
   // Native fonts follow system DPI; scale the pixel geometry exactly once.
   panel_x=(int)MathMax(0,MathMin(InpPanelX,cw-Px(UI_WIDTH)-4));
   panel_y=(int)MathMax(0,MathMin(InpPanelY,ch-Px(UI_BODY_TOP+UI_ROW+UI_FOOTER+UI_PAGE_NAV)-4));
   panel_width=chart_too_small ? (int)MathMax(40,MathMin(UI_WIDTH,MathFloor((cw-4)/geometry))) : UI_WIDTH;
   int available=(int)MathFloor((ch-panel_y-4)/geometry);
   body_rows=8+(int)MathMax(0,MathMin(2,plan.targets-1))+(has_own_exposure ? 1 : 0);
   // Reserve two status lines; add navigation space only if paging is needed.
   int footer_height=UI_FOOTER;
   body_capacity=(int)MathMax(UI_ROW,MathFloor((available-UI_BODY_TOP-footer_height)/(double)UI_ROW)*UI_ROW);
   body_capacity=(int)MathMin(body_capacity,body_rows*UI_ROW);
   if(body_capacity<body_rows*UI_ROW)
     {
      footer_height+=UI_PAGE_NAV;
      body_capacity=(int)MathMax(UI_ROW,MathFloor((available-UI_BODY_TOP-footer_height)/(double)UI_ROW)*UI_ROW);
      body_capacity=(int)MathMin(body_capacity,body_rows*UI_ROW);
     }
   viewport_height=body_capacity;
   body_pages=(int)MathCeil((double)(body_rows*UI_ROW)/body_capacity);
   body_page=(int)MathMax(0,MathMin(body_page,body_pages-1));
   panel_height=folded ? 38 : UI_BODY_TOP+viewport_height+footer_height;
   if((!folded && available<UI_BODY_TOP+UI_ROW+footer_height) || available<38) chart_too_small=true;
   if(chart_too_small) { panel_y=0; panel_height=(int)MathMin(76,MathMax(0,MathFloor(ch/geometry))); }
  }

void Render()
  {
   // One count snapshot keeps dynamic row sizing and row rendering consistent.
   int position_count=engine.PositionCount(),pending_count=engine.PendingCount();
   bool has_own_exposure=(position_count+pending_count)>0;
   int old_x=panel_x,old_y=panel_y,old_width=panel_width,old_height=panel_height;
   double old_geometry=geometry,old_font_zoom=font_zoom;
   Layout(has_own_exposure);
   if(plan_active && (old_x!=panel_x || old_y!=panel_y || old_width!=panel_width || old_height!=panel_height ||
      old_geometry!=geometry || old_font_zoom!=font_zoom))
     {
      // Use this frame's panel bounds and font metrics when avoiding the overlay.
      datetime left=0,right=0; VisibleSpan(left,right); SyncTags(right);
     }
   static int rendered_rows=-1,rendered_page=-1,rendered_capacity=-1;
   if(rendered_rows!=body_rows || rendered_page!=body_page || rendered_capacity!=body_capacity)
     {
      DeleteBody();
      rendered_rows=body_rows; rendered_page=body_page; rendered_capacity=body_capacity;
     }
   ObjectDelete(0,N("hint"));
   if(plan_active && direction_known && plan.mode==CS_MARKET)
      plan.entry=send_consumed && tag_quote.valid ? tag_quote.entry : LiveEntry();
   ZeroMemory(current_quote);
   // Drawing prompts and the initial empty panel have no executable quote.
   if(plan_active && direction_known && draw_stage==CS_DRAW_NONE && tp_ready)
     {
      if(send_consumed && tag_quote.valid) current_quote=tag_quote;
      else engine.Quote(plan,current_quote);
     }
   Box("background",0,0,panel_width,panel_height,C_BG);
   if(chart_too_small)
     {
      double remove_x=MathMax(4,panel_width-34);
      if(remove_x>=50 && panel_height>=24) Label("title",10,8,"ClearSizer",C_TEXT,11,remove_x-18);
      if(panel_height>=20)
        {
         Button("remove_ea",remove_x,4,24,MathMin(22,panel_height-8),"X",C_SELL,8);
         ObjectSetString(0,N("remove_ea"),OBJPROP_TOOLTIP,"Remove ClearSizer EA from this chart");
        }
      if(panel_height>=60) Label("small_hint",10,42,"Enlarge chart to show controls.",C_WARN,8,MathMax(10,panel_width-20));
      ChartRedraw(); return;
     }
   Label("title",10,10,"ClearSizer",C_TEXT,11,146);
   Button("zoom_down",162,8,24,22,"-",C_CARD);
   Button("zoom_up",190,8,24,22,"+",C_CARD);
   Button("fold",218,8,44,22,folded ? "OPEN" : "FOLD",C_CARD,8);
   Button("remove_ea",266,8,24,22,"X",C_SELL,8);
   ObjectSetString(0,N("remove_ea"),OBJPROP_TOOLTIP,"Remove ClearSizer EA from this chart");
   if(folded) { ChartRedraw(); return; }
   string trade_label=direction_known ? (plan.buy ? "BUY " : "SELL ")+ModeText() : "AUTO / "+ModeText();
   Label("subtitle",10,33,_Symbol+" / "+trade_label+" / "+(engine.IsHedging() ? "HEDGING" : "NETTING"),C_MUTED,8,280);
   if(BodyRow(0))
     {
      Button("new_order",10,BY(0),136,24,"NEW ORDER",plan.mode!=CS_MARKET ? C_ACCENT : C_CARD,9);
      ObjectSetString(0,N("new_order"),OBJPROP_TOOLTIP,"Start a pending setup. SL relative to Entry selects BUY or SELL; Entry relative to Ask/Bid selects LIMIT or STOP.");
      Button("quick_order",154,BY(0),136,24,"QUICK (MARKET)",plan.mode==CS_MARKET ? C_ACCENT : C_CARD,9);
     }
   if(BodyRow(UI_ROW))
     {
      Label("risk_caption",10,BY(UI_ROW)+5,"Risk %",C_MUTED,9,42);
      Edit("risk",54,BY(UI_ROW),68,24,DoubleToString(plan.risk_percent,2));
      Button("risk_down",126,BY(UI_ROW),24,24,"-",C_CARD);
      Button("risk_up",154,BY(UI_ROW),24,24,"+",C_CARD);
      Button("basis",182,BY(UI_ROW),108,24,plan.use_equity ? "EQUITY" : "BALANCE",C_CARD,8);
     }
   PriceRow("entry",2*UI_ROW,plan.mode==CS_MARKET ? "Entry (live)" : "Entry",plan.entry,plan.mode==CS_MARKET);
   PriceRow("sl",3*UI_ROW,"Stop loss",plan.sl);
   PriceRow("tp0",4*UI_ROW,"Take profit 1",plan.tp[0]);
   int row=5;
   if(plan.targets>=2) { PriceRow("tp1",row*UI_ROW,"Take profit 2",plan.tp[1]); row++; }
   if(plan.targets>=3) { PriceRow("tp2",row*UI_ROW,"Take profit 3",plan.tp[2]); row++; }
   ToggleButtons(row); row++;
   string currency=AccountInfoString(ACCOUNT_CURRENCY);
   if(BodyRow(row*UI_ROW))
     {
      Label("lots",10,BY(row*UI_ROW)+5,"Lots "+(current_quote.valid ? DoubleToString(current_quote.volume,8) : "--"),C_TEXT,9,132);
      Label("risk_info",154,BY(row*UI_ROW)+5,"Risk "+(current_quote.valid ? DoubleToString(current_quote.risk_cash,2)+" "+currency : "--"),C_SELL,9,136);
     }
   row++;
   if(BodyRow(row*UI_ROW))
     {
      Label("rr",10,BY(row*UI_ROW)+5,"R:R "+(current_quote.valid ? DoubleToString(current_quote.rr,2) : "--"),C_BUY,9,132);
      Label("margin",154,BY(row*UI_ROW)+5,"Margin "+(current_quote.valid ? DoubleToString(current_quote.margin,2) : "--"),C_MUTED,9,136);
     }
   row++;
   if(has_own_exposure && BodyRow(row*UI_ROW))
     {
      Button("cancel",10,BY(row*UI_ROW),136,24,armed=="cancel" ? "CONFIRM CANCEL" : "CANCEL PENDING",C_CARD,8);
      Button("close",154,BY(row*UI_ROW),136,24,armed=="close" ? "CONFIRM CLOSE" : "CLOSE POSITIONS",C_SELL,8);
      string scope="Only "+_Symbol+" / ID "+(string)InpMagic+". ";
      ObjectSetString(0,N("cancel"),OBJPROP_TOOLTIP,scope+IntegerToString(pending_count)+" pending order(s). Click again to confirm cancellation.");
      ObjectSetString(0,N("close"),OBJPROP_TOOLTIP,scope+IntegerToString(position_count)+" position(s). Click again to confirm closing.");
     }
   else
     {
      ObjectDelete(0,N("cancel")); ObjectDelete(0,N("close"));
     }
   double fy=UI_BODY_TOP+viewport_height;
   if(body_pages>1)
     {
      Button("previous",10,fy,48,20,"PREV",C_CARD,8);
      string page_text=IntegerToString(body_page+1)+" / "+IntegerToString(body_pages);
      if(draw_stage==CS_DRAW_SL) page_text="DRAW SL  |  "+page_text;
      else if(draw_stage==CS_DRAW_ENTRY) page_text="DRAW ENTRY  |  "+page_text;
      else if(draw_stage==CS_DRAW_TP) page_text="DRAW TP  |  "+page_text;
      Label("page",66,fy+4,page_text,C_MUTED,8,164);
      Button("next",242,fy,48,20,"NEXT",C_CARD,8);
     }
   else
     {
      ObjectDelete(0,N("previous")); ObjectDelete(0,N("page")); ObjectDelete(0,N("next"));
     }
   double ay=fy+(body_pages>1 ? 24 : 4);
   string send_text="CONFIRM ORDER";
   if(send_consumed) send_text="CHECK RESULT";
   else if(!plan_active) send_text="WAIT FOR SETUP";
   else if(draw_stage==CS_DRAW_SL) send_text="DRAW SL";
   else if(draw_stage==CS_DRAW_ENTRY) send_text="DRAW ENTRY";
   else if(draw_stage==CS_DRAW_TP || !tp_ready) send_text="DRAW TP";
   color send_fill=(plan_active && direction_known && draw_stage==CS_DRAW_NONE && tp_ready && !send_consumed) ? C_ACCENT : C_CARD;
   Button("discard",10,ay,136,28,"CANCEL SETUP",C_CARD,9);
   Button("send",154,ay,136,28,send_text,send_fill,8);
   string hint="Drag lines to adjust / ESC to cancel.";
   if(send_consumed) hint="Check result; start a new setup.";
   else if(!plan_active) hint="NEW ORDER: pending / QUICK: market.";
   else if(draw_stage==CS_DRAW_SL) hint="Click the chart to place STOP LOSS.";
   else if(draw_stage==CS_DRAW_ENTRY) hint="Click the chart to place PENDING ENTRY.";
   else if(draw_stage==CS_DRAW_TP) hint="Click the chart to place TAKE PROFIT.";
   string status=notice;
   string sub=detail;
   if(plan_active && direction_known && draw_stage==CS_DRAW_NONE && tp_ready && !current_quote.valid && armed=="" && quote_status)
     { status=current_quote.error; sub="Adjust the plan before sending."; }
   else if(plan_active && !send_consumed && draw_stage==CS_DRAW_NONE && tp_ready && current_quote.valid && armed=="" && quote_status)
      sub="Drag lines / ESC to cancel.";
   else if(sub=="") sub=hint;
   if(status=="") { status=hint; if(sub==hint) sub=""; }
   Label("status",10,ay+34,status,C_TEXT,9,280);
   Label("status_detail",10,ay+50,sub,C_MUTED,8,280);
   string full_context=notice;
   if(status!="" && status!=notice) full_context=status+(full_context=="" ? "" : "\n"+full_context);
   if(detail!="") full_context+=(full_context=="" ? "" : "\n")+detail;
   if(hint!="") full_context+=(full_context=="" ? "" : "\n")+hint;
   ObjectSetString(0,N("status"),OBJPROP_TOOLTIP,full_context);
   ObjectSetString(0,N("status_detail"),OBJPROP_TOOLTIP,full_context);
   ChartRedraw();
  }

void Rebuild() { DeleteBody(); Render(); }
void Line(const string id,const double price,const color ink,const string text,const bool selectable=true,const bool update_existing=true)
  {
   string name=N("line_"+id);
   bool created=(ObjectFind(0,name)<0);
   if(created) ObjectCreate(0,name,OBJ_HLINE,0,0,price);
   if(created || update_existing) ObjectSetDouble(0,name,OBJPROP_PRICE,price);
   ObjectSetInteger(0,name,OBJPROP_COLOR,ink);
   ObjectSetInteger(0,name,OBJPROP_STYLE,STYLE_DASH);
   ObjectSetInteger(0,name,OBJPROP_WIDTH,1);
   ObjectSetInteger(0,name,OBJPROP_BACK,true);
   ObjectSetInteger(0,name,OBJPROP_SELECTABLE,selectable);
   ObjectSetInteger(0,name,OBJPROP_HIDDEN,false);
   ObjectSetInteger(0,name,OBJPROP_ZORDER,10);
   ObjectSetString(0,name,OBJPROP_TEXT,"");
   ObjectSetString(0,name,OBJPROP_TOOLTIP,text+"  "+PriceText(price)+" (draft)");
  }
string LotsText(const double volume)
  {
   string out=DoubleToString(volume,8);
   while(StringLen(out)>0 && StringSubstr(out,StringLen(out)-1,1)=="0") out=StringSubstr(out,0,StringLen(out)-1);
   if(StringLen(out)>0 && StringSubstr(out,StringLen(out)-1,1)==".") out=StringSubstr(out,0,StringLen(out)-1);
   return out;
  }
string SignedCash(const double value) { return (value>0 ? "+" : "")+DoubleToString(value,2); }
bool DisplayQuote(CSQuote &q)
  {
   ZeroMemory(q);
   if(!plan_active) return false;
   if(send_consumed) { q=tag_quote; return q.valid; }
   // This copy is used only for drawing estimates, never for Execute or draft state.
   CSPlan shown=plan;
   if(shown.mode!=CS_MARKET && draw_stage==CS_DRAW_SL)
     { q.error="Place Entry to calculate lots and P/L."; return false; }
   if(shown.mode==CS_MARKET)
     {
      string why=""; bool candidate=shown.buy;
      if(!direction_known && !DirectionForSL(shown.sl,candidate,why)) { q.error=why; return false; }
      shown.buy=candidate;
      MqlTick tick={};
      if(!SymbolInfoTick(_Symbol,tick)) { q.error="Waiting for a valid quote."; return false; }
      shown.entry=engine.Price(shown.buy ? tick.ask : tick.bid);
     }
   else if(draw_stage==CS_DRAW_ENTRY)
     {
      bool candidate=shown.buy; CSOrderMode mode=shown.mode; string why="";
      if(!ClassifyPending(shown.entry,shown.sl,candidate,mode,why)) { q.error=why; return false; }
      shown.buy=candidate; shown.mode=mode;
     }
   if(!tp_ready)
     {
      double sign=shown.buy ? 1.0 : -1.0;
      // Missing targets fund a size preview only. Their hypothetical profit is not displayed.
      if(draw_stage!=CS_DRAW_TP)
         shown.tp[0]=engine.Price(shown.entry+sign*MathAbs(shown.entry-shown.sl)*InpRewardR);
      double step=MathMax(MathAbs(shown.tp[0]-shown.entry)*0.5,SymbolInfoDouble(_Symbol,SYMBOL_TRADE_TICK_SIZE));
      for(int i=1;i<shown.targets && i<3;i++) shown.tp[i]=engine.Price(shown.tp[i-1]+sign*step);
     }
   return engine.Quote(shown,q);
  }
void TagText(const string id,const double price,const CSQuote &q,string &full,string &compact)
  {
   string kind=id=="entry" ? "Entry" : (id=="sl" ? "SL" : "TP"+IntegerToString((int)StringToInteger(StringSubstr(id,2))+1));
   string currency=AccountInfoString(ACCOUNT_CURRENCY);
   double volume=q.volume,cash=0;
   if(id=="sl") cash=-q.risk_cash;
   else if(id!="entry")
     {
      int leg=(int)StringToInteger(StringSubstr(id,2));
      volume=q.leg_volume[leg]; cash=q.leg_reward_cash[leg];
     }
   string lots=q.valid ? LotsText(volume) : "--";
   string pnl=q.valid ? SignedCash(cash) : "--";
   if(id=="entry")
     {
      string loss=q.valid ? SignedCash(-q.risk_cash) : "--";
      string reward=q.valid && tp_ready ? SignedCash(q.reward_cash) : "--";
      full=kind+" "+PriceText(price)+" | "+lots+" lots | Est. SL "+loss+" / TP "+reward+" "+currency;
      compact=kind+" "+PriceText(price)+" | "+lots+"L | SL "+loss+" / TP "+reward+" "+currency;
     }
   else
     {
      full=kind+" "+PriceText(price)+" | "+lots+" lots | Est. "+pnl+" "+currency;
      compact=kind+" "+PriceText(price)+" | "+lots+"L | "+pnl+" "+currency;
     }
  }
int TagHeight()
  {
   uint width=0,height=0;
   if(TextSetFont(InpFont,-10*FontSize(8)) && TextGetSize("TP1 Ag",width,height)) return (int)MathMax(8,height);
   return (int)MathCeil(FontSize(8)*effective_dpi/72.0*1.25);
  }
void SyncTags(const datetime right)
  {
   CSQuote q; DisplayQuote(q);
   bool ready=(draw_stage==CS_DRAW_NONE && tp_ready && direction_known);
   string ids[5]={"entry","sl","tp0","tp1","tp2"};
   double prices[5]; prices[0]=plan.entry; prices[1]=plan.sl;
   for(int i=0;i<3;i++) prices[i+2]=plan.tp[i];
   int ys[5],xs[5],order[5],count=0;
   int height=(int)ChartGetInteger(0,CHART_HEIGHT_IN_PIXELS,0);
   int width=(int)ChartGetInteger(0,CHART_WIDTH_IN_PIXELS);
   int text_height=TagHeight(),gap=(int)MathMax(2,Px(2));
   for(int i=0;i<5;i++)
     {
      tag_visible[i]=false;
      bool visible=(i==1 || (i==0 && prices[0]>0 && (plan.mode==CS_MARKET || draw_stage!=CS_DRAW_SL)) ||
                    (i>=2 && ((ready && i-2<plan.targets) || (draw_stage==CS_DRAW_TP && i==2))));
      string full="",compact=""; TagText(ids[i],prices[i],q,full,compact);
      string tooltip="Draft estimate: "+full+"\n"+(q.valid ? "SL/TP forecast includes configured fees; not current floating P/L." : q.error);
      if(ObjectFind(0,N("line_"+ids[i]))>=0) ObjectSetString(0,N("line_"+ids[i]),OBJPROP_TOOLTIP,tooltip);
      int x=0,y=0;
      if(!visible || prices[i]<=0 || right<=0 || !ChartTimePriceToXY(0,0,right,prices[i],x,y) ||
         y<0 || y>=height || x<0 || x>=width)
        { ObjectDelete(0,N("tag_"+ids[i])); continue; }
      xs[i]=x-Px(8); ys[i]=(int)MathMax(text_height+2,MathMin(height-2,y-gap));
      order[count++]=i;
     }
   // Spread close levels vertically. Each tag still names its exact price and line.
   for(int a=0;a<count;a++) for(int b=a+1;b<count;b++)
      if(ys[order[b]]<ys[order[a]]) { int swap=order[a]; order[a]=order[b]; order[b]=swap; }
   for(int a=1;a<count;a++) ys[order[a]]=(int)MathMax(ys[order[a]],ys[order[a-1]]+text_height+gap);
   if(count>0 && ys[order[count-1]]>height-2)
     {
      ys[order[count-1]]=height-2;
      for(int a=count-2;a>=0;a--) ys[order[a]]=(int)MathMin(ys[order[a]],ys[order[a+1]]-text_height-gap);
     }
   for(int a=0;a<count;a++)
     {
      int i=order[a],bottom=ys[i],right_x=xs[i];
      string name=N("tag_"+ids[i]);
      int left_limit=Px(4);
      if(bottom>=panel_y && bottom-text_height<=panel_y+Px(panel_height))
         left_limit=(int)MathMax(left_limit,panel_x+Px(panel_width)+Px(8));
      int available=right_x-left_limit;
      string full="",compact=""; TagText(ids[i],prices[i],q,full,compact);
      string text=full;
      if(TextWidth(text,8)>available) text=compact;
      if(TextWidth(text,8)>available)
         text=(i==0 ? "Entry" : (i==1 ? "SL" : "TP"+IntegerToString(i-1)))+" "+PriceText(prices[i]);
      int text_width=TextWidth(text,8);
      if(bottom<text_height || available<text_width || available<=0)
        { ObjectDelete(0,name); continue; }
      if(ObjectFind(0,name)<0) ObjectCreate(0,name,OBJ_LABEL,0,0,0);
      ObjectSetInteger(0,name,OBJPROP_CORNER,CORNER_LEFT_UPPER);
      ObjectSetInteger(0,name,OBJPROP_ANCHOR,ANCHOR_RIGHT_LOWER);
      ObjectSetInteger(0,name,OBJPROP_XDISTANCE,right_x);
      ObjectSetInteger(0,name,OBJPROP_YDISTANCE,bottom);
      ObjectSetInteger(0,name,OBJPROP_FONTSIZE,FontSize(8));
      ObjectSetString(0,name,OBJPROP_FONT,InpFont);
      ObjectSetInteger(0,name,OBJPROP_COLOR,i==1 ? C_SELL : (i==0 ? C_ACCENT : C_BUY));
      ObjectSetInteger(0,name,OBJPROP_BACK,false);
      ObjectSetInteger(0,name,OBJPROP_SELECTED,false);
      ObjectSetInteger(0,name,OBJPROP_SELECTABLE,false);
      ObjectSetInteger(0,name,OBJPROP_HIDDEN,true);
      ObjectSetInteger(0,name,OBJPROP_ZORDER,20);
      ObjectSetString(0,name,OBJPROP_TEXT,text);
      ObjectSetString(0,name,OBJPROP_TOOLTIP,"Draft estimate: "+full+"\n"+(q.valid ? "SL/TP forecast includes configured fees; not current floating P/L." : q.error));
      tag_visible[i]=true; tag_left[i]=right_x-text_width; tag_right[i]=right_x;
      tag_top[i]=bottom-text_height; tag_bottom[i]=bottom; tag_price[i]=prices[i];
     }
  }
bool VisibleSpan(datetime &left,datetime &right)
  {
   left=0; right=0;
   int width=(int)ChartGetInteger(0,CHART_WIDTH_IN_PIXELS);
   int height=(int)ChartGetInteger(0,CHART_HEIGHT_IN_PIXELS,0);
   if(width<2 || height<2) return false;
   int first_x=-1,last_x=-1;
   datetime first=0,last=0;
   // Price scales and chart borders may not have a time coordinate. Find the plot edges.
   for(int x=0;x<width;x+=8)
     {
      int window=-1; datetime time=0; double price=0;
      if(ChartXYToTimePrice(0,x,height/2,window,time,price) && window==0 && time>0)
        { first_x=x; first=time; break; }
     }
   for(int x=width-1;x>=0;x-=8)
     {
      int window=-1; datetime time=0; double price=0;
      if(ChartXYToTimePrice(0,x,height/2,window,time,price) && window==0 && time>0)
        { last_x=x; last=time; break; }
     }
   if(first_x<0 || last_x<0) return false;
   for(int x=first_x-1;x>=0 && x>=first_x-7;x--)
     {
      int window=-1; datetime time=0; double price=0;
      if(!ChartXYToTimePrice(0,x,height/2,window,time,price) || window!=0 || time<=0) break;
      first=time;
     }
   for(int x=last_x+1;x<width && x<=last_x+7;x++)
     {
      int window=-1; datetime time=0; double price=0;
      if(!ChartXYToTimePrice(0,x,height/2,window,time,price) || window!=0 || time<=0) break;
      last=time;
     }
   if(first<=0 || last<=0 || first==last) return false;
   left=first<last ? first : last;
   right=first<last ? last : first;
   return true;
  }
void Zone(const string id,const double a,const double b,const color fill,const datetime left,const datetime right)
  {
   string name=N("zone_"+id);
   if(a<=0 || b<=0 || !MathIsValidNumber(a) || !MathIsValidNumber(b) || left<=0 || right<=left)
     { ObjectDelete(0,name); return; }
   if(ObjectFind(0,name)<0) ObjectCreate(0,name,OBJ_RECTANGLE,0,left,a,right,b);
   ObjectMove(0,name,0,left,a); ObjectMove(0,name,1,right,b);
   ObjectSetInteger(0,name,OBJPROP_COLOR,fill);
   ObjectSetInteger(0,name,OBJPROP_FILL,true);
   ObjectSetInteger(0,name,OBJPROP_BACK,true);
   ObjectSetInteger(0,name,OBJPROP_SELECTED,false);
   ObjectSetInteger(0,name,OBJPROP_SELECTABLE,false);
   ObjectSetInteger(0,name,OBJPROP_HIDDEN,true);
   ObjectSetInteger(0,name,OBJPROP_ZORDER,0);
   ObjectSetString(0,name,OBJPROP_TOOLTIP,"\n");
  }
void SyncLines(const bool live_only=false)
  {
   if(!plan_active) return;
   if(plan.mode==CS_MARKET) plan.entry=send_consumed && tag_quote.valid ? tag_quote.entry : LiveEntry();
   bool ready=(draw_stage==CS_DRAW_NONE && tp_ready && direction_known);
   bool editable=ready && !send_consumed;
   if(plan.entry>0 && (plan.mode==CS_MARKET || draw_stage!=CS_DRAW_SL))
      Line("entry",plan.entry,C_ACCENT,draw_stage==CS_DRAW_ENTRY ? "Click to place pending entry" : "Entry / "+ModeText(),editable && plan.mode!=CS_MARKET,!live_only || plan.mode==CS_MARKET);
   else ObjectDelete(0,N("line_entry"));
   Line("sl",plan.sl,C_SELL,draw_stage==CS_DRAW_SL ? "Click to place SL" : "Stop loss",editable,!live_only);
   for(int i=0;i<3;i++)
     {
      string id="tp"+IntegerToString(i);
      bool visible=(ready && i<plan.targets) || (draw_stage==CS_DRAW_TP && i==0);
      if(visible) Line(id,plan.tp[i],C_BUY,draw_stage==CS_DRAW_TP ? "Click to place TP1" : "Take profit "+IntegerToString(i+1),editable,!live_only);
      else ObjectDelete(0,N("line_"+id));
     }
   // Before pending Entry is placed, the live quote is only a visual reference.
   datetime left=0,right=0;
   VisibleSpan(left,right);
   double zone_entry=plan.entry>0 ? plan.entry : LiveEntry();
   if(zone_entry>0) Zone("loss",zone_entry,plan.sl,C'55,30,39',left,right);
   else ObjectDelete(0,N("zone_loss"));
   if(ready || draw_stage==CS_DRAW_TP)
      Zone("gain",plan.entry,plan.tp[ready ? plan.targets-1 : 0],C'23,43,65',left,right);
   else ObjectDelete(0,N("zone_gain"));
   SyncTags(right);
  }
void ClearDraftLines()
  {
   ObjectsDeleteAll(0,N("line_"));
   ObjectsDeleteAll(0,N("zone_"));
   ObjectsDeleteAll(0,N("tag_"));
   for(int i=0;i<5;i++) tag_visible[i]=false;
   ZeroMemory(tag_quote);
  }
void CancelDraft()
  {
   Disarm();
   type_confirm_after=0;
   plan_active=false; tp_ready=false; direction_known=false; entry_ready=false; draw_stage=CS_DRAW_NONE;
   plan.entry=0;
   plan.sl=0;
   for(int i=0;i<3;i++) plan.tp[i]=0;
   ClearDraftLines();
   Say("Setup cancelled.","NEW ORDER = Pending / QUICK = Market.");
  }
void GenerateExtraTargets(const int first=1)
  {
   if(!tp_ready) return;
   double sign=plan.buy ? 1.0 : -1.0;
   double step=MathMax(MathAbs(plan.tp[0]-plan.entry)*0.5,SymbolInfoDouble(_Symbol,SYMBOL_TRADE_TICK_SIZE));
   for(int i=(int)MathMax(1,first);i<plan.targets;i++)
      plan.tp[i]=engine.Price(plan.tp[i-1]+sign*step);
  }
void ToggleTarget(const bool third)
  {
   Disarm();
   if(send_consumed)
     { Say("Start NEW ORDER or QUICK to change targets.","Submitted draft estimates remain fixed."); return; }
   if(!engine.IsHedging()) { plan.targets=1; Say("Netting supports one TP.","Multiple TPs require a hedging account."); return; }
   int previous=plan.targets;
   if(third) plan.targets=(plan.targets==3 ? 2 : 3);
   else plan.targets=(plan.targets>=2 ? 1 : 2);
   if(plan.targets>previous) GenerateExtraTargets(previous);
   Say("Targets: TP1"+(plan.targets>=2 ? ", TP2" : "")+(plan.targets>=3 ? ", TP3" : ""),tp_ready ? "Extra lines appear automatically." : "Extra TPs appear after placing TP1.");
  }
void NewPlan(const CSOrderMode mode=CS_LIMIT)
  {
   if(closing) return;
   Disarm();
   type_confirm_after=0;
   ClearDraftLines();
   tp_ready=false; direction_known=false; entry_ready=false; draw_stage=CS_DRAW_NONE; plan_active=false; send_consumed=false;
   plan.mode=mode; plan.buy=true; plan.entry=0;
   double tick=SymbolInfoDouble(_Symbol,SYMBOL_TRADE_TICK_SIZE);
   MqlTick q={}; bool have_quote=SymbolInfoTick(_Symbol,q);
   if(!have_quote || (mode!=CS_LIMIT && mode!=CS_MARKET) || tick<=0 || q.bid<=0 || q.ask<=0 || q.ask<q.bid) { Say("Waiting for a valid quote.","Choose NEW ORDER or QUICK when prices arrive."); return; }
   double distance=MathMax(InpStopPoints*_Point,MathMax((double)SymbolInfoInteger(_Symbol,SYMBOL_TRADE_STOPS_LEVEL)*_Point+2*(q.ask-q.bid),tick*10));
   if(mode==CS_MARKET) plan.entry=engine.Price(q.ask);
   plan.sl=engine.Price(q.bid-distance);
   for(int i=0;i<3;i++) plan.tp[i]=0;
   plan_active=true; draw_stage=CS_DRAW_SL;
   Say(mode!=CS_MARKET ? "1 / 3  Click to place SL." : "1 / 2  Click to place SL.",mode!=CS_MARKET ? "Next: Entry determines BUY / SELL." : "Below price = BUY / above = SELL.");
   SyncLines();
  }
bool DirectionForSL(const double sl,bool &buy,string &why)
  {
   MqlTick tick={};
   if(!SymbolInfoTick(_Symbol,tick) || tick.bid<=0 || tick.ask<=0 || tick.ask<tick.bid) { why="Waiting for a valid quote."; return false; }
   double eps=SymbolInfoDouble(_Symbol,SYMBOL_TRADE_TICK_SIZE)*0.000001;
   if(sl<tick.bid-eps) { buy=true; return true; }
   if(sl>tick.ask+eps) { buy=false; return true; }
   why="Place SL below Bid or above Ask.";
   return false;
  }
bool ClassifyPending(const double entry,const double sl,bool &buy,CSOrderMode &mode,string &why)
  {
   double at=engine.Price(entry),stop=engine.Price(sl);
   double tick_size=SymbolInfoDouble(_Symbol,SYMBOL_TRADE_TICK_SIZE);
   double eps=tick_size*0.000001;
   if(!MathIsValidNumber(at) || !MathIsValidNumber(stop) || at<=0 || stop<=0 || tick_size<=0)
     { why="Entry and SL must be positive prices."; return false; }
   MqlTick tick={};
   if(!SymbolInfoTick(_Symbol,tick) || tick.bid<=0 || tick.ask<=0 || tick.ask<tick.bid)
     { why="Waiting for a valid quote."; return false; }
   double minimum=(double)SymbolInfoInteger(_Symbol,SYMBOL_TRADE_STOPS_LEVEL)*_Point;
   double stop_gap=MathAbs(at-stop);
   if(stop_gap<=eps || stop_gap+eps<minimum)
     { why="SL is too close to Entry."; return false; }
   buy=(stop<at);
   double market=buy ? tick.ask : tick.bid;
   double market_gap=MathAbs(at-market);
   if(market_gap<=eps || market_gap+eps<minimum)
     { why=buy ? "Entry is too close to Ask." : "Entry is too close to Bid."; return false; }
   mode=(buy ? at<market : at>market) ? CS_LIMIT : CS_STOP;
   return true;
  }
bool LockedSLValid(string &why)
  {
   bool candidate=false;
   if(!direction_known) { why="Place SL to determine direction."; return false; }
   if(plan.mode!=CS_MARKET)
     {
      double eps=SymbolInfoDouble(_Symbol,SYMBOL_TRADE_TICK_SIZE)*0.000001;
      if(!entry_ready || MathAbs(plan.entry-plan.sl)<=eps)
        { why="Place Entry away from SL."; return false; }
      candidate=(plan.sl<plan.entry);
      if(candidate!=plan.buy) { why="SL crossed the planned Entry."; return false; }
      return true;
     }
   if(!DirectionForSL(plan.sl,candidate,why)) return false;
   if(candidate!=plan.buy) { why="Market crossed the locked SL."; return false; }
   return true;
  }
void RestartSL()
  {
   Disarm(); tp_ready=false; direction_known=false; entry_ready=false; draw_stage=CS_DRAW_SL;
   plan.entry=plan.mode==CS_MARKET ? LiveEntry() : 0;
   for(int i=0;i<3;i++) plan.tp[i]=0;
   Say("Price moved. Place SL again.","Direction is chosen from the next SL click.");
   SyncLines(); Rebuild();
  }
void BeginEntryOrTP()
  {
   tp_ready=false;
   for(int i=0;i<3;i++) plan.tp[i]=0;
   if(plan.mode!=CS_MARKET)
     {
      direction_known=false; entry_ready=false;
      plan.entry=engine.Price((plan.sl+LiveEntry())*0.5);
      draw_stage=CS_DRAW_ENTRY;
      Say("2 / 3  Click to place pending Entry.","SL < Entry = BUY / SL > Entry = SELL.");
     }
   else
     {
      entry_ready=true;
      plan.entry=LiveEntry();
      plan.tp[0]=engine.Price(plan.entry+(plan.buy ? 1.0 : -1.0)*MathAbs(plan.entry-plan.sl)*InpRewardR);
      draw_stage=CS_DRAW_TP;
      Say("2 / 2  Click to place TP1.",plan.buy ? "BUY MARKET / place TP above entry." : "SELL MARKET / place TP below entry.");
     }
  }
bool PendingEdited()
  {
   if(plan.mode==CS_MARKET) return false;
   bool candidate=false; CSOrderMode mode=CS_LIMIT; string why="";
   if(!ClassifyPending(plan.entry,plan.sl,candidate,mode,why))
     {
      Disarm(); entry_ready=false; direction_known=false; tp_ready=false; draw_stage=CS_DRAW_ENTRY;
      for(int i=0;i<3;i++) plan.tp[i]=0;
      Say(why,"Click the chart to place pending Entry again.");
      SyncLines(); Rebuild(); return true;
     }
   bool changed=(!direction_known || candidate!=plan.buy);
   plan.buy=candidate; plan.mode=mode; direction_known=true; entry_ready=true;
   if(changed)
     {
      Disarm(); tp_ready=false; draw_stage=CS_DRAW_TP;
      for(int i=0;i<3;i++) plan.tp[i]=0;
      plan.tp[0]=engine.Price(plan.entry+(plan.buy ? 1.0 : -1.0)*MathAbs(plan.entry-plan.sl)*InpRewardR);
      Say("Direction changed. Place TP1 again.",OrderTypeText()+" / "+(plan.buy ? "TP above entry." : "TP below entry."));
      SyncLines(); Rebuild(); return true;
     }
   return false;
  }
bool StopEdited()
  {
   if(plan.mode!=CS_MARKET) return PendingEdited();
   bool candidate=false; string why="";
   if(!DirectionForSL(plan.sl,candidate,why)) { RestartSL(); return true; }
   if(!direction_known || candidate!=plan.buy)
     {
      Disarm(); plan.buy=candidate; direction_known=true;
      BeginEntryOrTP();
      SyncLines(); Rebuild(); return true;
     }
   return false;
  }
bool DrawingPrice(const double price,string &why)
  {
   MqlTick tick={};
   if(!SymbolInfoTick(_Symbol,tick) || tick.bid<=0 || tick.ask<=0 || tick.ask<tick.bid) { why="Waiting for a valid quote."; return false; }
   if(plan.mode==CS_MARKET) plan.entry=LiveEntry();
   double minimum=(double)SymbolInfoInteger(_Symbol,SYMBOL_TRADE_STOPS_LEVEL)*_Point;
   double eps=SymbolInfoDouble(_Symbol,SYMBOL_TRADE_TICK_SIZE)*0.000001;
   if(draw_stage==CS_DRAW_SL)
     {
      if(plan.mode!=CS_MARKET) return price>0 && MathIsValidNumber(price);
      bool candidate=false;
      if(!DirectionForSL(price,candidate,why)) return false;
      double gap=candidate ? tick.bid-price : price-tick.ask;
      if(plan.mode==CS_MARKET && gap+eps<minimum) { why="SL is inside the broker stop distance."; return false; }
      plan.buy=candidate;
     }
   else if(draw_stage==CS_DRAW_ENTRY)
     {
      if(plan.mode==CS_MARKET) return false;
      bool candidate=false; CSOrderMode mode=CS_LIMIT;
      if(!ClassifyPending(price,plan.sl,candidate,mode,why)) return false;
      plan.buy=candidate; plan.mode=mode; direction_known=true; entry_ready=true;
     }
   else if(draw_stage==CS_DRAW_TP)
     {
      if(!direction_known) return false;
      double sign=plan.buy ? 1.0 : -1.0;
      double anchor=plan.mode==CS_MARKET ? (plan.buy ? tick.bid : tick.ask) : plan.entry;
      if(sign*(price-plan.entry)<=eps || sign*(price-anchor)+eps<minimum)
        { why=plan.buy ? "Place TP above entry." : "Place TP below entry."; return false; }
     }
   else return false;
   return true;
  }
void DrawingPointer(const int x,const int y,const bool commit)
  {
   if(draw_stage==CS_DRAW_NONE || !plan_active || folded || busy || !HaveLock()) return;
   if(x>=panel_x && x<=panel_x+Px(panel_width) && y>=panel_y && y<=panel_y+Px(panel_height)) return;
   if(x<0 || y<0 || x>=ChartGetInteger(0,CHART_WIDTH_IN_PIXELS) || y>=ChartGetInteger(0,CHART_HEIGHT_IN_PIXELS,0)) return;
   ulong now=GetTickCount64();
   if(commit && last_draw_click>0 && now-last_draw_click<180) return;
   if(!commit && now-last_pointer_render<32) return;
   int subwindow=-1; datetime time=0; double price=0;
   bool tag_hit=false;
   int active=draw_stage==CS_DRAW_SL ? 1 : (draw_stage==CS_DRAW_ENTRY ? 0 : 2);
   for(int i=0;i<5;i++)
     {
      if(tag_visible[i] && x>=tag_left[i] && x<=tag_right[i] && y>=tag_top[i] && y<=tag_bottom[i])
        {
         // Hovering a label must not move its line away before the click arrives.
         if(!commit || i!=active) return;
         price=tag_price[i]; tag_hit=true; break;
        }
     }
   if(!tag_hit && (!ChartXYToTimePrice(0,x,y,subwindow,time,price) || subwindow!=0)) return;
   price=engine.Price(price);
   if(price<=0 || !MathIsValidNumber(price)) return;
   if(draw_stage==CS_DRAW_SL)
     {
      plan.sl=price;
      bool candidate=false; string ignored="";
      if(DirectionForSL(price,candidate,ignored)) plan.buy=candidate;
     }
   else if(draw_stage==CS_DRAW_ENTRY) plan.entry=price;
   else plan.tp[0]=price;
   if(commit)
     {
      last_draw_click=now;
      string why="";
      if(draw_stage==CS_DRAW_TP && !LockedSLValid(why)) { RestartSL(); return; }
      if(!DrawingPrice(price,why)) { Say(why,"Check direction and broker stop distance."); SyncLines(); Render(); return; }
      if(draw_stage==CS_DRAW_SL)
        {
         direction_known=true;
         BeginEntryOrTP();
        }
      else if(draw_stage==CS_DRAW_ENTRY)
        {
         draw_stage=CS_DRAW_TP;
         double distance=MathAbs(plan.entry-plan.sl);
         plan.tp[0]=engine.Price(plan.entry+(plan.buy ? 1.0 : -1.0)*distance*InpRewardR);
         Say("3 / 3  Click to place TP1.",OrderTypeText()+" / "+(plan.buy ? "place TP above entry." : "place TP below entry."));
        }
      else
        {
         draw_stage=CS_DRAW_NONE; tp_ready=true;
         GenerateExtraTargets();
         Say("Ready. Click CONFIRM ORDER to send.","Drag lines to fine-tune. No order sent.");
         quote_status=true;
        }
      SyncLines(); Rebuild();
     }
   else
     {
      last_pointer_render=now;
      SyncLines();
      if(now-last_pointer_ui>=120) { last_pointer_ui=now; Render(); }
      else ChartRedraw();
     }
  }

bool Number(const string text,double &value)
  {
   string s=text; StringTrimLeft(s); StringTrimRight(s);
   bool digit=false,dot=false;
   for(int i=0;i<StringLen(s);i++)
     {
      ushort c=StringGetCharacter(s,i);
      if(c>='0' && c<='9') { digit=true; continue; }
      if(c=='.' && !dot) { dot=true; continue; }
      return false;
     }
   if(!digit) return false;
   value=StringToDouble(s);
   return MathIsValidNumber(value) && value>=0;
  }
void Commit(const string id)
  {
   bool price_field=(id=="entry" || id=="sl" || id=="tp0" || id=="tp1" || id=="tp2");
   if(id!="risk" && !price_field) return;
   Disarm();
   if(price_field && send_consumed) { Say("Start NEW ORDER or QUICK for another setup.","Check the last result before creating another order."); Rebuild(); return; }
   if(price_field && (!plan_active || draw_stage!=CS_DRAW_NONE || !tp_ready))
     { Say(plan_active ? "Finish drawing on the chart." : "Choose NEW ORDER or QUICK."); Rebuild(); return; }
   double value=0;
   if(!Number(ObjectGetString(0,N(id),OBJPROP_TEXT),value)) { Say("Invalid number.","Use digits and a decimal point."); Rebuild(); return; }
   if(id=="risk") { if(value<=0 || value>100) { Say("Risk must be above 0 and at most 100%."); Rebuild(); return; } plan.risk_percent=value; }
   else
     {
      if(value<=0) { Say("Price must be positive."); Rebuild(); return; }
      value=engine.Price(value);
      if(id=="entry") plan.entry=value;
      else if(id=="sl") plan.sl=value;
      else for(int i=0;i<3;i++) if(id=="tp"+IntegerToString(i)) plan.tp[i]=value;
     }
   if(id=="sl" && StopEdited()) return;
   if(id=="entry" && PendingEdited()) return;
   Say("Draft updated.","Check size and risk, then CONFIRM ORDER.");
   quote_status=true;
   SyncLines(); Rebuild();
  }

void Action(const string id)
  {
   if(closing || busy) return;
   if(id!="send" && id!="close" && id!="cancel") return;
   // A queued double-click must not resubmit a completed or ambiguous request.
   if(id=="send" && (send_consumed || GetTickCount64()<type_confirm_after)) return;
   if(!HaveLock()) { Say("Panel ownership lost. Remove and reattach EA."); Render(); return; }
   if(id=="send")
     {
      if(!plan_active || draw_stage!=CS_DRAW_NONE || !tp_ready || !direction_known || !entry_ready || (plan.mode!=CS_LIMIT && plan.mode!=CS_STOP && plan.mode!=CS_MARKET))
        { Disarm(); Say(plan_active ? "Finish drawing on the chart." : "Choose NEW ORDER or QUICK."); Render(); return; }
      string why="";
      if(!LockedSLValid(why)) { RestartSL(); return; }
      if(plan.mode!=CS_MARKET)
        {
         bool candidate=false; CSOrderMode mode=CS_LIMIT;
         if(!ClassifyPending(plan.entry,plan.sl,candidate,mode,why)) { Disarm(); Say(why); Render(); return; }
         if(mode!=plan.mode)
           {
            // A quote crossing Entry must not silently change the reviewed order type at send time.
            plan.mode=mode; Disarm(); type_confirm_after=GetTickCount64()+600;
            Say("Type updated: "+OrderTypeText()+".","Market crossed Entry. Click CONFIRM again.");
            SyncLines(); Rebuild(); return;
           }
        }
      if(!engine.Quote(plan,current_quote)) { Disarm(); Say(current_quote.error); Render(); return; }
     }
   else
     {
      ulong now=GetTickCount64();
      if(armed!=id || now>armed_until)
        {
         Disarm();
         armed=id; armed_until=now+8000;
         Say(id=="close" ? "Close this symbol + ID's positions?" : "Cancel this symbol + ID's pending orders?","Click the same button again within 8 seconds.");
         Render(); return;
        }
     }
   Disarm(); busy=true;
   if(id=="send") { tag_quote=current_quote; send_consumed=true; }
   string message="";
   bool ok=false;
   if(id=="send") ok=engine.Execute(plan,message);
   else if(id=="close") ok=engine.ClosePositions(message);
   else if(id=="cancel") ok=engine.CancelPending(message);
   Print("ClearSizer: ",message);
   Say((ok ? "Completed: " : "Stopped: ")+message,"Full result: hover this status or open the Experts log.");
   ObjectSetString(0,N("status_detail"),OBJPROP_TOOLTIP,message);
   busy=false; SyncLines(true); Render();
  }

int OnInit()
  {
   ResetSessionState();
   if(InpMagic==0 || InpUIScale<0.6 || InpUIScale>2.5 || InpRiskPercent<=0 || InpRiskPercent>100 || InpStopPoints<=0 || InpRewardR<=0 || InpCostPerLot<0) return INIT_PARAMETERS_INCORRECT;
   prefix="CS_"+(string)ChartID()+"_";
   instance_key="CS.L."+Base36(HashText(AccountInfoString(ACCOUNT_SERVER)+"/"+_Symbol))+"."+Base36((ulong)AccountInfoInteger(ACCOUNT_LOGIN))+"."+Base36(InpMagic);
   instance_owner=(double)ChartID(); if(instance_owner==0) instance_owner=1;
   // Creation and atomic occupation are separate so an existing released lock is reusable.
   if(!AcquireInstanceLock()) return INIT_FAILED;
   engine.Configure(_Symbol,InpMagic,InpDeviationPoints,InpCostPerLot);
   plan.buy=true; plan.mode=CS_LIMIT; plan.targets=1; plan.risk_percent=InpRiskPercent; plan.use_equity=InpUseEquity;
   ui_zoom=InpUIScale;
   old_mouse_move=(bool)ChartGetInteger(0,CHART_EVENT_MOUSE_MOVE);
   if(!old_mouse_move) mouse_move_changed=ChartSetInteger(0,CHART_EVENT_MOUSE_MOVE,true);
   plan_active=false; tp_ready=false; direction_known=false; draw_stage=CS_DRAW_NONE;
   Say("Choose NEW ORDER or QUICK.","Pending: SL, Entry, TP / Quick: SL, TP.");
   EventSetTimer(1);
   Rebuild();
   return INIT_SUCCEEDED;
  }
void OnDeinit(const int reason)
  {
   closing=true;
   // Release before any synchronous chart/object operation can exhaust the unload time budget.
   ReleaseInstanceLock();
   EventKillTimer();
   if(mouse_move_changed) ChartSetInteger(0,CHART_EVENT_MOUSE_MOVE,old_mouse_move);
   if(prefix!="") ObjectsDeleteAll(0,prefix);
   ChartRedraw();
  }
void OnTick()
  {
   // One fixed break-even rule: at +1 initial R, target the original entry price.
   if(!busy && HaveLock()) engine.Manage(be_enabled,false,1.0,1.0,0.0);
  }
void OnTimer()
  {
   if(closing) return;
   if(!HaveLock()) { Disarm(); Say("Panel ownership lost. Remove and reattach EA."); Render(); return; }
   if(armed!="" && GetTickCount64()>armed_until) { Disarm(); Say("Confirmation expired.","Review again when ready."); }
   SyncLines(true); Render();
  }
void OnChartEvent(const int id,const long &lparam,const double &dparam,const string &sparam)
  {
   if(closing) return;
   if(id==CHARTEVENT_KEYDOWN && lparam==27)
     {
      // ESC also clears a finished setup awaiting CONFIRM, never a submitted attempt.
      if(plan_active && !send_consumed && !busy) CancelDraft();
      else Disarm();
      Rebuild(); return;
     }
   if(id==CHARTEVENT_MOUSE_MOVE)
     { if(((uint)StringToInteger(sparam)&3)==0) DrawingPointer((int)lparam,(int)dparam,false); return; }
   if(id==CHARTEVENT_CLICK)
     { DrawingPointer((int)lparam,(int)dparam,true); return; }
   if(id==CHARTEVENT_OBJECT_CLICK && draw_stage!=CS_DRAW_NONE && (StringFind(sparam,N("line_"))==0 || StringFind(sparam,N("zone_"))==0 || StringFind(sparam,N("tag_"))==0))
     { DrawingPointer((int)lparam,(int)dparam,true); return; }
   if(id==CHARTEVENT_CHART_CHANGE) { Disarm(); SyncLines(); Rebuild(); return; }
   if(id==CHARTEVENT_OBJECT_ENDEDIT && StringFind(sparam,prefix)==0)
     { Commit(StringSubstr(sparam,StringLen(prefix))); return; }
   if(id==CHARTEVENT_OBJECT_DRAG && StringFind(sparam,N("line_"))==0)
     {
      if(draw_stage!=CS_DRAW_NONE || !tp_ready || send_consumed) return;
      Disarm();
      string field=StringSubstr(sparam,StringLen(N("line_")));
      double value=engine.Price(ObjectGetDouble(0,sparam,OBJPROP_PRICE));
      if(field=="entry" && plan.mode!=CS_MARKET) plan.entry=value;
      if(field=="sl") plan.sl=value;
      for(int i=0;i<3;i++) if(field=="tp"+IntegerToString(i)) plan.tp[i]=value;
      if(field=="sl" && StopEdited()) return;
      if(field=="entry" && plan.mode!=CS_MARKET && PendingEdited()) return;
      Say("Draft line updated.","Check size and risk, then CONFIRM ORDER.");
      quote_status=true;
      RefreshEdits(); SyncLines(); Render(); return;
     }
   if(id!=CHARTEVENT_OBJECT_CLICK || StringFind(sparam,prefix)!=0) return;
   string key=StringSubstr(sparam,StringLen(prefix));
   if(key=="remove_ea") { RemovePanel(); return; }
   if(key=="send" || key=="close" || key=="cancel") { Action(key); return; }
   if(key=="background" || key=="status" || key=="status_detail" || StringFind(key,"line_")==0 || StringFind(key,"zone_")==0 || StringFind(key,"tag_")==0) return;
   // Clicking in an edit must preserve its current text and keyboard focus.
   if(key=="risk" || key=="entry" || key=="sl" || key=="tp0" || key=="tp1" || key=="tp2") { Disarm(); return; }
   Disarm();
   if(key=="fold") { if(draw_stage!=CS_DRAW_NONE) CancelDraft(); folded=!folded; }
   else if(key=="previous") body_page=(int)MathMax(0,body_page-1);
   else if(key=="next") body_page=(int)MathMin(body_pages-1,body_page+1);
   else if(key=="zoom_down") ui_zoom=MathMax(0.6,ui_zoom-0.1);
   else if(key=="zoom_up") ui_zoom=MathMin(2.5,ui_zoom+0.1);
   else if(key=="new_order") { body_page=0; NewPlan(CS_LIMIT); }
   else if(key=="quick_order") { body_page=0; NewPlan(CS_MARKET); }
   else if(key=="discard") CancelDraft();
   else if(key=="toggle_tp2") ToggleTarget(false);
   else if(key=="toggle_tp3") ToggleTarget(true);
   else if(key=="basis") plan.use_equity=!plan.use_equity;
   else if(key=="risk_down") plan.risk_percent=MathMax(0.05,plan.risk_percent-0.25);
   else if(key=="risk_up") plan.risk_percent=MathMin(100,plan.risk_percent+0.25);
   else if(key=="be")
     {
      be_enabled=!be_enabled;
      Say(be_enabled ? "Break-even ON / this symbol + ID." : "Break-even disabled.",be_enabled ? "At +1R, move SL to entry (before fees)." : "Current SL stays in place.");
     }
   else return;
   SyncLines(); Rebuild();
  }
