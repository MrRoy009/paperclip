"""MNQ15-BAR-SCREEN-v1: exploratory adjusted-trade-bar study, not quote replay."""
from dataclasses import dataclass
from decimal import Decimal as D, ROUND_CEILING, getcontext
import argparse,calendar,csv,datetime as dt,hashlib,json,pathlib,sys
from zoneinfo import ZoneInfo
getcontext().prec=40
SEC=10**9; STEP=10*SEC; QUARTER=900*SEC; TICK=D('.25')
SCENARIOS=(1,2,4)

@dataclass(frozen=True)
class Bar:
 time:int
 o:D
 h:D
 l:D
 c:D

def features(bars):
 if len(bars)<9:return None
 w=bars[-9:]
 if any(b is None for b in w):return None
 hi=max(b.h for b in w[:-1]);lo=min(b.l for b in w[:-1])
 total=sum((max(w[i].h,w[i-1].c)-min(w[i].l,w[i-1].c))/TICK for i in range(1,9))
 distance=max(8,int((2*total/8).to_integral_value(rounding=ROUND_CEILING)))
 return hi,lo,distance

def session(open_ns,quotes,adverse=2):
 if adverse not in SCENARIOS:raise ValueError('unregistered cost scenario')
 for t,b in quotes.items():
  if t!=b.time or not open_ns<=t<open_ns+26*QUARTER or (t-open_ns)%STEP:raise ValueError('invalid session bucket')
  if min(b.o,b.h,b.l,b.c)<=0 or b.l>min(b.o,b.c) or b.h<max(b.o,b.c) or b.h<b.l:raise ValueError('invalid OHLC')
 agg=[];forming=[];trades=[];events=[];position=None;pending=None
 entries=0;cooldown=0;realized=D(0);max_mae=D(0);flat=False
 start=0;missing=0
 def log(t,reason,**details):events.append({'time':t,'reason':reason,**details})
 def close(t,raw,reason,timing):
  nonlocal position,pending,realized,cooldown,max_mae
  p=position;fill=raw-p['side']*adverse*TICK
  gross=(fill-p['entry_fill'])*p['side']*200
  rawgross=(raw-p['entry_raw'])*p['side']*200
  trade={**p,'exit_time':t,'exit_raw':raw,'exit_fill':fill,'exit_reason':reason,'exit_time_model':timing,
         'gross_cents':gross,'fees_cents':D(200),'net_cents':gross-200,
         'matched_raw_gross_cents':rawgross,'execution_drag_cents':rawgross-gross}
  trades.append(trade);realized+=trade['net_cents'];max_mae=max(max_mae,p['mae_cents'])
  cooldown=open_ns+((t-open_ns)//QUARTER+3)*QUARTER
  log(t,reason);position=None;pending=None
 for index in range(2340):
  t=open_ns+index*STEP
  if index and index%90==0:
   bar=None
   if len(forming)==90:bar=Bar(t,forming[0].o,max(b.h for b in forming),min(b.l for b in forming),forming[-1].c)
   agg.append(bar);forming=[]
   if not flat and pending is None:
    f=features(agg)
    if len(agg)<9:log(t,'WARMUP')
    elif f is None:log(t,'MISSING_DATA')
    else:
     hi,lo,distance=f;buy=bar.c>hi;sell=bar.c<lo
     if position is not None:
      if (position['side']==1 and sell) or (position['side']==-1 and buy):pending={'kind':'exit','time':t,'reason':'OPPOSITE_SIGNAL'}
     else:
      reason=('AFTER_ENTRY_CUTOFF' if t>open_ns+22*QUARTER else
              'MAX_ENTRIES' if entries>=3 else 'COOLDOWN' if t<cooldown else
              'NO_SIGNAL' if not buy and not sell else 'SIZE_ZERO' if D(distance)*50+600>5000 else None)
      if reason:log(t,reason,**({'distance_ticks':distance,'risk_per_contract_cents':distance*50+600,'signal':'LONG' if buy else 'SHORT'} if reason=='SIZE_ZERO' else {}))
      else:pending={'kind':'entry','time':t,'deadline':t+60*SEC,'side':1 if buy else -1,'distance':distance};log(t,'ENTRY_ORDER')
  if pending and pending['kind']=='entry' and t>=pending['deadline']:pending=None;log(t,'ENTRY_EXPIRED')
  if t>=open_ns+22800*SEC and not flat:
   flat=True
   if pending and pending['kind']=='entry':pending=None;log(t,'ENTRY_CANCELLED')
   if position and pending is None:pending={'kind':'exit','time':t,'reason':'FLAT_BY'}
   log(t,'FLAT_BY_TIMER')
  b=quotes.get(t)
  if b is None:missing+=1;continue
  forming.append(b)
  if pending:
   if pending['kind']=='entry':
    side=pending['side'];fill=b.o+side*adverse*TICK
    position={'entry_time':t,'entry_raw':b.o,'entry_fill':fill,'side':side,'distance_ticks':pending['distance'],
              'stop':fill-side*pending['distance']*TICK,'mae_cents':D(0)}
    entries+=1;pending=None;log(t,'ENTRY')
   else:close(t,b.o,pending['reason'],'10s_bar_open')
  if position:
   p=position;worst=b.l if p['side']==1 else b.h
   # Full-bar range may occur after stop/exit: conservative exposure diagnostic,
   # explicitly not marked-to-market account drawdown.
   p['mae_cents']=max(p['mae_cents'],max(D(0),(p['entry_fill']-worst)*p['side']*200+200+adverse*50))
   if (p['side']==1 and b.l<=p['stop']) or (p['side']==-1 and b.h>=p['stop']):
    gap=(p['side']==1 and b.o<=p['stop']) or (p['side']==-1 and b.o>=p['stop'])
    raw=min(b.o,p['stop']) if p['side']==1 else max(b.o,p['stop'])
    close(t if gap else t+STEP-1,raw,'STOP','gap_at_open' if gap else 'within_10s_bucket_time_unknown')
 # Complete last 15m bucket for coverage even though no decision follows flat-by.
 agg.append(Bar(open_ns+26*QUARTER,forming[0].o,max(b.h for b in forming),min(b.l for b in forming),forming[-1].c) if len(forming)==90 else None)
 failed=position is not None
 if failed:log(open_ns+26*QUARTER,'FLAT_BY_FAILURE')
 return {'trades':trades,'events':events,'entries':entries,'missing_10s':missing,'missing_15m':sum(b is None for b in agg),
         'flat_by_failure':failed,'unclosed_position':position,'net_cents':realized-(100 if failed else 0),'max_full_bar_mae_cents':max_mae}

def read(path,year,month):
 import pyarrow as pa
 import pyarrow.parquet as pq
 table=pq.read_table(path)
 required=['ts','open','high','low','close','volume','trade_count']
 if table.column_names!=required:raise ValueError('unexpected schema')
 if str(table.schema.field('ts').type)!='timestamp[ns, tz=UTC]':raise ValueError('UTC ns bucket timestamp required')
 data={k:(table[k].cast(pa.int64()).to_pylist() if k=='ts' else table[k].to_pylist()) for k in required}
 tz=ZoneInfo('America/Chicago');days={};opens={}
 for day in range(1,calendar.monthrange(year,month)[1]+1):
  date=dt.date(year,month,day)
  if date.weekday()<5:
   label=date.isoformat();op=int(dt.datetime.combine(date,dt.time(8,30),tzinfo=tz).timestamp())*SEC
   days[label]={};opens[label]=op
 prev=-1;outside=0;fractional=0;volumes=0
 for i,t in enumerate(data['ts']):
  if t is None or t<=prev or t%STEP:raise ValueError('duplicate, unordered, null or unaligned timestamp')
  prev=t
  utc=dt.datetime.fromtimestamp(t//SEC,dt.timezone.utc)
  if (utc.year,utc.month)!=(year,month):raise ValueError('row outside declared UTC sample month')
  prices=[]
  for k in ['open','high','low','close']:
   p=D(str(data[k][i]));
   if not p.is_finite() or p<=0:raise ValueError('invalid price')
   prices.append(p)
  o,h,l,c=prices
  if l>min(o,c) or h<max(o,c) or h<l:raise ValueError('OHLC containment failure')
  if any(p/TICK!=(p/TICK).to_integral_value() for p in prices):fractional+=1
  for k in ['volume','trade_count']:
   if data[k][i] is None or data[k][i]<0:raise ValueError('invalid volume/count')
  volumes+=data['volume'][i]
  date=utc.astimezone(tz).date().isoformat()
  if date not in days or not opens[date]<=t<opens[date]+26*QUARTER:outside+=1;continue
  days[date][t]=Bar(t,*prices)
 return days,opens,{'rows':table.num_rows,'outside_synthetic_session_rows':outside,'fractional_tick_ohlc_rows':fractional,
                    'total_volume_all_rows':volumes,'schema':str(table.schema),'first_timestamp_ns':data['ts'][0],'last_timestamp_ns':data['ts'][-1]}

def clean(value):
 if isinstance(value,D):return str(value)
 if isinstance(value,dict):return {k:clean(v) for k,v in value.items()}
 if isinstance(value,list):return [clean(v) for v in value]
 return value

def summarize(results,adverse):
 trades=[t for r in results.values() for t in r['trades']]
 pnl=[t['net_cents'] for t in trades];win=sum((v for v in pnl if v>0),D(0));loss=-sum((v for v in pnl if v<0),D(0))
 equity=D(0);peak=D(0);dd=D(0)
 for value in pnl:equity+=value;peak=max(peak,equity);dd=max(dd,peak-equity)
 rejected=[e for r in results.values() for e in r['events'] if e['reason']=='SIZE_ZERO']
 counts={}
 for r in results.values():
  for e in r['events']:counts[e['reason']]=counts.get(e['reason'],0)+1
 return {'adverse_ticks_per_side':adverse,'sessions':len(results),'trades':len(trades),'wins':sum(v>0 for v in pnl),'losses':sum(v<0 for v in pnl),'flat':sum(v==0 for v in pnl),
         'gross_cents':sum((t['gross_cents'] for t in trades),D(0)),'fees_cents':sum((t['fees_cents'] for t in trades),D(0)),
         'execution_drag_cents':sum((t['execution_drag_cents'] for t in trades),D(0)),
         'matched_raw_gross_cents':sum((t['matched_raw_gross_cents'] for t in trades),D(0)),
         'closed_trade_net_cents':sum(pnl,D(0)),'ledger_net_cents':sum((r['net_cents'] for r in results.values()),D(0)),
         'profit_factor':win/loss if loss else None,'worst_trade_cents':min(pnl) if pnl else None,
         'trade_close_drawdown_cents':dd,'max_full_bar_adverse_excursion_cents':max((r['max_full_bar_mae_cents'] for r in results.values()),default=D(0)),
         'sessions_with_missing_buckets':sum(r['missing_10s']>0 for r in results.values()),'missing_10s':sum(r['missing_10s'] for r in results.values()),
         'missing_15m':sum(r['missing_15m'] for r in results.values()),'unclosed_sessions':sum(r['flat_by_failure'] for r in results.values()),
         'complete_month_economic_summary_valid':not any(r['flat_by_failure'] or r['missing_10s'] for r in results.values()),
         'decision_reason_counts':counts,'size_rejected_signals':len(rejected),
         'rejected_stop_ticks_min':min((e['distance_ticks'] for e in rejected),default=None),
         'rejected_stop_ticks_max':max((e['distance_ticks'] for e in rejected),default=None),
         'minimum_required_risk_cents':min((e['risk_per_contract_cents'] for e in rejected),default=None)}

def run(path,plan,out,year=2026,month=10):
 if out.exists():raise ValueError('output exists')
 expected='dadae7d14298c599677a481ac8a4b522b21a20741169aad7928a603da43e0d6a'
 sha=hashlib.sha256(path.read_bytes()).hexdigest()
 if sha!=expected:raise ValueError('not the frozen source file')
 days,opens,quality=read(path,year,month)
 summaries=[];detail={}
 for adverse in SCENARIOS:
  results={date:session(opens[date],quotes,adverse) for date,quotes in days.items()}
  summaries.append(summarize(results,adverse));detail[str(adverse)]=results
 report={'variant':'MNQ15-BAR-SCREEN-v1','status':'EXPLORATORY_ADJUSTED_CONTINUOUS_BAR_STUDY','source_sha256':sha,
         'plan_sha256':hashlib.sha256(plan.read_bytes()).hexdigest(),'period':'2026-10','quality':quality,'scenarios':summaries,
         'quote_strategy_validated':False,'profitability_established':False,'holdout_used':False,
         'calendar':'synthetic Mon-Fri 08:30–15:00 America/Chicago, no exchange-calendar certification',
         'tzdata_version':__import__('importlib.metadata').metadata.version('tzdata'),
         'python':sys.version,'pyarrow':__import__('pyarrow').__version__,
         'notes':['prices are adjusted continuous trade bars, not dated contract executable quotes','spread filter unavailable; actual spread and stop event order unknown','fees and adverse fill costs are assumed, not actual','all scenarios rerun full strategy; no tuning or holdout','trade-close drawdown is not account drawdown; full-bar MAE is an exposure diagnostic, not actual realized loss','sparse indicator windows rejected, no filling missing bars','one exploratory month cannot establish edge']}
 out.mkdir(parents=True)
 (out/'summary.json').write_text(json.dumps(clean(report),indent=2)+'\n')
 (out/'sessions.json').write_text(json.dumps(clean(detail),indent=2)+'\n')
 for adverse in SCENARIOS:
  trades=[{'date':date,**t} for date,r in detail[str(adverse)].items() for t in r['trades']]
  with (out/f'trades-{adverse}-ticks.csv').open('w',newline='') as f:
   fields=list(trades[0]) if trades else ['date']
   writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader();writer.writerows(clean(trades))
 return report

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--data',type=pathlib.Path,required=True);p.add_argument('--plan',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True)
 a=p.parse_args();print(json.dumps(clean(run(a.data,a.plan,a.out)),indent=2))
