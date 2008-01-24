import alsaaudio
import sys
if len(sys.argv) > 1: name = sys.argv[1]
else: name = "Master"

m = alsaaudio.Mixer(name)

