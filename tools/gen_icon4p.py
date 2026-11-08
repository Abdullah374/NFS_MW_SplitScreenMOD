# Draws the 4 Player Split Screen menu icon and writes src/icon4p.inc
from PIL import Image, ImageDraw, ImageFont, ImageChops
S=8; W=64*S
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
def figure(m, cx, top, num, solid=True, sc=1.0):
    d=ImageDraw.Draw(m)
    u=S*sc
    hr=5*u
    d.ellipse((cx-hr, top, cx+hr, top+2*hr), fill=255)
    tw=8.5*u; ty=top+2*hr+0.8*u; tb=ty+14.5*u
    d.rounded_rectangle((cx-tw, ty, cx+tw, tb), radius=4*u, fill=255)
    f=ImageFont.truetype(FONT, int(11*u))
    bb=d.textbbox((0,0),str(num),font=f)
    tx=cx-(bb[0]+bb[2])/2; tyy=(ty+tb)/2-(bb[1]+bb[3])/2
    if solid:
        d.text((tx,tyy),str(num),font=f,fill=0)
    else:
        d.rounded_rectangle((cx-tw+2.2*u, ty+2.2*u, cx+tw-2.2*u, tb-2.2*u), radius=2*u, fill=0)
        d.text((tx,tyy),str(num),font=f,fill=255)
def dots(m, horiz, pos):
    d=ImageDraw.Draw(m)
    for i in range(12):
        a=(3+i*5)*S
        if horiz: d.rectangle((a, pos, a+3*S-1, pos+3*S-1), fill=255)
        else: d.rectangle((pos, a, pos+3*S-1, a+3*S-1), fill=255)
def finish(m,name):
    a=m.resize((64,64),Image.LANCZOS)
    im=Image.new('RGBA',(64,64),(255,255,255,0)); im.putalpha(a); im.save(name+'.png')
    bg=Image.new('RGBA',(64,64),(40,40,60,255)); bg.alpha_composite(im)
    bg.resize((256,256),Image.NEAREST).save(name+'_view.png')
    return im

def dots2(m):
    d=ImageDraw.Draw(m)
    for k in range(-6,7):
        a=30.5+5*k
        if a<1 or a+3>63: continue
        d.rectangle((a*S, 30.5*S, (a+3)*S-1, 33.5*S-1), fill=255)
        d.rectangle((30.5*S, a*S, 33.5*S-1, (a+3)*S-1), fill=255)
m=Image.new('L',(W,W),0); dots2(m)
for i,(cx,ty) in enumerate([(14.5,2),(49.5,2),(14.5,36),(49.5,36)]):
    figure(m,cx*S,ty*S,i+1,solid=True,sc=0.98)
im=finish(m,'icon4p')
a=im.getchannel('A').tobytes()
with open(__import__('os').path.join(__import__('os').path.dirname(__file__),'..','src','icon4p.inc'),'w') as f:
    f.write('// 64x64 alpha mask for the 4 Player Split Screen menu icon (white glyph, drawn by gen_icon4p.py)\n')
    f.write('static const unsigned char kIcon4PAlpha[64*64] = {\n')
    for r in range(64): f.write(''.join('%d,'%b for b in a[r*64:r*64+64])+'\n')
    f.write('};\n')
