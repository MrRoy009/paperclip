"""Offline, bounded MNQ quote replay. No network or paid dependencies."""
import argparse, csv, datetime as dt, decimal, hashlib, io, json, pathlib, re
import subprocess, sys, tempfile, zoneinfo

MAX_ROWS = 25000
I64 = (1 << 63)-1
ROOT = pathlib.Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()

def require(ok, message):
    if not ok:
        raise ValueError(message)

def ticks(value):
    require(len(value)<=28,'price text too long')
    require(bool(re.fullmatch(r'[0-9]+(?:\.[0-9]{1,9})?', value)), 'price must be positive decimal points')
    with decimal.localcontext() as context:
        context.prec=40
        p = decimal.Decimal(value)*4
    require(p == int(p) and 0 < p <= I64, 'off-grid, nonpositive or overflowing price')
    return int(p)

def timezone():
    # Require a pinned local IANA database and record the exact CT data bytes.
    for base in zoneinfo.TZPATH:
        file = pathlib.Path(base)/'America/Chicago'
        version = pathlib.Path(base)/'tzdata.zi'
        if file.is_file() and version.is_file():
            return zoneinfo.ZoneInfo.from_file(io.BytesIO(file.read_bytes()),key='America/Chicago'), {
                'zone': 'America/Chicago', 'version': version.read_text().splitlines()[0],
                'sha256': digest(file)}
    raise ValueError('local pinned IANA tzdata required; no automatic download')

