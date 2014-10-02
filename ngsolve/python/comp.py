from ngsolve import __platform
if __platform.startswith('linux') or __platform.startswith('darwin'):
    # Linux or Mac OS X
    from libngcomp.ngcomp import *

if __platform.startswith('win'):
    # Windows
    from ngslib.comp import *

__all__ = ['BND', 'BilinearForm', 'COUPLING_TYPE', 'CompoundFESpace', 'ElementId', 'FESpace', 'GridFunction', 'LinearForm', 'Mesh', 'Preconditioner', 'VOL']

