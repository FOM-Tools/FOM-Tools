#!/bin/env python
import numpy as np
import matplotlib.pyplot as plt
import json
from mpl_toolkits.mplot3d import Axes3D
from matplotlib import pyplot,cm
import matplotlib as mpl
#from scipy.sparse import coo_matrix
#def joinMaps(maplist):
    

def plotMap(name,histMap,lenX,title):
    cutVal=0.01 #%
    #yLabels=sorted(histMap.keys())
    #lenY=len(yLabels)
    #pos=0
    #yMap={}
    #print histMap
    normValue=0.
    for term in histMap.values():    #integrate over lib
        for c in term.values():
            normValue+=c
    print "normValue=",normValue
    yLabels=[]
    xLabels=[]
    
    for term,v in histMap.iteritems(): #loop over all
            for init,c in v.iteritems():
                cold=c
                c = float(c)/normValue
                histMap[term][init]=c #normalize to 1
                if c>cutVal : #if above threshod add labels to image
                    if term not in yLabels:
                        yLabels.append(term)
                    if init not in xLabels:
                        xLabels.append(init)
    if len(yLabels) == 0 or len(xLabels) == 0:
     print "Lib does not have anything above cut value", cutVal, name
     return
    yLabels=sorted(yLabels)
    xLabels=sorted(xLabels)
    pos=0
    yMap={}
    xMap={}
    for l in yLabels:
        yMap[l]=pos
        pos+=1
    pos=0
    for l in xLabels:
        xMap[l]=pos
        pos+=1

    lenY=len(yLabels)
    lenX=len(xLabels)
    counts=np.zeros((lenX,lenY))
    print len(xLabels),len(yLabels)
    
    for term,v in histMap.iteritems(): #create image
        if term in yMap:
            kpos=yMap[term]
            for init,c in v.iteritems():
                if c>cutVal:
                    print "term =",term,"init=", init, c
                    counts[xMap[init],kpos]=float(c)
    del xMap,yMap
    counts=counts.T #imshow displays transpose
    if lenX<5 and lenY < 5:
        print counts
    # dp=300
    # pfact=24
    # psize=(max(2,lenX/dp*pfact+2),max(2,lenY/dp*pfact+2))
    # print "psize is",psize
    # fig=plt.figure(figsize=psize,dpi=dp)
    fig=plt.figure()
    ax=fig.add_subplot(111)
    xbins=np.arange(lenX)
    ybins=np.arange(lenY)
    print counts.shape
    # ax.pcolormesh(xbins,ybins,counts)
    #plt.xticks(xbins+0.5,xLabels,rotation=45,size=4)
    #plt.yticks(ybins+0.5,yLabels,rotation=45,size=4)
    plt.xticks(xbins+0.5,xLabels,rotation=45)
    plt.yticks(ybins+0.5,list(reversed(yLabels)),rotation=45)
    plt.xlabel("Initiator",  fontsize=18)
    plt.ylabel("Terminator",  fontsize=18)
    plt.title("%s above %s%% of total"%(title,cutVal*100)) 
    #plt.imshow(counts,cmap='Greys',interpolation='nearest',aspect='auto',extent=[0,lenX,0,lenY],alpha=1)
    import numpy
    masked=numpy.ma.masked_where(counts == 0, counts)
    print title
    plt.imshow(masked,cmap=plt.cm.jet,interpolation='nearest',aspect='auto',extent=[0,lenX,0,lenY],alpha=1)
    cbar=plt.colorbar()
    plt.show()
    #cbar.ax.tick_params(labelsize=4)
    #plt.imsave(name,counts,cmap='Greys',interpolation='nearest')
    #plt.savefig(name)
    #plt.close()
    del fig
    del ax
    return
