"""Read-only free MNQ historical sample download; never places or simulates orders."""
import argparse,datetime as dt,hashlib,json,pathlib,re,tempfile,urllib.request

HOST='historicalfuturesdata.com'
LIMIT=16*1024*1024

def sample_url(month):
    if not re.fullmatch(r'20[0-9]{2}-(?:0[1-9]|1[0-2])',month):
        raise ValueError('month must be YYYY-MM')
    return f'https://{HOST}/samples/MNQ/10s/cme.mnq.bars.10s.{month}.parquet'

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,req,fp,code,msg,headers,newurl):
        raise ValueError('redirect rejected; inspect changed provider URL before retrying')

def validate(data):
    if len(data)<12 or data[:4]!=b'PAR1' or data[-4:]!=b'PAR1':
        raise ValueError('download is not a Parquet container')
    footer=int.from_bytes(data[-8:-4],'little')
    if not 0<footer<=len(data)-12:
        raise ValueError('invalid Parquet footer length')

def fetch(month,out,opener=None):
    if out.exists():raise ValueError('output directory exists; refusing overwrite')
    url=sample_url(month)
    opener=opener or urllib.request.build_opener(NoRedirect())
    request=urllib.request.Request(url,headers={'User-Agent':'MNQ-Research-Sample-Check/1.0','Accept':'application/octet-stream'})
    with opener.open(request,timeout=30) as response:
        if response.getcode()!=200:raise ValueError('provider did not return HTTP 200')
        if response.geturl()!=url:raise ValueError('unexpected final URL')
        length=response.headers.get('Content-Length')
        if length and int(length)>LIMIT:raise ValueError('sample exceeds 16 MiB limit')
        data=response.read(LIMIT+1)
        if len(data)>LIMIT:raise ValueError('sample exceeds 16 MiB limit')
        if length and int(length)!=len(data):raise ValueError('incomplete download')
    validate(data)
    report={'status':'ONLINE_DOWNLOAD_OK_STRATEGY_REPLAY_BLOCKED','provider':HOST,'url':url,
            'checked_at_utc':dt.datetime.now(dt.timezone.utc).isoformat(),'bytes':len(data),
            'sha256':hashlib.sha256(data).hexdigest(),'data_class':'continuous_adjusted_10_second_OHLCV_bars',
            'validation':'HTTP transfer, byte count, hash and Parquet container/footer only; columns not decoded',
            'strategy_compatible':False,'strategy_run':False,'billing':'public free sample; no key or paid endpoint',
            'reason':'provider documents adjusted continuous OHLCV bars, not dated-contract event-level bid/ask; cannot replay existing strategy faithfully',
            'provider_docs':'https://historicalfuturesdata.com/docs',
            'required_data':'unadjusted dated MNQ event BBO with verified time/order semantics',
            'live_trading_enabled':False}
    out.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=out.parent) as tmp:
        stage=pathlib.Path(tmp)/'download';stage.mkdir()
        (stage/pathlib.PurePosixPath(url).name).write_bytes(data)
        (stage/'online-check.json').write_text(json.dumps(report,indent=2)+'\n')
        stage.rename(out)
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--month',required=True);p.add_argument('--out',type=pathlib.Path,required=True)
    a=p.parse_args()
    try:
        print(json.dumps(fetch(a.month,a.out),indent=2))
    except (OSError,ValueError) as e:
        p.exit(1,'Online history check failed: '+str(e)+'\n')
if __name__=='__main__':main()
