from os import environ
from sys import path
path.append(environ['NETGENDIR']+'/../lib')
del environ
del path

from sys import platform as __platform

__all__ = ['ngstd','bla','fem','la','comp','solve'];
 
# print ("importing ngsolve modules")
# from . import ngstd
# from . import bla
# from . import fem
# from . import la
# from . import comp
# from . import solve
# print ("done main init")
