from glob       import glob
from setuptools import setup, find_packages, Extension 
from os         import getenv 


Includes  = [ '%s/include ' % getenv('VIRTUAL_ENV', '/usr/local')
            , '/usr/include' ]
Libraries = [ '%s/lib' % getenv('VIRTUAL_ENV', '/usr/local')
            , '/usr/include' ]
          

VENV = getenv('VIRTUAL_ENV', None)


libconf = Extension('appconf.__appconf'
                   , sources            = ['src/appconf.cc']
                   , extra_compile_args = [ '-std=c++0x']
                   , include_dirs       = Includes 
                   , library_dirs       = Libraries 
                   , libraries          = [ 'appconf' ])

setup(
    name = 'appconf',
    version = '0.0.1',
    author = 'Christopher J. Hanks',
    author_email = 'develop@cjhanks.name',
    packages     = ['appconf'],
    package_dir  = {'appconf': 'lib'},
    ext_modules  = [libconf]
)
