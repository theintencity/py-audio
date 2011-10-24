#!/usr/bin/env python

import time, sys, traceback
try:
    import audiodev, audiospeex, audiotts
except:
    print 'cannot load audiodev.so, audiospeex.so or audiotts.so, please set the PYTHONPATH'
    traceback.print_exc()
    sys.exit(-1)

if len(sys.argv) <= 1:
    print 'supply the text to be spoken on command line, e.g., \npython %s Hello, how are you doing?'%(sys.argv[0],)
    sys.exit(-1)

text = ' '.join(sys.argv[1:])
queue, upsample = [], None
def inout(fragment, timestamp, userdata):
    global queue, upsample
    if queue:
        data = queue.pop(0)
        data, upsample = audiospeex.resample(data, input_rate=8000, output_rate=44100, state=upsample)
        return data
    return ""

data = audiotts.convert(text)
duration = len(data) / 320 * 0.020
while data:
    queue.append(data[:320])
    data = data[320:]
    
audiodev.open(output="default", 
            format="l16", sample_rate=44100, frame_duration=20,
            output_channels=1, input_channels=1, callback=inout)

try:
    time.sleep(duration)
except KeyboardInterrupt:
    audiodev.close()

del upsample

