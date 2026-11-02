import unittest
from decimal import Decimal as D
import bar_study as m
O=0
def fixture(short=False):
 bars={}
 for i in range(26):
  values=[D('20000')]*90
  if i<8:values[:4]=map(D,['20000','20002.5','19997.5','20000'])
  elif i==8:values[:4]=map(D,['20002.5','20007.5','20002.5','20006.25']);values[4:]=[D('20006.25')]*86
  for j,p in enumerate(values):
   if short:p=D(40000)-p
   t=i*m.QUARTER+j*m.STEP
   bars[t]=m.Bar(t,p,p,p,p)
 # next bar's opening price is the signal close, then constant no new breakouts
 t=9*m.QUARTER;p=D('19993.75') if short else D('20006.25')
 bars[t]=m.Bar(t,p,p,p,p)
 return bars
class StudyTests(unittest.TestCase):
 def test_features_exact_distance(self):
  b=fixture();first=[]
  for i in range(9):
   w=[b[i*m.QUARTER+j*m.STEP] for j in range(90)]
   first.append(m.Bar((i+1)*m.QUARTER,w[0].o,max(x.h for x in w),min(x.l for x in w),w[-1].c))
  self.assertEqual(m.features(first),(D('20002.5'),D('19997.5'),43))
 def test_long_entry_flat_timer(self):
  r=m.session(0,fixture());t=r['trades'][0]
  self.assertEqual(t['entry_time'],9*m.QUARTER);self.assertEqual(t['entry_fill'],D('20006.75'))
  self.assertEqual(t['exit_reason'],'FLAT_BY');self.assertEqual(t['exit_time'],22800*m.SEC)
  self.assertEqual(t['exit_fill'],D('19999.50'));self.assertEqual(t['net_cents'],D(-1650))
  self.assertEqual(t['execution_drag_cents'],D(200));self.assertEqual(r['missing_15m'],0)
 def test_short_symmetry(self):
  a=m.session(0,fixture())['trades'][0];b=m.session(0,fixture(True))['trades'][0]
  self.assertEqual(b['side'],-1);self.assertEqual(a['net_cents'],b['net_cents'])
 def test_long_stop_within_entry_bar(self):
  b=fixture();t=9*m.QUARTER;x=b[t];b[t]=m.Bar(t,x.o,x.h,D('19990'),x.c)
  trade=m.session(0,b)['trades'][0]
  self.assertEqual(trade['exit_reason'],'STOP');self.assertEqual(trade['exit_time'],t+m.STEP-1)
  self.assertEqual(trade['net_cents'],D(-2450))
 def test_short_stop_within_entry_bar(self):
  b=fixture(True);t=9*m.QUARTER;x=b[t];b[t]=m.Bar(t,x.o,D('20010'),x.l,x.c)
  trade=m.session(0,b)['trades'][0];self.assertEqual(trade['net_cents'],D(-2450))
 def test_gap_long(self):
  b=fixture();t=9*m.QUARTER+m.STEP;b[t]=m.Bar(t,*[D('19990')]*4)
  trade=m.session(0,b)['trades'][0];self.assertEqual(trade['exit_fill'],D('19989.5'));self.assertEqual(trade['exit_time'],t)
  self.assertEqual(trade['net_cents'],D(-3650))
 def test_gap_short(self):
  b=fixture(True);t=9*m.QUARTER+m.STEP;b[t]=m.Bar(t,*[D('20010')]*4)
  trade=m.session(0,b)['trades'][0];self.assertEqual(trade['exit_fill'],D('20010.5'));self.assertEqual(trade['net_cents'],D(-3650))
 def test_no_tie_signal(self):
  b=fixture()
  for j in range(90):
   t=8*m.QUARTER+j*m.STEP;b[t]=m.Bar(t,*[D('20002.5')]*4)
  r=m.session(0,b);self.assertEqual(r['entries'],0)
 def test_missing_window(self):
  b=fixture();del b[0];r=m.session(0,b)
  self.assertEqual(r['entries'],0);self.assertEqual(r['missing_10s'],1)
  self.assertTrue(any(e['reason']=='MISSING_DATA' for e in r['events']))
 def test_ttl_exact_no_fill(self):
  b=fixture();start=9*m.QUARTER
  for j in range(6):del b[start+j*m.STEP]
  r=m.session(0,b);self.assertEqual(r['entries'],0);self.assertTrue(any(e['reason']=='ENTRY_EXPIRED' for e in r['events']))
 def test_ttl_before_deadline_fills(self):
  b=fixture();start=9*m.QUARTER
  for j in range(5):del b[start+j*m.STEP]
  r=m.session(0,b);self.assertEqual(r['entries'],1);self.assertEqual(r['trades'][0]['entry_time'],start+50*m.SEC)
 def test_end_does_not_fabricate_exit(self):
  b=fixture()
  for t in list(b):
   if t>=22800*m.SEC:del b[t]
  r=m.session(0,b);self.assertTrue(r['flat_by_failure']);self.assertEqual(r['trades'],[]);self.assertEqual(r['net_cents'],-100)
 def test_missing_flat_quote_waits(self):
  b=fixture();del b[22800*m.SEC];r=m.session(0,b)
  self.assertEqual(r['trades'][0]['exit_time'],22810*m.SEC)
 def test_fractional_adjusted_price_translation(self):
  b=fixture();shift=D('.01375');translated={t:m.Bar(t,x.o+shift,x.h+shift,x.l+shift,x.c+shift) for t,x in b.items()}
  a=m.session(0,b)['trades'][0];z=m.session(0,translated)['trades'][0]
  self.assertEqual(a['net_cents'],z['net_cents']);self.assertEqual(z['entry_fill']-a['entry_fill'],shift)
 def test_scenario_drag(self):
  for adverse in m.SCENARIOS:
   t=m.session(0,fixture(),adverse)['trades'][0]
   self.assertEqual(t['execution_drag_cents'],100*adverse)
   self.assertEqual(t['gross_cents'],t['matched_raw_gross_cents']-t['execution_drag_cents'])
 def test_closed_drawdown_arithmetic(self):
  fake={'one':{'trades':[{'net_cents':D(-100),'gross_cents':D(100),'fees_cents':D(200),'execution_drag_cents':D(100),'matched_raw_gross_cents':D(200)},
                         {'net_cents':D(50),'gross_cents':D(250),'fees_cents':D(200),'execution_drag_cents':D(100),'matched_raw_gross_cents':D(350)}],
               'net_cents':D(-50),'max_full_bar_mae_cents':D(250),'missing_10s':0,'missing_15m':0,'flat_by_failure':False,'events':[]}}
  s=m.summarize(fake,2);self.assertEqual(s['trade_close_drawdown_cents'],100);self.assertEqual(s['profit_factor'],D('.5'))
 def test_invalid_ohlc(self):
  b=fixture();b[0]=m.Bar(0,D(2),D(1),D(0),D(2));self.assertRaises(ValueError,m.session,0,b)
 def test_empty_day_coverage(self):
  r=m.session(0,{});self.assertEqual(r['missing_10s'],2340);self.assertEqual(r['missing_15m'],26);self.assertEqual(r['entries'],0)
 def test_no_unregistered_cost_tuning(self):self.assertRaises(ValueError,m.session,0,fixture(),3)
 def test_size_boundary(self):
  for width,expected in [(44,88),(D('44.5'),89)]:
   bars=[m.Bar(i,D(1000),D(1000)+width*TICK,D(1000),D(1000)) for i in range(9)]
   self.assertEqual(m.features(bars)[2],expected)
 def test_high_low_lookahead_excluded(self):
  bars=[m.Bar(i,D(100),D(101),D(99),D(100)) for i in range(8)]
  bars.append(m.Bar(8,D(100),D(110),D(100),D(109)))
  hi,lo,_=m.features(bars);self.assertEqual(hi,D(101));self.assertEqual(lo,D(99))
 def test_missing_day_invalidates_month_summary(self):
  r=m.summarize({'empty':m.session(0,{})},2);self.assertFalse(r['complete_month_economic_summary_valid'])
 def test_size_rejection_diagnostics(self):
  b=fixture()
  for i in range(8):
   t=i*m.QUARTER;b[t]=m.Bar(t,D('20000'),D('20010'),D('19990'),D('20000'))
  t=8*m.QUARTER;b[t]=m.Bar(t,D('20011'),D('20015'),D('20011'),D('20015'))
  for j in range(1,90):
   time=t+j*m.STEP;b[time]=m.Bar(time,*[D('20015')]*4)
  r=m.session(0,b);s=m.summarize({'day':r},2)
  self.assertEqual(r['entries'],0);self.assertGreater(s['size_rejected_signals'],0);self.assertGreater(s['minimum_required_risk_cents'],5000)
 def test_opposite_exit_priority_and_cooldown(self):
  b=fixture();start=9*m.QUARTER
  for j in range(1,90):
   t=start+j*m.STEP;b[t]=m.Bar(t,*[D('19997')]*4)
  t=10*m.QUARTER;b[t]=m.Bar(t,D('19997.25'),D('20000'),D('19990'),D('20000'))
  for i in [10,11]:
   for j in range(1,90):
    t=i*m.QUARTER+j*m.STEP;p=D('20008')+(i-10);b[t]=m.Bar(t,p,p,p,p)
  r=m.session(0,b);trade=r['trades'][0]
  self.assertEqual(trade['exit_reason'],'OPPOSITE_SIGNAL');self.assertEqual(trade['exit_time'],10*m.QUARTER)
  self.assertEqual(trade['net_cents'],-2200)
  self.assertTrue(any(e['time']==11*m.QUARTER and e['reason']=='COOLDOWN' for e in r['events']))
  self.assertTrue(any(e['time']==12*m.QUARTER and e['reason']=='COOLDOWN' for e in r['events']))
 def test_entry_cutoff_inclusive(self):
  for label,entries in [(22,1),(23,0)]:
   b={t:m.Bar(t,*[D('20000')]*4) for t in fixture()}
   for j in range(90):
    t=(label-1)*m.QUARTER+j*m.STEP;b[t]=m.Bar(t,*[D('20002')]*4)
   self.assertEqual(m.session(0,b)['entries'],entries)
TICK=m.TICK
if __name__=='__main__':unittest.main()
