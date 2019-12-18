import os
from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

ospdir = os.path.join(os.environ['HOME'], 'software/ospray-superbuild-git')

ext_modules = [
    Extension("ospray",
        sources=['main.pyx'],
        include_dirs=[ospdir+'/include'],
        library_dirs=[ospdir+'/lib'],
        libraries=['ospray'])
]

setup(name='PyOSPRay',
      ext_modules=cythonize(ext_modules))