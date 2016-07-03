import time, sys, traceback
try:
    import audiodev, audiospeex
except:
    print 'cannot load audiodev.so and audiospeex.so, please set the PYTHONPATH'
    traceback.print_exc()
    sys.exit(-1)
    
# capabilities

print audiodev.get_api_name()
print audiodev.get_devices()

upsample = downsample = enc = dec = None

queue = []
def inout(fragment, timestamp, userdata):
    global queue, enc, dec, upsample, downsample
    try:
        #print [sys.getrefcount(x) for x in (None, upsample, downsample, enc, dec)]
        fragment1, downsample = audiospeex.resample(fragment, input_rate=48000, output_rate=8000, state=downsample)
        fragment2, enc = audiospeex.lin2speex(fragment1, sample_rate=8000, state=enc)
        fragment3, dec = audiospeex.speex2lin(fragment2, sample_rate=8000, state=dec)
        fragment4, upsample = audiospeex.resample(fragment3, input_rate=8000, output_rate=48000, state=upsample)
        fragment5 = fragment4 + fragment4 # create stereo
        # print len(fragment), len(fragment1), len(fragment2), len(fragment3), len(fragment4), len(fragment5)
        queue.append(fragment5)
        if len(queue) < 50:
            return ""
        fragment = queue.pop(0)
        return fragment
    except KeyboardInterrupt:
        pass
    except:
        print traceback.print_exc()
        return ""

audiodev.open(output="default", input="default",
            format="l16", sample_rate=48000, frame_duration=20,
            output_channels=2, input_channels=1, flags=0x01, callback=inout)

try:
    while True:
        time.sleep(10)
except KeyboardInterrupt:
    audiodev.close()

del upsample, downsample, enc, dec

