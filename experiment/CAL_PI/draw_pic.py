import matplotlib.pyplot as plt
import numpy as np

if __name__ == "__main__":
    with open('result2.txt', 'r') as f:
        y = [float(i) for i in f.read().split()]
    x = np.arange(1, len(y) + 1)
    aver = sum(y) / len(y)
    mxval = max(y)
    mnval = min(y)
    plt.plot(x, y)
    plt.scatter(x, y)
    plt.xlabel = 'times'
    plt.ylabel = 'PI'
    plt.grid(linestyle="-.")
    plt.title('AVER: %f MAX: %f MIN: %f' % (aver, mxval, mnval))
    plt.savefig('result2.png')
    plt.show()
