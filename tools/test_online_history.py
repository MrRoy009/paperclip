import importlib.util,pathlib,tempfile,unittest,io,urllib.request
spec=importlib.util.spec_from_file_location('online_history',pathlib.Path(__file__).with_name('online_history.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
VALID=b'PAR1'+b'test'+(4).to_bytes(4,'little')+b'PAR1'
class Response(io.BytesIO):
 def __init__(self,data=VALID,headers=None,url=None,code=200):
  super().__init__(data);self.headers=headers or {};self.url=url or m.sample_url('2026-10');self.code=code
 def getcode(self):return self.code
 def geturl(self):return self.url
class Opener:
 def __init__(self,**args):self.args=args
 def open(self,request,timeout):
  assert timeout==30 and request.get_method()=='GET'
  assert 'Authorization' not in request.headers
  return Response(**self.args)
class Checks(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.out=pathlib.Path(self.tmp.name)/'sample'
 def tearDown(self):self.tmp.cleanup()
 def reject(self,**kwargs):
  with self.assertRaises(ValueError):m.fetch('2026-10',self.out,Opener(**kwargs))
  self.assertFalse(self.out.exists())
 def test_read_only_download_remains_incompatible(self):
  r=m.fetch('2026-10',self.out,Opener(headers={'Content-Length':str(len(VALID))}))
  self.assertFalse(r['strategy_compatible']);self.assertFalse(r['strategy_run']);self.assertFalse(r['live_trading_enabled'])
 def test_bad_magic(self):self.reject(data=b'<html>login</html>')
 def test_bad_footer(self):self.reject(data=b'PAR1'+b'test'+(999).to_bytes(4,'little')+b'PAR1')
 def test_truncated(self):self.reject(headers={'Content-Length':'100'})
 def test_redirect(self):self.reject(url='https://other.example/file')
 def test_paid_status(self):self.reject(code=402)
 def test_too_large_header(self):self.reject(headers={'Content-Length':str(m.LIMIT+1)})
 def test_overwrite(self):self.out.mkdir();self.assertRaises(ValueError,m.fetch,'2026-10',self.out,Opener())
 def test_month_path_injection(self):self.assertRaises(ValueError,m.sample_url,'2026-10/../../api')
 def test_redirect_handler(self):
  self.assertRaises(ValueError,m.NoRedirect().redirect_request,None,None,302,'',{},'https://other.example')
if __name__=='__main__':unittest.main()
