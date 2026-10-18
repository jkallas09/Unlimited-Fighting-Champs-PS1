"""Compose Main Event Pulse: an original 8-second arena loop for PS1 SPU.
No recordings, third-party samples or borrowed melodies are used.
Run with Python 3 to regenerate walkout_music.adpcm, music_meta.h and preview WAV.
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
SECONDS = 8
rng = random.Random(35191)
notes = [45,45,48,43,45,45,41,43,45,45,48,43,41,43,40,43]
lead = [69,None,72,76,74,None,72,67,69,None,65,67,64,None,67,69,
        69,72,76,None,79,76,74,72,69,None,67,65,64,67,69,None]

def hz(midi):
    return 440 * 2 ** ((midi - 69) / 12)

pcm = []
for n in range(RATE * SECONDS):
    t = n / RATE
    beat = int(t * 2)
    eighth = int(t * 4)
    bt = t - beat * .5
    et = t - eighth * .25
    # A plucked low synth, original minor-key lead, kick, snare and closed hats.
    bass_f = hz(notes[beat % 16])
    bass_env = min(1, bt / .008) * math.exp(-bt * 5)
    bass = (math.sin(2*math.pi*bass_f*t) + .22*math.sin(4*math.pi*bass_f*t)) * bass_env * .19
    kick = 0
    if beat % 4 in (0,2) or (eighth % 8 == 7):
        kt = bt if beat % 4 in (0,2) else et
        phase = 2*math.pi*(49*kt + 11*(1-math.exp(-kt*32)))
        kick = math.sin(phase)*math.exp(-kt*19)*.32
    noise = rng.uniform(-1,1)
    snare = noise*math.exp(-bt*25)*.18 if beat % 4 in (1,3) else 0
    hat = noise*math.exp(-et*75)*(.035 if eighth % 2 else .055)
    melody = 0
    note = lead[eighth % 32]
    if note is not None:
        freq=hz(note)
        env=min(1,et/.012)*math.exp(-et*13)
        melody=(math.sin(2*math.pi*freq*t)+.18*math.sin(6*math.pi*freq*t))*env*.095
    v=math.tanh((bass+kick+snare+hat+melody)*1.35)*.83
    pcm.append(round(v*32767))

# PS1 ADPCM: choose a predictor and shift by decoded mean-square error.
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
root=Path(__file__).resolve().parent
(root/'walkout_music.adpcm').write_bytes(encoded)
(root/'music_meta.h').write_text(f'#define WALKOUT_MUSIC_BYTES {len(encoded)}\n#define WALKOUT_MUSIC_RATE {RATE}\n')
with wave.open(str(root/'walkout_music_preview.wav'),'wb') as w:
    w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE)
    w.writeframes(struct.pack('<'+'h'*len(decoded),*decoded))
noise=sum((a-b)**2 for a,b in zip(pcm,decoded))
signal=sum(x*x for x in pcm)
snr=10*math.log10(signal/max(noise,1))
print(f'Original track: {SECONDS}s, {RATE}Hz; {len(encoded)} ADPCM bytes; decoded SNR {snr:.1f}dB; peak {max(abs(x) for x in decoded)}')
assert len(encoded)%64==0 and len(encoded)<450000
assert snr>22
