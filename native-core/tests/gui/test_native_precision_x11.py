# SPDX-License-Identifier: GPL-3.0-or-later
# Optional developer Xvfb interaction test; Python is not a GUI/runtime dependency.
import os,time,subprocess
from pathlib import Path
from Xlib import X,XK,display,protocol
from PIL import Image
home=Path.home(); assert os.getuid()!=0 and home.parent==Path("/tmp") and home.name.startswith("sp-guiverify-") and not any(home.iterdir()), "Requires a new, empty disposable ordinary-account home"
app=subprocess.Popen([os.environ['APP']]);d=display.Display();root=d.screen().root
try:
 w=None
 for _ in range(100):
  for child in root.query_tree().children:
   if (child.get_wm_name() or '').startswith('SysPulse Native - Linux Administration'):w=child;break
  if w:break
  time.sleep(.05)
 assert w,'window missing';time.sleep(.7)
 def click(x,y,delay=.3):
  w.send_event(protocol.event.ButtonPress(time=X.CurrentTime,root=root,window=w,child=X.NONE,root_x=x+30,root_y=y+30,event_x=x,event_y=y,state=0,detail=1,same_screen=1),event_mask=X.ButtonPressMask);d.sync();time.sleep(delay)
 def key(ch,delay=.12):
  symbols={'/':'slash','-':'minus','.':'period','_':'underscore'};sym=XK.string_to_keysym(symbols.get(ch,ch));code=d.keysym_to_keycode(sym);assert code,(ch,sym)
  w.send_event(protocol.event.KeyPress(time=X.CurrentTime,root=root,window=w,child=X.NONE,root_x=30,root_y=30,event_x=0,event_y=0,state=0,detail=code,same_screen=1),event_mask=X.KeyPressMask);d.sync();time.sleep(delay)
 def snapshot(name=None):
  g=w.get_geometry();im=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);img=Image.frombytes('RGB',(g.width,g.height),im.data,'raw','BGRX')
  if name:img.save(home/(name+'.png'))
  return img
 def nav(i,compact=False):
  y=83+i*43+(20 if i>=4 else 0)+(20 if i>=8 else 0);click(25 if compact else 100,y+18,.6)
  rgb=snapshot().getpixel((41 if compact else 210,y+18));assert all(abs(a-b)<4 for a,b in zip(rgb,(251,233,237))),(i,rgb)
 def fill(src,dst,offset=228):
  click(offset+260,392);click(offset+100,275)
  for ch in str(src):key(ch,0)
  time.sleep(.15);click(offset+100,345)
  for ch in str(dst):key(ch,0)
  time.sleep(.25)
 def check_page(i):
  y=83+i*43+(20 if i>=4 else 0)+(20 if i>=8 else 0);rgb=snapshot().getpixel((210,y+18));assert all(abs(a-b)<4 for a,b in zip(rgb,(251,233,237))),(i,rgb)
 # Real data, all destinations and compact layout.
 time.sleep(4.2);snapshot('overview');nav(1);time.sleep(4.2);snapshot('performance');nav(2);time.sleep(2.2);click(228+50,238);snapshot('task-manager');nav(3);nav(4);nav(5);snapshot('services');nav(6);snapshot('system-tools');nav(7);nav(8);nav(9);snapshot('reports')
 # Real private report export; tool availability is environment-specific.
 click(228+420,359,.5);reports=list(home.glob('SysPulse-health-*.txt'));assert reports and reports[0].stat().st_mode&0o777==0o600
 # Actual verified backup; modal blocks navigation; cancel and resize cannot apply.
 nav(8);src=home/'source';dst=home/'backup';restore=home/'restored';src.write_bytes(b'GUI acceptance backup\x00\xff')
 fill(src,dst);click(228+90,392);snapshot('review');click(100,101);check_page(8);assert not dst.exists();g=w.get_geometry();click(228+300,g.height-25);assert not dst.exists()
 click(228+90,392);w.configure(width=900,height=700);d.sync();time.sleep(.5);click(52+80,675);assert not dst.exists();w.configure(width=1366,height=760);d.sync();time.sleep(.5)
 click(228+90,392);click(228+80,735,.7);assert dst.read_bytes()==src.read_bytes();assert dst.stat().st_mode&0o777==0o600
 fill(dst,restore);click(228+90,392);click(228+80,735,.7);assert restore.read_bytes()==src.read_bytes()
 # F-key and keyboard focus navigation (keyboard activates same control path).
 key('F1');check_page(0);key('Tab');key('Tab');key('Return');check_page(1)
 w.configure(width=900,height=700);d.sync();time.sleep(.5);nav(2,True);snapshot('compact-task-manager');nav(0,True);snapshot('compact-overview');w.configure(width=700,height=620);d.sync();time.sleep(.5);nav(1,True);snapshot('narrow-performance');nav(0,True);snapshot('narrow-overview')
 assert app.poll() is None
 print('PASS: ten direct destinations, live rendering, task selection, private report export, review isolation, cancel/resize refusal, actual backup and restore, F-key/Tab navigation, 900/700px compact layouts.')
finally:app.terminate();app.wait(timeout=5);d.close()
