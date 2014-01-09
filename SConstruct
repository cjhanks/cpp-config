# vim: filetype=python: tw=100

from os         import getenv
from os.path    import join


AddOption('--prefix'
        , dest      = 'prefix'
        , nargs     = 1
        , type      = 'string'
        , action    = 'store'
        , default   = getenv('PREFIX', '/usr/local')
        )


Env = Environment()
Env.Append(CCFLAGS = [ '-Wall'
                     , '-Wextra'
                     , '-Werror'
                     , '-g'
                     , '-O3'
                     , '-std=c++11' ])
Env.Append(CPPPATH   = ['include', 'src'])
Env.Append(LINKFLAGS = ['-rdynamic', '-lrt' ])                            

lib = Env.SharedLibrary('appconf', source = Glob('src/*.cc'))

Env.Alias('install', Env.Install(join(GetOption('prefix'), 'lib'), lib))
Env.Alias('install'
        , Env.InstallAs(join(GetOption('prefix'), 'include', 'appconf')
                      , Dir('#include')))

