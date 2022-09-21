import numpy as np
import matplotlib.pyplot as plt
import math


tmp1 = np.loadtxt("./picking_list1.csv", dtype=np.float, delimiter=',')
x = tmp1[:,2]
y = tmp1[:,3]

k = tmp1[:, 5]

plt.figure()
'''
xx = np.arange(x[156],x[0],-1)
b = y[0] - math.tan(k[156])*x[0]
yy = math.tan(k[156])*xx + b

plt.plot(xx,yy,c='blue')

'''
print(x)
print(y)

tmp2 = np.loadtxt("./picking_list.csv", dtype=np.float, delimiter=',')
xx = tmp2[:, 1]
yy = tmp2[:, 2]

plt.scatter(x,y,c='red')
plt.scatter(xx,yy,c='green')

'''
xxx = x[140:142]
yyy = y[140:142]
plt.scatter(xxx,yyy,c = 'blue')

xxx = x[152:154]
yyy = y[152:154]

plt.scatter(xxx,yyy,c = 'blue')
'''

plt.show()

