from glob import glob
from setuptools import setup, find_packages, Extension


libconf = Extension('pyconf.__pyconf'
                   , sources            = ['src/libconf.cc']
                   , extra_compile_args = [ '-std=c++0x']
                   , include_dirs       = [ '/usr/local/include'
                                          , '/usr/include'
                                          , '$VIRTUAL_ENV/include' ]
                   , library_dirs       = [ '/usr/local/lib'
                                          , '/usr/lib'     
                                          , '$VIRTUAL_ENV/lib' ]
                   , libraries          = [ 'conf' ])

setup(
    name = 'pyconf',
    version = '0.0.1',
    author = 'Christopher J. Hanks',
    author_email = 'develop@cjhanks.name',
    packages     = ['pyconf'],
    package_dir  = {'pyconf': 'lib'},
    ext_modules  = [libconf]
)
