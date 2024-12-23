import numpy as np

def f(x):
    return x**2*np.exp(x**2)

x = [i/5 for i in range(-5, 6)]

I = 0.5*f(0.7746)+0.5*f(-0.7746)
print(I)
