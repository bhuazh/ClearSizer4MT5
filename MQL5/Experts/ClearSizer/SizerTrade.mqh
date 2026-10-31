#ifndef CLEAR_SIZER_TRADE_MQH
#define CLEAR_SIZER_TRADE_MQH

// Original ClearSizer trade engine. No third-party libraries or network calls.
enum CSOrderMode { CS_MARKET=0, CS_LIMIT=1, CS_STOP=2 };
struct CSPlan
{
   bool buy;
   CSOrderMode mode;
   double entry;
   double sl;
   double tp[3];
   int targets;
   double risk_percent;
   bool use_equity;
};
struct CSQuote
{
   bool valid;
   double entry,volume,risk_cash,risk_percent,reward_cash,rr,margin;
   double leg_volume[3];
   double leg_risk_cash[3],leg_reward_cash[3];
   string error;
};

class CSizerTrade
{
private:
   string m_symbol;
   ulong m_magic,m_deviation,m_next_log,m_next_manage;
   double m_cost;

   bool Positive(const double v) { return MathIsValidNumber(v) && v>0.0; }
   bool Fail(CSQuote &q,const string why) { q.valid=false; q.error=why; return false; }
   bool BasicPosition()
   {
      return PositionGetString(POSITION_SYMBOL)==m_symbol &&
             (ulong)PositionGetInteger(POSITION_MAGIC)==m_magic;
   }
   bool PositionProvenance(string &why)
   {
      if(IsHedging()) return true;
      ulong identifier=(ulong)PositionGetInteger(POSITION_IDENTIFIER);
      if(!HistorySelectByPosition(identifier))
      { why="Cannot verify netting position history; management and close skipped."; return false; }
      bool found_entry=false;
      for(int i=0;i<HistoryDealsTotal();i++)
      {
         ulong deal=HistoryDealGetTicket(i);
         if(deal==0) continue;
         long type=HistoryDealGetInteger(deal,DEAL_TYPE);
         if(type!=DEAL_TYPE_BUY && type!=DEAL_TYPE_SELL) continue;
         if((ulong)HistoryDealGetInteger(deal,DEAL_MAGIC)!=m_magic)
         { why="Mixed netting position: another EA or manual deal was found; management and close skipped."; return false; }
         long entry=HistoryDealGetInteger(deal,DEAL_ENTRY);
         if(entry==DEAL_ENTRY_IN || entry==DEAL_ENTRY_INOUT) found_entry=true;
      }
      if(!found_entry) why="No verified opening deal for netting position; management and close skipped.";
      return found_entry;
   }
   bool OwnPosition()
   {
      if(!BasicPosition()) return false;
      string why="";
      if(PositionProvenance(why)) return true;
      Log(why);
      return false;
   }
   bool OwnOrder()
   {
      return OrderGetString(ORDER_SYMBOL)==m_symbol &&
             (ulong)OrderGetInteger(ORDER_MAGIC)==m_magic;
   }
   bool BuyType(const ENUM_ORDER_TYPE t)
   {
      return t==ORDER_TYPE_BUY || t==ORDER_TYPE_BUY_LIMIT ||
             t==ORDER_TYPE_BUY_STOP || t==ORDER_TYPE_BUY_STOP_LIMIT;
   }
   bool PendingType(const ENUM_ORDER_TYPE t)
   {
      return t==ORDER_TYPE_BUY_LIMIT || t==ORDER_TYPE_SELL_LIMIT ||
             t==ORDER_TYPE_BUY_STOP || t==ORDER_TYPE_SELL_STOP ||
             t==ORDER_TYPE_BUY_STOP_LIMIT || t==ORDER_TYPE_SELL_STOP_LIMIT;
   }
   bool AnySymbolExposure()
   {
      for(int i=PositionsTotal()-1;i>=0;i--)
         if(PositionGetTicket(i)>0 && PositionGetString(POSITION_SYMBOL)==m_symbol) return true;
      for(int i=OrdersTotal()-1;i>=0;i--)
         if(OrderGetTicket(i)>0 && OrderGetString(ORDER_SYMBOL)==m_symbol) return true;
      return false;
   }
   double DirectionVolume(const bool buy)
   {
      double v=0;
      for(int i=PositionsTotal()-1;i>=0;i--)
         if(PositionGetTicket(i)>0 && PositionGetString(POSITION_SYMBOL)==m_symbol &&
            (PositionGetInteger(POSITION_TYPE)==POSITION_TYPE_BUY)==buy)
            v+=PositionGetDouble(POSITION_VOLUME);
      for(int i=OrdersTotal()-1;i>=0;i--)
         if(OrderGetTicket(i)>0 && OrderGetString(ORDER_SYMBOL)==m_symbol &&
            BuyType((ENUM_ORDER_TYPE)OrderGetInteger(ORDER_TYPE))==buy)
            v+=OrderGetDouble(ORDER_VOLUME_CURRENT);
      return v;
   }
   bool Permission(string &why)
   {
      if(!TerminalInfoInteger(TERMINAL_CONNECTED)) why="Terminal is disconnected.";
      else if(!TerminalInfoInteger(TERMINAL_TRADE_ALLOWED)) why="Enable Algo Trading in MT5.";
      else if(!MQLInfoInteger(MQL_TRADE_ALLOWED)) why="Allow Algo Trading in this EA's settings.";
      else if(!AccountInfoInteger(ACCOUNT_TRADE_ALLOWED)) why="Trading is disabled for this account.";
      else if(!AccountInfoInteger(ACCOUNT_TRADE_EXPERT)) why="Broker disallows Expert Advisor trading.";
      else return true;
      return false;
   }
   bool FillPolicy(const bool pending,ENUM_ORDER_TYPE_FILLING &fill,string &why)
   {
      if(pending) { fill=ORDER_FILLING_RETURN; return true; }
      long execution=SymbolInfoInteger(m_symbol,SYMBOL_TRADE_EXEMODE);
      long flags=SymbolInfoInteger(m_symbol,SYMBOL_FILLING_MODE);
      if(execution==SYMBOL_TRADE_EXECUTION_INSTANT || execution==SYMBOL_TRADE_EXECUTION_REQUEST ||
         (flags & SYMBOL_FILLING_FOK)!=0) fill=ORDER_FILLING_FOK;
      else if((flags & SYMBOL_FILLING_IOC)!=0) fill=ORDER_FILLING_IOC;
      else if(execution!=SYMBOL_TRADE_EXECUTION_MARKET) fill=ORDER_FILLING_RETURN;
      else { why="Broker provides no supported market filling policy."; return false; }
      return true;
   }
   ENUM_ORDER_TYPE PlanType(const CSPlan &p)
   {
      if(p.mode==CS_LIMIT) return p.buy ? ORDER_TYPE_BUY_LIMIT : ORDER_TYPE_SELL_LIMIT;
      if(p.mode==CS_STOP) return p.buy ? ORDER_TYPE_BUY_STOP : ORDER_TYPE_SELL_STOP;
      return p.buy ? ORDER_TYPE_BUY : ORDER_TYPE_SELL;
   }
   bool Request(const CSPlan &p,const CSQuote &q,const int leg,MqlTradeRequest &r,string &why)
   {
      ZeroMemory(r);
      r.action=p.mode==CS_MARKET ? TRADE_ACTION_DEAL : TRADE_ACTION_PENDING;
      r.symbol=m_symbol; r.magic=m_magic; r.deviation=m_deviation;
      r.type=PlanType(p); r.price=q.entry; r.sl=Price(p.sl); r.tp=Price(p.tp[leg]);
      r.volume=q.leg_volume[leg];
      double point=SymbolInfoDouble(m_symbol,SYMBOL_POINT);
      r.comment="CS:"+DoubleToString(MathAbs(q.entry-r.sl)/point,8);
      if(StringLen(r.comment)>31) { why="Initial risk exceeds order comment capacity."; return false; }
      if(!FillPolicy(p.mode!=CS_MARKET,r.type_filling,why)) return false;
      if(p.mode!=CS_MARKET)
      {
         long expiration=SymbolInfoInteger(m_symbol,SYMBOL_EXPIRATION_MODE);
         if((expiration & SYMBOL_EXPIRATION_GTC)!=0) r.type_time=ORDER_TIME_GTC;
         else if((expiration & SYMBOL_EXPIRATION_DAY)!=0) r.type_time=ORDER_TIME_DAY;
         else { why="Broker requires an expiry time; this panel supports GTC or DAY only."; return false; }
      }
      return true;
   }
   bool Check(MqlTradeRequest &r,MqlTradeCheckResult &c,string &why)
   {
      ZeroMemory(c); ResetLastError();
      bool ok=OrderCheck(r,c);
      if(ok && (c.retcode==0 || c.retcode==TRADE_RETCODE_DONE)) return true;
      why=StringFormat("Check %u: %s (error %d)",c.retcode,c.comment,GetLastError());
      return false;
   }
   string ResultText(const MqlTradeResult &r,const int error)
   {
      return StringFormat("Server %u: %s (error %d; order %I64u; deal %I64u)",
                          r.retcode,r.comment,error,r.order,r.deal);
   }
   void Log(const string text)
   {
      ulong now=GetTickCount64();
      if(now>=m_next_log) { Print("ClearSizer: ",text); m_next_log=now+30000; }
   }
   string Base36(ulong n)
   {
      string digits="0123456789abcdefghijklmnopqrstuvwxyz",s="";
      do { s=StringSubstr(digits,(int)(n%36),1)+s; n/=36; } while(n>0);
      return s;
   }
   string RiskKey(const ulong identifier)
   {
      // Include the server because login numbers can repeat across brokers.
      string server=AccountInfoString(ACCOUNT_SERVER);
      uint hash=2166136261;
      for(int i=0;i<StringLen(server);i++) hash=(uint)((hash^(uint)StringGetCharacter(server,i))*16777619);
      // Base 36 keeps this key below MT5's 63 character limit.
      return "CS.R."+Base36(hash)+"."+Base36((ulong)AccountInfoInteger(ACCOUNT_LOGIN))+"."+
             Base36(m_magic)+"."+Base36(identifier);
   }
   double InitialRisk()
   {
      ulong identifier=(ulong)PositionGetInteger(POSITION_IDENTIFIER);
      string key=RiskKey(identifier);
      double risk=0;
      if(GlobalVariableGet(key,risk) && Positive(risk)) return risk;
      string comment=PositionGetString(POSITION_COMMENT);
      double open=PositionGetDouble(POSITION_PRICE_OPEN);
      double sign=PositionGetInteger(POSITION_TYPE)==POSITION_TYPE_BUY ? 1.0 : -1.0;
      // Entry-deal SL is the original order SL, even after position SL was moved.
      // This also recovers actual initial R after slippage or broker comment changes.
      if(HistorySelectByPosition(identifier))
      {
         for(int i=0;i<HistoryDealsTotal();i++)
         {
            ulong deal=HistoryDealGetTicket(i);
            if(deal==0 || HistoryDealGetInteger(deal,DEAL_ENTRY)!=DEAL_ENTRY_IN ||
               (ulong)HistoryDealGetInteger(deal,DEAL_MAGIC)!=m_magic ||
               HistoryDealGetString(deal,DEAL_SYMBOL)!=m_symbol) continue;
            double original_sl=HistoryDealGetDouble(deal,DEAL_SL);
            if(Positive(original_sl)) risk=sign*(open-original_sl);
            break;
         }
      }
      if(!Positive(risk))
      {
         int start=StringFind(comment,"CS:");
         if(start<0) return 0;
         risk=StringToDouble(StringSubstr(comment,start+3))*SymbolInfoDouble(m_symbol,SYMBOL_POINT);
      }
      if(!Positive(risk)) return 0;
      if(GlobalVariableSet(key,risk)==0) Log("Cannot persist initial risk; order comment remains available.");
      else GlobalVariablesFlush();
      return risk;
   }
   void RecoverRisk()
   {
      for(int i=PositionsTotal()-1;i>=0;i--)
         if(PositionGetTicket(i)>0 && OwnPosition()) InitialRisk();
   }
   bool VolumeLegal(const double v,const double minimum,const double maximum,const double step)
   {
      return Positive(v) && v>=minimum-step*1e-7 && v<=maximum+step*1e-7 &&
             MathAbs(v/step-MathRound(v/step))<1e-6;
   }

public:
   void Configure(const string symbol,const ulong magic,const ulong deviation,const double cost_per_lot)
   {
      m_symbol=symbol; m_magic=magic; m_deviation=deviation;
      m_cost=(MathIsValidNumber(cost_per_lot) && cost_per_lot>=0) ? cost_per_lot : -1;
      m_next_log=0; m_next_manage=0;
      SymbolSelect(m_symbol,true); RecoverRisk();
   }
   bool IsHedging()
   {
      return AccountInfoInteger(ACCOUNT_MARGIN_MODE)==ACCOUNT_MARGIN_MODE_RETAIL_HEDGING;
   }
   double Price(const double value)
   {
      double tick=SymbolInfoDouble(m_symbol,SYMBOL_TRADE_TICK_SIZE);
      int digits=(int)SymbolInfoInteger(m_symbol,SYMBOL_DIGITS);
      if(!Positive(tick) || !MathIsValidNumber(value)) return 0;
      return NormalizeDouble(MathRound(value/tick)*tick,digits);
   }
   bool Quote(const CSPlan &p,CSQuote &q)
   {
      ZeroMemory(q);
      if(p.mode<CS_MARKET || p.mode>CS_STOP) return Fail(q,"Invalid order mode.");
      if(p.targets<1 || p.targets>3) return Fail(q,"Choose one, two or three targets.");
      if(!Positive(p.risk_percent) || p.risk_percent>100) return Fail(q,"Risk must be greater than 0 and at most 100%.");
      if(m_cost<0) return Fail(q,"Cost per lot must be a nonnegative account-currency amount.");
      if(!IsHedging() && p.targets!=1) return Fail(q,"Multiple targets require a hedging account.");
      if(!IsHedging() && AnySymbolExposure())
         return Fail(q,"Netting: this symbol already has a position or order. No merging is allowed.");
      MqlTick tick={};
      if(!SymbolInfoTick(m_symbol,tick) || !Positive(tick.ask) || !Positive(tick.bid) || tick.ask<tick.bid)
         return Fail(q,"No valid Bid/Ask quote is available.");
      double point=SymbolInfoDouble(m_symbol,SYMBOL_POINT);
      double tick_size=SymbolInfoDouble(m_symbol,SYMBOL_TRADE_TICK_SIZE);
      if(!Positive(point) || !Positive(tick_size)) return Fail(q,"Invalid broker point or tick size.");
      q.entry=Price(p.mode==CS_MARKET ? (p.buy ? tick.ask : tick.bid) : p.entry);
      double sl=Price(p.sl),sign=p.buy ? 1.0 : -1.0;
      if(!Positive(q.entry) || !Positive(sl)) return Fail(q,"Entry and stop loss must be positive prices.");
      long mode=SymbolInfoInteger(m_symbol,SYMBOL_TRADE_MODE);
      if(mode==SYMBOL_TRADE_MODE_DISABLED || mode==SYMBOL_TRADE_MODE_CLOSEONLY ||
         (mode==SYMBOL_TRADE_MODE_LONGONLY && !p.buy) || (mode==SYMBOL_TRADE_MODE_SHORTONLY && p.buy))
         return Fail(q,"Broker does not allow this trade direction.");
      long flags=SymbolInfoInteger(m_symbol,SYMBOL_ORDER_MODE);
      long required=p.mode==CS_MARKET ? SYMBOL_ORDER_MARKET : (p.mode==CS_LIMIT ? SYMBOL_ORDER_LIMIT : SYMBOL_ORDER_STOP);
      if((flags & required)==0 || (flags & SYMBOL_ORDER_SL)==0 || (flags & SYMBOL_ORDER_TP)==0)
         return Fail(q,"Broker does not allow the requested order, SL or TP type.");
      double distance=(double)SymbolInfoInteger(m_symbol,SYMBOL_TRADE_STOPS_LEVEL)*point;
      double epsilon=tick_size*1e-7;
      double close_price=p.buy ? tick.bid : tick.ask;
      double anchor=p.mode==CS_MARKET ? close_price : q.entry;
      if(sign*(q.entry-sl)<=epsilon || sign*(anchor-sl)+epsilon<distance)
         return Fail(q,"SL is on the wrong side or inside the broker's stop distance.");
      if(p.mode!=CS_MARKET)
      {
         double market=p.buy ? tick.ask : tick.bid;
         double gap=sign*(q.entry-market)*(p.mode==CS_LIMIT ? -1.0 : 1.0);
         if(gap<=epsilon || gap+epsilon<distance)
            return Fail(q,"Pending entry is on the wrong side or too close to market.");
      }
      double previous=q.entry;
      for(int i=0;i<p.targets;i++)
      {
         double tp=Price(p.tp[i]);
         if(!Positive(tp) || sign*(tp-previous)<=epsilon || sign*(tp-anchor)+epsilon<distance)
            return Fail(q,"Targets must progress away from entry and satisfy broker stop distance.");
         previous=tp;
      }
      double minimum=SymbolInfoDouble(m_symbol,SYMBOL_VOLUME_MIN);
      double maximum=SymbolInfoDouble(m_symbol,SYMBOL_VOLUME_MAX);
      double step=SymbolInfoDouble(m_symbol,SYMBOL_VOLUME_STEP);
      if(!Positive(minimum) || !Positive(maximum) || !Positive(step) || maximum<minimum)
         return Fail(q,"Broker volume minimum, maximum or step is invalid.");
      ENUM_ORDER_TYPE side=p.buy ? ORDER_TYPE_BUY : ORDER_TYPE_SELL;
      double loss=0;
      // Use the broker's minimum legal volume, never assume that 1 lot is legal.
      if(!OrderCalcProfit(side,m_symbol,minimum,q.entry,sl,loss) || !MathIsValidNumber(loss) || loss>=0)
         return Fail(q,"Broker cannot calculate the stop-loss value in account currency.");
      double loss_per_lot=-loss/minimum+m_cost;
      double capital=AccountInfoDouble(p.use_equity ? ACCOUNT_EQUITY : ACCOUNT_BALANCE);
      if(!Positive(capital) || !Positive(loss_per_lot)) return Fail(q,"Invalid balance, equity or stop-loss value.");
      double budget=capital*p.risk_percent/100.0;
      double raw=budget/loss_per_lot;
      if(!Positive(raw)) return Fail(q,"Risk budget cannot fund a valid volume.");
      q.volume=NormalizeDouble(MathFloor(raw/step)*step,8);
      // Never round up to minimum or allow floating-point tolerance to exceed budget.
      if(q.volume*loss_per_lot>budget)
         q.volume=NormalizeDouble(q.volume-step,8);
      if(q.volume<minimum-step*1e-7) return Fail(q,"Calculated volume is below the broker minimum; increase risk or reduce SL distance.");
      double units=MathFloor(q.volume/step+1e-7),base=MathFloor(units/p.targets);
      for(int i=0;i<p.targets;i++)
      {
         q.leg_volume[i]=NormalizeDouble((i==p.targets-1 ? units-base*(p.targets-1) : base)*step,8);
         if(!VolumeLegal(q.leg_volume[i],minimum,maximum,step))
            return Fail(q,"Each target must meet broker min/max/step volume. Change risk or target count.");
      }
      double limit=SymbolInfoDouble(m_symbol,SYMBOL_VOLUME_LIMIT);
      if(Positive(limit) && DirectionVolume(p.buy)+q.volume>limit+step*1e-7)
         return Fail(q,"This trade exceeds the broker's aggregate directional volume limit.");
      if(p.mode!=CS_MARKET)
      {
         long order_limit=AccountInfoInteger(ACCOUNT_LIMIT_ORDERS);
         if(order_limit>0 && OrdersTotal()+p.targets>order_limit)
            return Fail(q,"This trade exceeds the account's active order limit.");
      }
      for(int i=0;i<p.targets;i++)
      {
         double margin=0,reward=0;
         if(!OrderCalcMargin(side,m_symbol,q.leg_volume[i],q.entry,margin) ||
            !MathIsValidNumber(margin) || margin<0)
            return Fail(q,"Broker cannot calculate required margin.");
         // Profit is proportional to volume; calculation uses the same legal reference volume.
         if(!OrderCalcProfit(side,m_symbol,minimum,q.entry,Price(p.tp[i]),reward) || !Positive(reward))
            return Fail(q,"Broker cannot calculate the target value in account currency.");
         q.margin+=margin;
         q.leg_risk_cash[i]=loss_per_lot*q.leg_volume[i];
         q.leg_reward_cash[i]=reward/minimum*q.leg_volume[i]-m_cost*q.leg_volume[i];
         q.reward_cash+=q.leg_reward_cash[i];
      }
      double free_margin=AccountInfoDouble(ACCOUNT_MARGIN_FREE);
      if(!MathIsValidNumber(free_margin) || free_margin<0 || q.margin>free_margin+1e-8)
         return Fail(q,"Insufficient free margin for all target orders.");
      q.risk_cash=q.volume*loss_per_lot;
      q.risk_percent=q.risk_cash/capital*100.0;
      q.rr=q.reward_cash/q.risk_cash;
      q.valid=true; q.error="";
      return true;
   }
   bool Execute(const CSPlan &p,string &message)
   {
      if(!Permission(message)) return false;
      CSQuote q;
      // Quote obtains a fresh Bid/Ask and recalculates size at the moment of execution.
      if(!Quote(p,q)) { message=q.error; return false; }
      MqlTradeRequest requests[3];
      double checked_margin=0,account_margin=AccountInfoDouble(ACCOUNT_MARGIN);
      for(int i=0;i<p.targets;i++)
      {
         if(!Request(p,q,i,requests[i],message)) return false;
         MqlTradeCheckResult check={};
         if(!Check(requests[i],check,message))
         { message=StringFormat("Target %d preflight failed: %s No orders sent.",i+1,message); return false; }
         checked_margin+=MathMax(0.0,check.margin-account_margin);
      }
      if(MathMax(checked_margin,q.margin)>AccountInfoDouble(ACCOUNT_MARGIN_FREE)+1e-8)
      { message="Insufficient aggregate free margin after broker checks. No orders sent."; return false; }
      int accepted=0;
      for(int i=0;i<p.targets;i++)
      {
         if(!Permission(message))
         { message=StringFormat("%d of %d targets accepted; stopped: %s",accepted,p.targets,message); return false; }
         // A netting account can never receive a second leg or merge with another strategy.
         if(!IsHedging() && AnySymbolExposure())
         { message="Netting exposure changed during preflight. No order sent."; return false; }
         MqlTradeResult result={}; ResetLastError();
         bool sent=OrderSend(requests[i],result);
         int error=GetLastError();
         PrintFormat("ClearSizer target %d: %s",i+1,ResultText(result,error));
         if(!sent || (result.retcode!=TRADE_RETCODE_DONE && result.retcode!=TRADE_RETCODE_PLACED))
         {
            string detail=ResultText(result,error);
            if(result.retcode==TRADE_RETCODE_DONE_PARTIAL)
               detail=StringFormat("Target %d partially filled %.8f of %.8f lots. %s",i+1,result.volume,requests[i].volume,detail);
            message=StringFormat("%d of %d targets accepted before stop. %s Remaining targets were not sent; inspect Positions/Orders.",accepted,p.targets,detail);
            RecoverRisk(); return false;
         }
         accepted++;
         if(p.mode==CS_MARKET && result.retcode==TRADE_RETCODE_PLACED)
         {
            message=StringFormat("Target %d accepted, execution pending (order %I64u). %d later targets not sent. Inspect Orders/Positions before retrying.",i+1,result.order,p.targets-accepted);
            RecoverRisk(); return false;
         }
      }
      RecoverRisk();
      message=StringFormat("%d target order(s) %s; %.8f lots. Estimated risk %.2f %s.",accepted,
                          p.mode==CS_MARKET ? "executed" : "placed",q.volume,q.risk_cash,AccountInfoString(ACCOUNT_CURRENCY));
      return true;
   }
   void Manage(const bool be,const bool trail,const double trigger_r,const double trail_r,const double offset_points)
   {
      if((!be && !trail) || !Positive(trigger_r) || (trail && !Positive(trail_r)) ||
         !MathIsValidNumber(offset_points) || offset_points<0) return;
      ulong now=GetTickCount64();
      if(now<m_next_manage) return;
      m_next_manage=now+1000;
      string why;
      if(!Permission(why)) { Log(why); return; }
      MqlTick tick={};
      if(!SymbolInfoTick(m_symbol,tick) || !Positive(tick.bid) || !Positive(tick.ask)) return;
      double point=SymbolInfoDouble(m_symbol,SYMBOL_POINT),size=SymbolInfoDouble(m_symbol,SYMBOL_TRADE_TICK_SIZE);
      if(!Positive(point) || !Positive(size)) return;
      double gap=MathMax((double)SymbolInfoInteger(m_symbol,SYMBOL_TRADE_STOPS_LEVEL),
                         (double)SymbolInfoInteger(m_symbol,SYMBOL_TRADE_FREEZE_LEVEL))*point+size;
      for(int i=PositionsTotal()-1;i>=0;i--)
      {
         ulong ticket=PositionGetTicket(i);
         if(ticket==0 || !OwnPosition()) continue;
         double risk=InitialRisk();
         if(!Positive(risk)) { Log("Missing original R: cannot manage ticket "+(string)ticket); continue; }
         bool buy=PositionGetInteger(POSITION_TYPE)==POSITION_TYPE_BUY;
         double sign=buy ? 1.0 : -1.0,open=PositionGetDouble(POSITION_PRICE_OPEN);
         double current=buy ? tick.bid : tick.ask,old=PositionGetDouble(POSITION_SL);
         double tp=PositionGetDouble(POSITION_TP);
         if(sign*(current-open)<trigger_r*risk) continue;
         double desired=0;
         if(be) desired=open+sign*offset_points*point;
         if(trail)
         {
            double candidate=current-sign*trail_r*risk;
            if(desired==0 || sign*(candidate-desired)>0) desired=candidate;
         }
         desired=Price(desired);
         if(!Positive(desired) || sign*(current-desired)<gap ||
            (old>0 && sign*(desired-old)<size*0.999) ||
            (old>0 && sign*(current-old)<gap) || (tp>0 && sign*(tp-current)<gap)) continue;
         MqlTradeRequest request={}; MqlTradeResult result={};
         request.action=TRADE_ACTION_SLTP; request.symbol=m_symbol; request.magic=m_magic;
         request.position=ticket; request.sl=desired; request.tp=tp;
         ResetLastError();
         if(!OrderSend(request,result) ||
            (result.retcode!=TRADE_RETCODE_DONE && result.retcode!=TRADE_RETCODE_NO_CHANGES))
         {
            Log("SL management ticket "+(string)ticket+": "+ResultText(result,GetLastError()));
            m_next_manage=now+10000;
         }
      }
   }
   int PositionCount()
   {
      int count=0;
      for(int i=PositionsTotal()-1;i>=0;i--) if(PositionGetTicket(i)>0 && OwnPosition()) count++;
      return count;
   }
   int PendingCount()
   {
      int count=0;
      for(int i=OrdersTotal()-1;i>=0;i--)
         if(OrderGetTicket(i)>0 && OwnOrder() && PendingType((ENUM_ORDER_TYPE)OrderGetInteger(ORDER_TYPE))) count++;
      return count;
   }
   bool ClosePositions(string &message)
   {
      if(!Permission(message)) return false;
      ulong tickets[];
      int excluded=0;
      // Snapshot first so changes in server collections cannot skip tickets.
      for(int i=0;i<PositionsTotal();i++)
         if(PositionGetTicket(i)>0 && BasicPosition())
         {
            if(!OwnPosition()) { excluded++; continue; }
            int n=ArraySize(tickets); ArrayResize(tickets,n+1); tickets[n]=(ulong)PositionGetInteger(POSITION_TICKET);
         }
      int closed=0,partial=0,pending=0,failed=0,gone=0;
      string detail="";
      for(int i=0;i<ArraySize(tickets);i++)
      {
         if(!PositionSelectByTicket(tickets[i])) { gone++; continue; }
         if(!OwnPosition()) { excluded++; continue; }
         MqlTick tick={}; MqlTradeRequest r={}; MqlTradeResult result={};
         string why="";
         if(!Permission(why) || !SymbolInfoTick(m_symbol,tick) || !Positive(tick.ask) || !Positive(tick.bid))
         { failed++; detail="No valid quote or trading permission. "+why; continue; }
         bool buy=PositionGetInteger(POSITION_TYPE)==POSITION_TYPE_BUY;
         r.action=TRADE_ACTION_DEAL; r.position=tickets[i]; r.symbol=m_symbol; r.magic=m_magic;
         r.volume=PositionGetDouble(POSITION_VOLUME); r.type=buy ? ORDER_TYPE_SELL : ORDER_TYPE_BUY;
         r.price=Price(buy ? tick.bid : tick.ask); r.deviation=m_deviation; r.comment="CS close";
         MqlTradeCheckResult check={};
         if(!FillPolicy(false,r.type_filling,why) || !Check(r,check,why))
         { failed++; detail="Ticket "+(string)tickets[i]+": "+why; Print("ClearSizer: ",detail); continue; }
         ResetLastError(); bool sent=OrderSend(r,result);
         string status=ResultText(result,GetLastError());
         if(sent && result.retcode==TRADE_RETCODE_DONE) closed++;
         else if(sent && result.retcode==TRADE_RETCODE_DONE_PARTIAL) { partial++; detail=status; }
         else if(sent && result.retcode==TRADE_RETCODE_PLACED) { pending++; detail=status; }
         else { failed++; detail=status; }
         PrintFormat("ClearSizer close ticket %I64u: %s",tickets[i],status);
      }
      message=StringFormat("Close: %d completed, %d partial, %d awaiting execution, %d failed, %d already absent, %d excluded.",closed,partial,pending,failed,gone,excluded);
      if(excluded>0) message+=" Mixed or unverified netting ownership was skipped; see Experts journal.";
      if(detail!="") message+=" "+detail+" Inspect Positions/Orders before retrying.";
      return failed==0 && partial==0 && pending==0 && excluded==0;
   }
   bool CancelPending(string &message)
   {
      if(!Permission(message)) return false;
      ulong tickets[];
      for(int i=0;i<OrdersTotal();i++)
         if(OrderGetTicket(i)>0 && OwnOrder() && PendingType((ENUM_ORDER_TYPE)OrderGetInteger(ORDER_TYPE)))
         { int n=ArraySize(tickets); ArrayResize(tickets,n+1); tickets[n]=(ulong)OrderGetInteger(ORDER_TICKET); }
      int canceled=0,failed=0,gone=0;
      string detail="";
      for(int i=0;i<ArraySize(tickets);i++)
      {
         if(!OrderSelect(tickets[i]) || !OwnOrder()) { gone++; continue; }
         MqlTradeRequest r={}; MqlTradeResult result={};
         r.action=TRADE_ACTION_REMOVE; r.order=tickets[i]; r.symbol=m_symbol; r.magic=m_magic;
         ResetLastError(); bool sent=OrderSend(r,result);
         string status=ResultText(result,GetLastError());
         if(sent && result.retcode==TRADE_RETCODE_DONE) canceled++;
         else { failed++; detail="Ticket "+(string)tickets[i]+": "+status; }
         PrintFormat("ClearSizer cancel ticket %I64u: %s",tickets[i],status);
      }
      message=StringFormat("Cancel: %d canceled, %d failed, %d already absent.",canceled,failed,gone);
      if(detail!="") message+=" "+detail+" See Experts journal.";
      return failed==0;
   }
};
#endif
