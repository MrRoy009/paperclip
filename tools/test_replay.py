"""End-to-end quote replay and failure-path checks, generated synthetic inputs only."""
import csv,datetime as dt,importlib.util,json,pathlib,subprocess,sys,tempfile,unittest
spec=importlib.util.spec_from_file_location('replay',pathlib.Path(__file__).with_name('replay.py'))
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
BINARY=pathlib.Path(sys.argv.pop(1)).resolve()

class ReplayTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.path=pathlib.Path(self.tmp.name)
  self.manifest={'kind':'synthetic','contract':'MNQZ24','source':'generated unit fixture, not market history','timestamp_basis':'UTC_UNIX_NS','quote_coverage':'event_bbo','adjustment':'unadjusted','calendar_policy':'synthetic_regular_hours','sessions':['2024-11-05']}
  self.tz,_=r.timezone();self.open=int(dt.datetime(2024,11,5,8,30,tzinfo=self.tz).timestamp())*10**9
  self.rows=[]
  def price(t):return str(r.decimal.Decimal(t)/4)
  def mid(t,m):self.rows.append([str(t),'MNQZ24',price((m-2)//2),price((m+2)//2)])
  for i in range(1,10):
   values=[160000,160020,159980,160000] if i<9 else [160020,160060,160020,160050]
   for j,m in enumerate(values):mid(self.open+(i-1)*900*10**9+(j+1)*10**9,m)
  self.label=self.open+9*900*10**9
  self.rows.extend([[str(self.label),'MNQZ24','20006.25','20006.50'],[str(self.label+10**9),'MNQZ24','19995.75','19996.00']])
 def tearDown(self):self.tmp.cleanup()
 def save(self):
  m=self.path/'manifest.json';q=self.path/'quotes.csv'
  m.write_text(json.dumps(self.manifest))
  with q.open('w',newline='') as f:
   w=csv.writer(f);w.writerow(['timestamp_ns','contract','bid','ask']);w.writerows(self.rows)
  return m,q
 def run_replay(self):
  m,q=self.save();out=self.path/'result'
  s=r.replay(m,q,BINARY,out)
  return s,out
 def reject(self):
  with self.assertRaises((ValueError,OverflowError)):self.run_replay()
  self.assertFalse((self.path/'result').exists())
 def test_long_ledger_and_provenance(self):
  s,out=self.run_replay();self.assertEqual(s['net_cents'],-2350);self.assertEqual(s['closed_trades'],1)
  self.assertEqual(s['status'],'SYNTHETIC_PIPELINE_TEST');self.assertFalse(s['performance_evaluation_valid'])
  self.assertEqual(s['timezone']['zone'],'America/Chicago');self.assertEqual(s['invalid_sessions'],1)
  with (out/'trades.csv').open() as f:t=list(csv.DictReader(f))
  self.assertEqual(t[0]['side'],'BUY');self.assertEqual(t[0]['exit_reason'],'STOP')
 def test_short_direction(self):
  for row in self.rows[:36]:
   b,a=r.ticks(row[2]),r.ticks(row[3]);row[2]=str(r.decimal.Decimal(160000-a)/4);row[3]=str(r.decimal.Decimal(160000-b)/4)
  self.rows[-2][2:]=['19993.50','19993.75'] # short fill 79974
  self.rows[-1][2:]=['20004.00','20004.25'] # stop buy at 80017
  s,out=self.run_replay();self.assertEqual(s['net_cents'],-2350)
  with (out/'trades.csv').open() as f:self.assertEqual(list(csv.DictReader(f))[0]['side'],'SELL')
 def test_future_historical_rejected(self):self.manifest['kind']='historical_user_supplied';self.manifest['sessions']=['2030-11-05'];self.reject()
 def test_mixed_contract(self):self.rows[-1][1]='MNQH25';self.reject()
 def test_continuous_contract(self):self.manifest['contract']='MNQ.c.0';self.reject()
 def test_offgrid(self):self.rows[0][2]='20000.10';self.reject()
 def test_crossed(self):self.rows[0][2:]=['20001.00','20000.00'];self.reject()
 def test_unsorted(self):self.rows[0],self.rows[1]=self.rows[1],self.rows[0];self.reject()
 def test_equal_timestamps_keep_order(self):self.rows[-1][0]=self.rows[-2][0];s,_=self.run_replay();self.assertEqual(s['net_cents'],-2350)
 def test_candle_schema_rejected(self):self.manifest['quote_coverage']='ohlcv';self.reject()
 def test_empty(self):self.rows=[];self.reject()
 def test_overflow(self):self.rows[0][2:]=['9999999999999999999999999999']*2;self.reject()
 def test_weekend(self):self.manifest['sessions']=['2024-11-09'];self.reject()
 def test_unselected_date(self):self.manifest['sessions']=['2024-11-06'];self.reject()
 def test_off_session(self):self.rows[0][0]=str(self.open-1);self.reject()
 def test_row_limit_not_truncation(self):self.rows=self.rows[:1]*25001;self.reject()
 def test_flat_failure_not_synthetic_close(self):
  self.rows=self.rows[:-1];s,out=self.run_replay();self.assertEqual(s['closed_trades'],0)
  sessions=json.loads((out/'sessions.json').read_text());self.assertTrue(sessions[0]['flat_by_failure']);self.assertEqual(sessions[0]['unclosed_position'],1)
 def test_output_reuse_rejected(self):self.run_replay();self.assertRaises(ValueError,self.run_replay)
 def test_explicit_calendar_and_dst(self):
  for date,expected in [('2024-10-29',13),('2024-11-05',14)]:
   self.manifest['sessions']=[date];m,q=self.save();self.rows=[];m,q=self.save()
   # A single quote verifies UTC conversion without inventing a historical series.
   d=dt.date.fromisoformat(date);o=int(dt.datetime.combine(d,dt.time(8,30),tzinfo=self.tz).timestamp())*10**9
   self.rows=[[str(o),'MNQZ24','20000.00','20000.25']];m,q=self.save()
   _,ss,_=r.load(m,q);self.assertEqual(dt.datetime.fromtimestamp(ss[date]['open_ns']//10**9,dt.timezone.utc).hour,expected)
 def test_runner_rejects_trailing_column(self):
  p=subprocess.run([str(BINARY),str(self.open)],input=f'1,{self.open},80000,80001,\n',text=True,capture_output=True)
  self.assertNotEqual(p.returncode,0);self.assertEqual(p.stdout,'')
 def test_bad_later_row_does_not_publish(self):self.rows[-1][2]='bad';self.reject()
 def test_selected_missing_session_is_reported(self):
  self.manifest['sessions'].append('2024-11-06');s,out=self.run_replay()
  self.assertEqual(s['sessions'],2);self.assertEqual(s['invalid_sessions'],2)
  ss=json.loads((out/'sessions.json').read_text());self.assertEqual(ss[1]['quotes'],0);self.assertEqual(ss[1]['missing_bars'],26)
 def test_deterministic_repeated_replay(self):
  _,out=self.run_replay();m,q=self.save();other=self.path/'repeat'
  r.replay(m,q,BINARY,other)
  for name in ['summary.json','sessions.json','trades.csv']:self.assertEqual((out/name).read_bytes(),(other/name).read_bytes())

if __name__=='__main__':unittest.main()
