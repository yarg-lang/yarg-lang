import time

def fib(n):
    if n < 2.0:
        return n
    return fib(n - 2.0) + fib(n - 1.0)

start = time.time()
print(fib(35.0))
print(time.time() - start)
