
import time
Frequency = 6000 # Hz  
toggle = True

Loop_time=1/Frequency
tmp = bytearray([0 for i in range(32)])

try:
    while 1:
        Timestamp = time.perf_counter()
        if toggle:
            tmp[0] = 1
        else:
            tmp[0] = 0
        
        toggle ^= True
        dt = time.perf_counter()-Timestamp
        while(dt < Loop_time):
            dt = time.perf_counter()-Timestamp
        print("dt : {} , Frequency : {} Hz".format(dt,1/dt))

except KeyboardInterrupt:
            # ctrl-C abort handling
            print('stopped')