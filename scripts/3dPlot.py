
from mpl_toolkits import mplot3d
from matplotlib import cm
import matplotlib.pyplot as plt
import pandas as pd
import sys, os
sys.path.append(os.path.dirname(os.path.abspath('')))
sys.path.append(os.path.join(os.path.abspath(''),'..','scripts'))
sys.path.append(os.path.join(os.path.abspath(''),'..','src'))

from Experiment import *
from Analyzer import *

matplotlib.use('macosx')

df = Analyzer("minnesota").df
results = df

X = []
Y = []
Z = []
i = 0
for r in sorted(results['weight_scale'].unique()):
    d = results[results['weight_scale'] == r]
    rni = d[d['mode'] == 0]
    emf = d[d['mode'] == 5]
    a2 = np.amin([emf.loc[i,'sequence'] for i in emf.index],axis=0)
    a1 = np.amin([rni.loc[i,'sequence'] for i in rni.index],axis=0)
    a3 = (a2-a1)/a1
    X.append(r)
    i += 1
    Z.append(a3)
X = np.array(X).reshape(len(X),1)
Z= np.array(Z)
Y = np.array(range(Z.shape[1])).reshape(Z.shape[1],1).T

fig = plt.figure()
fig.patch.set_facecolor('white')
ax = fig.add_subplot(111, projection='3d')
ax.plot_surface(X,Y,Z, cmap=cm.magma_r)
ax.set_xlabel("W")
ax.set_ylabel("$\ell$")
ax.set_zlabel("$D_r$")
plt.show()
