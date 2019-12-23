#!/usr/bin/env python3
import requests
import threading
from time import perf_counter
import matplotlib.pyplot as plt
import sys

status = {}
use_time = {}


class HttpLoad(threading.Thread):
    def __init__(self, cnt, url: str):
        threading.Thread.__init__(self)
        self.cnt = cnt
        self.target = url

    def run(self):
        global status, use_time
        import random
        for req in range(self.cnt):
            try:
                start = perf_counter()
                res = requests.get(self.target + '/%d.html' % random.randint(0, 100000))
                end = perf_counter()
                mutex.acquire()
                if res.status_code not in status:
                    status[res.status_code] = 0
                    use_time[res.status_code] = []
                status[res.status_code] += 1
                use_time[res.status_code].append((end - start) * 1000)
                mutex.release()
            except Exception:
                mutex.acquire()
                if 'failed' not in status:
                    status['failed'] = 0
                status['failed'] += 1
                mutex.release()


if __name__ == '__main__':
    target = 'http://202.204.194.17:9168'
    # target = 'https://www.baidu.com'
    mutex = threading.Lock()
    tls = []
    try:
        num_thread = int(sys.argv[1])
        num_post = int(sys.argv[2]) // num_thread
    except IndexError:
        num_thread = 5
        num_post = 10
    for i in range(num_thread):
        tls.append(HttpLoad(num_post, target))
        tls[-1].start()
    for i in tls:
        i.join()
    print("http status:\t", status)
    tol_time = 0
    for i in use_time:
        tol_time += sum(use_time[i])
        use_time[i] = round(sum(use_time[i]) / len(use_time[i]), 3)
    print("ms/time:\t", use_time)
    print('Total use time: %.4f ms' % tol_time)
    labels = status.keys()
    sizes = status.values()
    explore = [0.1 if i == 200 else 0.0 for i in labels]
    plt.pie(sizes, labels=labels, autopct='%1.2f%%', explode=explore, shadow=True)
    plt.title(' ,'.join(['%s: %.3f ms/time' % (i, use_time[i]) for i in use_time]))
    plt.savefig('result.png', dpi=300, bbox_inches='tight')
    plt.show()
