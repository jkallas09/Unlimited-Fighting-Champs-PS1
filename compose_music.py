"""Four original, independently composed PS1 entrance loops.
All instruments are synthesized mathematically; no samples or melody references.
Python 3 regenerates the bank, metadata and four decoded preview WAVs.
"""
import math
import random
import struct
import wave
from pathlib import Path
RATE=22050
SECONDS=8
TRACKS=[
    ("Emerald Ruckus",[38,45,50,48,38,45,53,48,41,48,53,50,36,43,48,45],
     [74,77,81,77,74,72,69,72,74,77,79,77,72,69,67,69,77,81,84,81,79,77,74,72,69,72,74,77,72,69,67,69]),
    ("Boneless After Dark",[30,30,30,33,30,30,28,28,26,26,29,29,28,28,25,28],
     [66,None,None,69,None,64,None,None,62,None,66,None,None,61,None,None,66,None,73,None,69,None,None,64,62,None,None,61,None,64,None,None]),
    ("Lunch Break Rampage",[28,28,31,28,26,26,28,28,24,24,26,24,23,23,26,28],
     [52,52,None,55,52,None,50,None,48,48,None,50,47,None,50,None,52,None,55,57,52,None,50,None,48,None,50,52,47,None,50,52]),
    ("Iced Latte Overdrive",[40,47,43,47,40,47,50,43,38,45,41,45,36,43,40,43],
     [76,None,79,83,None,81,79,None,74,77,None,81,79,None,77,74,76,79,None,83,86,None,83,79,72,None,76,79,None,76,74,71])]
def hz(note):return 440*2**((note-69)/12)
def compose(style,bassline,lead):
    rng=random.Random(35191+style*7919);pcm=[]
    for n in range(RATE*SECONDS):
        t=n/RATE;beat=int(t*2);eighth=int(t*4);sixteenth=int(t*8)
        bt=t-beat*.5;et=t-eighth*.25;st=t-sixteenth*.125
        noise=rng.uniform(-1,1);f=hz(bassline[beat%16])
        env=min(1,bt/.008)*math.exp(-bt*(4 if style==1 else 7))
        bass=(math.sin(2*math.pi*f*t)+.22*math.sin(4*math.pi*f*t))*env*.22
        kick=0
        kickbeat=(beat%4 in (0,2)) if style!=1 else (beat%4==0 or eighth%16==11)
        if kickbeat:kick=math.sin(2*math.pi*(45*bt+10*(1-math.exp(-bt*32))))*math.exp(-bt*18)*.34
        snare=noise*math.exp(-bt*(19 if style==2 else 28))*.19 if beat%4 in (1,3) else 0
        hat=noise*math.exp(-st*85)*(.032 if sixteenth%2 else .045) if style==3 else noise*math.exp(-et*75)*.04
        melody=0;note=lead[eighth%32]
        if note is not None:
            mf=hz(note);phase=2*math.pi*mf*t
            if style==0: # Bright plucked strings over a stomping drum pattern.
                tone=math.sin(phase)+.42*math.sin(phase*2)+.2*math.sin(phase*3)
                menv=min(1,et/.003)*math.exp(-et*15)
            elif style==1: # Sparse bell-like motif, sub bass and half-time space.
                tone=math.sin(phase+.9*math.sin(phase*2))*0.9
                menv=min(1,et/.01)*math.exp(-et*6)
            elif style==2: # Synthesized power-chord distortion, no guitar samples.
                tone=math.tanh(2.8*(math.sin(phase)+.6*math.sin(phase*1.4983)))
                menv=min(1,et/.006)*math.exp(-et*9)
            else: # Syncopated bright electro-funk stabs and sixteenth hats.
                tone=math.sin(phase)+.3*math.sin(phase*3)+.15*math.sin(phase*4)
                menv=min(1,et/.005)*math.exp(-et*19)
            melody=tone*menv*(.13 if style==2 else .11)
        extra=0
        if style==0 and eighth%4==3:extra=math.sin(2*math.pi*170*et)*math.exp(-et*30)*.1
        if style==2:extra=noise*math.exp(-((t%2))*7)*.028
        v=math.tanh((bass+kick+snare+hat+melody+extra)*1.25)*.82
        pcm.append(round(v*32767))
    return pcm
def encode(pcm):
    COEFFS = [(0,0),(60,0),(115,-52),(98,-55),(122,-60)]
    encoded=bytearray()
    h1=h2=0
    decoded=[]
    for pos in range(0,len(pcm),28):
        block=pcm[pos:pos+28]
        block += [0]*(28-len(block))
        best=None
        for filt,(c1,c2) in enumerate(COEFFS):
            p1,p2=h1,h2;peak=1
            for value in block:
                residual=value-((p1*c1+p2*c2+32)//64)
                peak=max(peak,abs(residual));p2,p1=p1,value
            guess=0
            while guess<12 and peak <= 7*(1 << (11-guess)):
                guess+=1
            for shift in range(max(0,guess-1),min(12,guess+1)+1):
                step=1 << (12-shift)
                p1,p2=h1,h2;out=[];quant=[];error=0
                for value in block:
                    pred=(p1*c1+p2*c2+32)//64
                    q=max(-8,min(7,round((value-pred)/step)))
                    sample=max(-32768,min(32767,pred+q*step))
                    error+=(value-sample)**2
                    quant.append(q&15);out.append(sample);p2,p1=p1,sample
                if best is None or error<best[0]:
                    best=(error,filt,shift,quant,out,p1,p2)
        _,filt,shift,quant,out,h1,h2=best
        flag=4 if pos==0 else 0
        if pos+28>=len(pcm):flag=3
        encoded.extend([(filt<<4)|shift,flag])
        encoded.extend(quant[i]|(quant[i+1]<<4) for i in range(0,28,2))
        decoded.extend(out)
    encoded.extend(bytes((-len(encoded))%64))
    return bytes(encoded),decoded

root=Path(__file__).resolve().parent
bank=bytearray();offsets=[];sizes=[]
for style,(name,bassline,lead) in enumerate(TRACKS):
    pcm=compose(style,bassline,lead);encoded,decoded=encode(pcm)
    offsets.append(len(bank));sizes.append(len(encoded));bank.extend(encoded)
    path=root/('walkout_'+str(style)+'_preview.wav')
    with wave.open(str(path),'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE)
        w.writeframes(struct.pack('<'+'h'*len(decoded),*decoded))
    noise=sum((a-b)**2 for a,b in zip(pcm,decoded));signal=sum(x*x for x in pcm)
    snr=10*math.log10(signal/max(noise,1))
    print(f'{style}: {name}; {len(encoded)} bytes; decoded SNR {snr:.1f} dB')
    assert snr>22 and len(encoded)%64==0
assert len(bank)+0x1010<512*1024
(root/'walkout_music.adpcm').write_bytes(bank)
meta=f'#define WALKOUT_MUSIC_BYTES {len(bank)}\n#define WALKOUT_MUSIC_RATE {RATE}\n'
meta+='static const unsigned int music_offsets[4]={'+','.join(map(str,offsets))+'};\n'
meta+='static const char *const music_names[4]={'+','.join('"'+t[0].upper()+'"' for t in TRACKS)+'};\n'
(root/'music_meta.h').write_text(meta)
print(f'Bank: {len(bank)} bytes, ends at SPU address {len(bank)+0x1010:#x}')
