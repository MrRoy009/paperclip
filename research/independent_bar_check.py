"""Independent pandas rolling-window check of the no-trade historical result."""
import pandas as pd
import numpy as np
import json,pathlib,datetime as dt,argparse
p=argparse.ArgumentParser();p.add_argument('--data',type=pathlib.Path,default=pathlib.Path('outputs/online-mnq-history-check/cme.mnq.bars.10s.2026-10.parquet'));p.add_argument('--sessions',type=pathlib.Path,default=pathlib.Path('outputs/mnq-bar-study-october-2026-checked/sessions.json'));p.add_argument('--out',type=pathlib.Path,default=pathlib.Path('outputs/mnq-bar-study-october-2026-checked/independent-check.json'));args=p.parse_args()
data=args.data
frame=pd.read_parquet(data).set_index('ts').tz_convert('America/Chicago')
frame=frame[(frame.index.time>=dt.time(8,30))&(frame.index.time<dt.time(15))]
records=[];days=[]
for date,g in frame.groupby(frame.index.date):
 if date.month!=10 or date.weekday()>=5:continue
 bars=g.resample('15min',closed='left',label='right',origin='start_day').agg({'open':'first','high':'max','low':'min','close':'last','volume':'sum','trade_count':'count'})
 bars=bars[bars['trade_count']==90]
 assert len(bars)==26,(date,len(bars))
 high=bars.high.shift(1).rolling(8).max();low=bars.low.shift(1).rolling(8).min()
 previous=bars.close.shift(1)
 tr=pd.concat([bars.high-bars.low,(bars.high-previous).abs(),(bars.low-previous).abs()],axis=1).max(axis=1)
 distance=np.ceil(tr.rolling(8).sum()).clip(lower=8) # 2 * (TR points * 4 ticks/point) / 8
 signal=((bars.close>high)|(bars.close<low))&(bars.index.time<=dt.time(14))
 for timestamp in bars.index[signal]:
  d=int(distance.loc[timestamp]);records.append({'timestamp_ns':timestamp.value,'distance_ticks':d,'risk_cents':50*d+600})
 days.append(str(date))
detail=json.loads(args.sessions.read_text())['2']
observed=sorted((e['time'],e['distance_ticks'],e['risk_per_contract_cents']) for r in detail.values() for e in r['events'] if e['reason']=='SIZE_ZERO')
expected=sorted((r['timestamp_ns'],r['distance_ticks'],r['risk_cents']) for r in records)
assert expected==observed
assert len(records)==32 and min(r['risk_cents'] for r in records)==10600
assert all(r['risk_cents']>5000 for r in records)
report={'status':'PASS','independent_method':'pandas resampling and shifted rolling windows, separate from study engine',
        'complete_regular_day_sessions':len(days),'first_session':days[0],'last_session':days[-1],
        'exact_signal_time_distance_risk_matches':len(records),'minimum_risk_cents':min(r['risk_cents'] for r in records),
        'min_stop_ticks':min(r['distance_ticks'] for r in records),'max_stop_ticks':max(r['distance_ticks'] for r in records),
        'affordable_signals':sum(r['risk_cents']<=5000 for r in records),'signal_details':records}
args.out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='signal_details'},indent=2))
