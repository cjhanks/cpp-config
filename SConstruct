# vim: filetype=python: tw=100


Env = Environment()
Env.Append(CCFLAGS = [ '-Wall'
                     , '-Wextra'
                     , '-Werror'
                     , '-g'
                     , '-O0'
                     , '-DCONFIG_SINGLETON'
                     , '-std=c++11' ])
Env.Append(CPPPATH   = ['include', 'src'])
Env.Append(LINKFLAGS = ['-rdynamic', '-lrt' ])                            

lib = Env.SharedLibrary('conf', source = Glob('src/*.cc'))

t0  = Env.Program('tst0', source = ['test/tst0.cc', lib])
t1  = Env.Program('tst1', source = ['test/tst1.cc', lib])
t2  = Env.Program('tst2', source = ['test/tst2.cc', lib])
t3  = Env.Program('tst3', source = ['test/tst3.cc', lib])

example  = Env.Program('example', source = ['test/example.cc', lib])
example  = Env.Program('perf-0', source = ['test/perf-0.cc', lib])

Env.Alias('install', Env.Install('/usr/local/lib', lib))
Env.Alias('install', Env.Install('/usr/local/include/conf', 'include'))