# http://qutip.googlecode.com/svn/doc/1.1.3/html/examples/examples-3d-histogram.html

    num_elem=lenX*lenY
    X,Y=np.meshgrid(xbins,ybins)
    print X.shape,Y.shape,counts.shape
    X=X.T.flatten()-0.5
    Y=Y.T.flatten()-0.5
    zpos=np.zeros(num_elem)
    dx=0.75*np.ones(num_elem)
    dy=dx.copy()
    dz=counts.flatten()
    del counts
    nrm=mpl.colors.Normalize(0,1)
    colors=cm.jet(nrm(dz))
    fig=plt.figure(figsize=psize,dpi=dp)
    #ax = Axes3D(fig,azim=-40,elev=70)
    ax=fig.add_subplot(111,projection='3d')

    #ax = Axes3D(fig)
    ax.bar3d(X, Y, zpos, dx, dy, dz, color=colors)
    # ax.axes.w_xaxis.set_major_locator(IndexLocator(1,-0.5)) #set x-ticks to integers
    # ax.axes.w_yaxis.set_major_locator(IndexLocator(1,-0.5)) #set y-ticks to integers
    # ax.axes.w_zaxis.set_major_locator(IndexLocator(1,0)) #set z-ticks to integer
    ax.set_zlim3d([0,1.1])
    cax,kw=mpl.colorbar.make_axes(ax,shrink=.75,pad=.02) #add colorbar with normalized range
    cb1=mpl.colorbar.ColorbarBase(cax,cmap=cm.jet,norm=nrm)

    # surf = ax.plot_surface(X, Y, counts, rstride=1, cstride=1, cmap='hot', linewidth=0, antialiased=False)
    #ax.set_zlim(0.,1.01)
    plt.savefig(name[:-4]+"_3D.png")
    plt.close()
    del dx
    del dy
    del dz,zpos
    del fig
    del ax
    del xbins,ybins
    del X
    del Y

def plotMaps(libMaps,suffix):
    count=0
    for k,v in libMaps.iteritems():
        #if count>3:break
        count+=1
        n=k.split("/")[-1][:-3]
        print "plotmaps k",k,v[1],n
        if len(v[0]) > 0:
            plotMap(n+suffix+".png",v[0],v[1],"Correlations of stack ids of %s for %s"%(suffix,n))

f=open("tuple.json")
stackIds,maps=json.load(f)

callHists={}
for k,v in maps.iteritems():
    callHists[k]=[{},0]
    calls=callHists[k][0]
    hist2d=v[4]
    lenX=0
    for term,d in hist2d.iteritems():
        h={}
        calls[term]=h
        for init,count in d.iteritems():
            h[init]=count[0]
        if len(h) > lenX: lenX=len(h)
    callHists[k][1]=lenX

sizeHists={}
for k,v in maps.iteritems():
    sizeHists[k]=[{},0]
    sizes=sizeHists[k][0]
    hist2d=v[4]
    lenX=0
    for term,d in hist2d.iteritems():
        h={}
        sizes[term]=h
        for init,count in d.iteritems():
            h[init]=count[1]
        if len(h) > lenX: lenX=len(h)
    sizeHists[k][1]=lenX

plotMaps(callHists,"Calls")
plotMaps(sizeHists,"MallocSize")

callHists={}
for k,v in maps.iteritems():
    callHists[k]=[{},0]
    calls=callHists[k][0]
    hist2d=v[5]
    lenX=0
    for term,d in hist2d.iteritems():
        h={}
        calls[term]=h
        for init,count in d.iteritems():
            h[init]=count[0]
        if len(h) > lenX: lenX=len(h)
    callHists[k][1]=lenX

sizeHists={}
for k,v in maps.iteritems():
    sizeHists[k]=[{},0]
    sizes=sizeHists[k][0]
    hist2d=v[5]
    lenX=0
    for term,d in hist2d.iteritems():
        h={}
        sizes[term]=h
        for init,count in d.iteritems():
            h[init]=count[1]
        if len(h) > lenX: lenX=len(h)
    sizeHists[k][1]=lenX


plotMaps(callHists,"UBCCalls")
plotMaps(sizeHists,"UBCMallocSize")
