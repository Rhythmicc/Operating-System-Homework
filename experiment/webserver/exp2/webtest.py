#!/usr/bin/env python3
import requests
import threading
from time import perf_counter

status = {}
use_time = []


class http_load(threading.Thread):
    def __init__(self, cnt):
        threading.Thread.__init__(self)
        self.cnt = cnt

    def run(self):
        for i in range(self.cnt):
            start = perf_counter()
            res = requests.get('http://10.3.40.47:9168')
            end = perf_counter()
            mutex.acquire()
            if res.status_code not in status:
                status[res.status_code] = 0
            status[res.status_code] += 1
            use_time.append(end - start)
            mutex.release()


if __name__ == '__main__':
    mutex = threading.Lock()
    tls = []
    for i in range(10):
        tls.append(http_load(5))
        tls[-1].start()
    for i in tls:
        i.join()
    print("http status:", status)
    import numpy as np
    use_time = np.array(use_time)
    print("use time:[", use_time)
