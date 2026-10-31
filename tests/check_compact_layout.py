"""Geometry model for current single-page Layout. No native MQL/MT5 rendering."""
from pathlib import Path
import math
import re

ROOT=Path(__file__).resolve().parent.parent
source=(ROOT/'MQL5/Experts/ClearSizer/ClearSizer.mq5').read_text(encoding="utf-8")
clean=re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/','',source)
stack=[];pairs={')':'(',']':'[','}':'{'}
for char in clean:
    if char in '([{':stack.append(char)
    elif char in ')]}':assert stack and stack.pop()==pairs[char],char
assert not stack
ui=source[source.index('bool ObjectBase('):source.index('void Line(')]
assert 'UI_WIDTH=300,UI_BODY_TOP=50,UI_ROW=28' in source
assert 'body_rows=8+' in ui and '+(has_own_exposure ? 1 : 0)' in ui
assert 'const int UI_FOOTER=80,UI_PAGE_NAV=20' in ui
assert 'UI_BODY_TOP+viewport_height+footer_height' in ui
assert 'double ay=fy+(body_pages>1 ? 24 : 4)' in ui
assert 'Label("hint"' not in ui
assert 'ObjectDelete(0,N("hint"))' in ui
assert 'Label("status",10,ay+34' in ui
assert 'Label("status_detail",10,ay+50' in ui
assert 'OBJPROP_TOOLTIP,full_context' in ui
for removed in ('manage_tab','management_settings','trail_enabled','ManagementButtons','ManagementParameters'):
    assert removed not in ui,removed
assert 'ToggleButtons(row); row++;' in ui
assert 'rendered_rows!=body_rows' in ui and 'rendered_capacity!=body_capacity' in ui
count=0
for cw in (220,300,320,506,800,1280):
 for ch in (200,300,440,614,900):
  for dpi in (96,144,192):
   for zoom in (.6,1,1.5,2.5):
    for targets in (1,2,3):
     for exposure in (False,True):
      rows=8+targets-1+int(exposure)
      ratio=dpi/96;fz=min(zoom,(cw-8)/(300*ratio));small=fz<.6
      geometry=ratio*max(.6,fz);px=lambda value:math.floor(value*geometry+.5)
      x=max(0,min(12,cw-px(300)-4));y=max(0,min(28,ch-px(178)-4))
      available=math.floor((ch-y-4)/geometry)
      footer_height=80
      capacity=min(rows*28,max(28,math.floor((available-50-footer_height)/28)*28))
      if capacity<rows*28:
       footer_height=100
       capacity=min(rows*28,max(28,math.floor((available-50-footer_height)/28)*28))
      small=small or available<50+28+footer_height
      if not small:
       assert x+px(300)<=cw and y+px(50+capacity+footer_height)<=ch,(cw,ch,dpi,zoom,rows)
       pages=math.ceil(rows*28/capacity)
       footer=50+capacity
       actions_y=footer+(24 if pages>1 else 4)
       assert actions_y+28<actions_y+34
       assert actions_y+50+12<=footer+footer_height
       font8=max(6,math.floor(8*max(.6,fz)+.5))
       conservative_text_height=math.ceil(font8*96*ratio/72)
       assert px(actions_y+50)+conservative_text_height<=px(footer+footer_height)
       for row in range(rows):
        page=row*28//capacity;row_y=50+row*28-page*capacity
        assert page<pages and 50<=row_y and row_y+24<=50+capacity
      count+=1
print(f'PASS: balanced source delimiters; {count} single-page layout model combinations; no native text rendering')
print('Full logical sizes: default 300x354; TP2/TP3 add 28 each; own exposure adds 28; footer80 plus20 for paging; folded 300x38')