def load(manifest_path, csv_path):
    manifest = json.loads(manifest_path.read_text())
    for key in ['kind','contract','source','timestamp_basis','quote_coverage','adjustment','calendar_policy','sessions']:
        require(key in manifest, 'manifest missing '+key)
    require(manifest['kind'] in ['synthetic','historical_user_supplied'], 'unknown provenance kind')
    require(bool(re.fullmatch(r'MNQ[HMUZ][0-9]{2}',manifest['contract'])), 'one dated MNQ contract required, e.g. MNQZ24')
    require(isinstance(manifest['source'],str) and bool(manifest['source'].strip()), 'source provenance required')
    require(manifest['timestamp_basis']=='UTC_UNIX_NS', 'UTC nanoseconds required')
    require(manifest['quote_coverage']=='event_bbo', 'event-level bid/ask required; no candle or trade-only substitution')
    require(manifest['adjustment']=='unadjusted', 'back-adjusted/continuous prices unsupported')
    require(manifest['calendar_policy']=='synthetic_regular_hours', 'explicit synthetic regular-hours policy required')
    dates=manifest['sessions']
    require(isinstance(dates,list) and 0 < len(dates) <= 60 and all(isinstance(d,str) for d in dates), '1..60 explicit session dates required')
    require(dates == sorted(set(dates)), 'session dates must be unique and chronological')
    tz,tzmeta=timezone();sessions={}
    for date in dates:
        require(bool(re.fullmatch(r'\d{4}-\d{2}-\d{2}',date)), 'date must be YYYY-MM-DD')
        day=dt.date.fromisoformat(date)
        require(2019 <= day.year <= 2035 and day.weekday()<5, 'unsupported date/weekend')
        if manifest['kind']=='historical_user_supplied':
            require(day < dt.datetime.now(tz).date(), 'historical session must be completed')
        start=dt.datetime.combine(day,dt.time(8,30),tzinfo=tz)
        sessions[date]={'open_ns':int(start.timestamp())*10**9,'quotes':[]}
    count=0;prev=-1
    with csv_path.open(newline='') as f:
        reader=csv.DictReader(f)
        require(reader.fieldnames==['timestamp_ns','contract','bid','ask'], 'exact CSV header required: timestamp_ns,contract,bid,ask')
        for row in reader:
            count+=1;require(count<=MAX_ROWS,'prototype limit 25000 total input quotes exceeded; not truncated or sampled')
            require(None not in row and all(v is not None for v in row.values()), 'malformed CSV row')
            text=row['timestamp_ns']; require(bool(re.fullmatch(r'[0-9]{1,19}',text)), 'timestamp must be unsigned decimal UTC ns')
            stamp=int(text);require(prev<=stamp<=I64,'out-of-order or overflowing timestamp');prev=stamp
            require(row['contract']==manifest['contract'],'mixed or mismatched dated contracts')
            bid,ask=ticks(row['bid']),ticks(row['ask'])
            require(bid<=ask and bid+ask<=I64,'crossed quote or midpoint overflow')
            date=dt.datetime.fromtimestamp(stamp//10**9,dt.timezone.utc).astimezone(tz).date().isoformat()
            require(date in sessions,'quote date not selected; filter explicitly before replay')
            ss=sessions[date]
            require(ss['open_ns']<=stamp<ss['open_ns']+23400*10**9,'quote outside synthetic 08:30–15:00 session')
            ss['quotes'].append((stamp,bid,ask))
    require(count>0,'empty data file')
    return manifest,sessions,tzmeta

def replay(manifest_path,csv_path,binary,out):
    require(not out.exists(),'output directory already exists; refusing stale result overwrite')
    manifest,sessions,tzmeta=load(manifest_path,csv_path)
    results=[]
    for date,ss in sessions.items():
        data=''.join(f'{i},{stamp},{bid},{ask}\n' for i,(stamp,bid,ask) in enumerate(ss['quotes'],1))
        p=subprocess.run([str(binary.resolve()),str(ss['open_ns'])],input=data,text=True,capture_output=True,timeout=120)
        require(p.returncode==0,'session '+date+' rejected: '+p.stderr.strip())
        result=json.loads(p.stdout);result['date']=date;result['contract']=manifest['contract']
        result['quality']='INCOMPLETE' if result['missing_bars'] or result['flat_by_failure'] or not result['quotes'] else 'BAR_COVERAGE_ONLY_NOT_FEED_COMPLETENESS'
        results.append(result)
    trades=[]; equity=0;peak=0;drawdown=0
    for result in results:
        entry=None
        for event in result['events']:
            if event['reason']=='ENTRY':
                # Direction is determined by actual entry fill in the replay log,
                # retained by the C++ runner, not inferred from its later outcome.
                entry=event
            elif event['reason'] in ['STOP','OPPOSITE_SIGNAL','FLAT_BY']:
                require(entry is not None,'exit without entry')
                side='BUY' if entry['position_after']>0 else 'SELL'
                gross=(event['price_ticks']-entry['price_ticks'])*(1 if side=='BUY' else -1)*50
                net=gross-200
                trades.append({'date':result['date'],'contract':manifest['contract'],'side':side,'entry_ns':entry['timestamp_ns'],'exit_ns':event['timestamp_ns'],'entry_ticks':entry['price_ticks'],'exit_ticks':event['price_ticks'],'exit_reason':event['reason'],'gross_cents':gross,'fees_cents':200,'net_cents':net})
                equity+=net;peak=max(peak,equity);drawdown=max(drawdown,peak-equity);entry=None
        if not result['unclosed_position']:
            require(sum(t['net_cents'] for t in trades if t['date']==result['date'])==result['net_cents'],'ledger reconciliation failure')
    summary={'status':'SYNTHETIC_PIPELINE_TEST' if manifest['kind']=='synthetic' else 'HISTORICAL_REPLAY_UNVERIFIED_SOURCE',
             'manifest':manifest,'input_sha256':digest(csv_path),'manifest_sha256':digest(manifest_path),
             'runner_sha256':digest(binary),'timezone':tzmeta,'sessions':len(results),'closed_trades':len(trades),
             'gross_cents':sum(r['gross_cents'] for r in results),'fees_cents':sum(r['fees_cents'] for r in results),
             'net_cents':sum(r['net_cents'] for r in results),'closed_trade_drawdown_cents':drawdown,
             'invalid_sessions':sum(r['quality']=='INCOMPLETE' for r in results),
             'performance_evaluation_valid':False,
             'limits':['synthetic fees and calendar; no exchange holiday/early-close validation','bar coverage does not prove complete event BBO feed','drawdown only at closed trades, not marked to market','no liquidity, latency or additional execution slippage model','prototype copies full state; hard limit 25000 input quotes; no downsampling','no profitability or account-compliance conclusion']}
    out.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=out.parent) as tmp:
        stage=pathlib.Path(tmp)/'result';stage.mkdir()
        (stage/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        (stage/'sessions.json').write_text(json.dumps(results,indent=2)+'\n')
        with (stage/'trades.csv').open('w',newline='') as f:
            names=['date','contract','side','entry_ns','exit_ns','entry_ticks','exit_ticks','exit_reason','gross_cents','fees_cents','net_cents']
            w=csv.DictWriter(f,fieldnames=names);w.writeheader();w.writerows(trades)
        stage.rename(out)
    return summary

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest',type=pathlib.Path,required=True);p.add_argument('--quotes',type=pathlib.Path,required=True)
    p.add_argument('--runner',type=pathlib.Path,default=ROOT/'build/fnq_replay_session');p.add_argument('--out',type=pathlib.Path,required=True)
    a=p.parse_args()
    try:
        s=replay(a.manifest,a.quotes,a.runner,a.out)
        print(json.dumps({k:s[k] for k in ['status','sessions','closed_trades','net_cents','invalid_sessions']}))
    except (ValueError,OSError,subprocess.SubprocessError,decimal.InvalidOperation,KeyError) as e:
        print('REPLAY REJECTED: '+str(e),file=sys.stderr);return 1
    return 0
if __name__=='__main__':sys.exit(main())
