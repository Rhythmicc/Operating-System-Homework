#!/usr/bin/env python3
import requests
import threading
from time import perf_counter
import matplotlib.pyplot as plt
import sys
import numpy as np

status = {}
use_time = {}


class HttpLoad(threading.Thread):
    def __init__(self, cnt, url: str):
        threading.Thread.__init__(self)
        self.cnt = cnt
        self.target = url

    def run(self):
        for req in range(self.cnt):
            try:
                start = perf_counter()
                res = requests.get(self.target)
                end = perf_counter()
                mutex.acquire()
                if res.status_code not in status:
                    status[res.status_code] = 0
                    use_time[res.status_code] = []
                status[res.status_code] += 1
                use_time[res.status_code].append(round(end - start, 3))
                mutex.release()
            except Exception as e:
                print(e)
                mutex.acquire()
                if 'failed' not in status:
                    status['failed'] = 0
                status['failed'] += 1
                mutex.release()


if __name__ == '__main__':
    target = 'http://10.3.40.47:9168'
    #target = 'https://www.baidu.com'
    mutex = threading.Lock()
    tls = []
    num_thread = int(sys.argv[1])
    num_post = int(sys.argv[2])
    for i in range(num_thread):
        tls.append(HttpLoad(num_post, target))
        tls[-1].start()
    for i in tls:
        i.join()
    print("http status:", status)
    print("use time: {")
    for i in use_time:
        print('\t%s:' % i, use_time[i])
    print('}')
    for st in use_time:
        x = np.arange(1, status[st] + 1)
        plt.plot(x, use_time[st], label=str(st))
        plt.scatter(x, use_time[st])
    plt.legend(loc='best')
    plt.title('result')
    plt.savefig('result.png')
    plt.show()
