# vim: ft=python sts=4 ts=8 et sw=4

import os.path

VERSION='0.0.1'
APPNAME='BaczekKPAI'
srcdir = '.'
blddir = 'output'


def set_options(opt):
    opt.add_option('--spring-dir', default='../../../',
            help='Spring RTS checkout directory')
    opt.add_option('--variant', default='default',
            help="variant to build: default, debug")

    opt.tool_options('boost')
    opt.tool_options('python')


def configure(conf):
    # tool checks
    conf.check_tool('gcc')
    conf.check_tool('gxx')
    conf.check_tool('python')
    conf.check_python_version()
    conf.check_python_headers()
    conf.check_tool('boost')
    #conf.check_boost(lib='python filesystem system thread',
    conf.check_boost(lib='signals filesystem python system thread',
           kind='STATIC_BOTH', 
           static='both',
           score_version=(-1000, 1000),
           tag_minscore=1000,
           min_version='1.37.0',
           mandatory=True)
    
    #conf.find_gxx()
    # global compiler flags
    conf.env.append_value('CCFLAGS',  '-Wall')
    conf.env.append_value('CXXFLAGS',  '-Wall')
    import Options
    if Options.platform=='win32':
        conf.env.append_value('shlib_LINKFLAGS', ['-Wl,--add-stdcall-alias'])
        conf.env['shlib_PATTERN'] = '%s.dll'
    # global conf options
    from Options import options
    conf.env['spring_dir'] = os.path.abspath(options.spring_dir)
    if options.variant not in ('default', 'debug'):
        raise ValueError, 'invalid variant '+options.variant
    conf.env['variant'] = options.variant

    # variants
    env2 = conf.env.copy()
    conf.set_env_name('debug', env2)
    env2.set_variant('debug')
    # more compiler flags
    conf.env.append_value('CCFLAGS', ['-O2', '-g'])
    conf.env.append_value('CXXFLAGS', ['-O2', '-g'])
    # debug flags
    conf.setenv('debug')
    conf.env['CCFLAGS'] = '-g'
    conf.env['CXXFLAGS'] = '-g'
    

def build(bld):
    print '** Building variant', bld.env['variant']
    import glob, os.path
    def get_spring_files():
        springdir = bld.env['spring_dir']
        tocopy = \
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'CUtils', '*.cpp')) +\
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'CUtils', '*.c')) +\
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'LegacyCpp', '*.cpp')) +\
            [os.path.join(springdir, 'rts', 'System', 'float3.cpp'),
                os.path.join(springdir, 'rts', 'Game', 'GameVersion.cpp')]
        return tocopy

    spring_files = get_spring_files()
    for f in spring_files:
        bld.new_task_gen(
                name='copy',
                before='cxx cc',
                target=os.path.split(f)[-1],
                rule='cp -p %s ${TGT}'%f,
                always=True,
                on_results=True,
        )
    
    skirmishai = bld.new_task_gen(
            name="SkirmishAI",
            features='cxx cc cshlib',
            includes=['.'] + [os.path.join(bld.env['spring_dir'], x)
                for x in ('rts', 'rts/System', 'AI/Wrappers',
                    'AI/Wrappers/CUtils', 'AI/Wrappers/LegacyCpp',
                    'rts/Game')],
            uselib = '''BOOST_SYSTEM BOOST_SIGNALS BOOST_THREAD BOOST_FILESYSTEM
                        BOOST_PYTHON PYEMBED BOOST''',
            source = \
                glob.glob(os.path.join(srcdir, '*.cpp')) +\
                glob.glob(os.path.join(srcdir, 'GUI', '*.cpp')) +\
                glob.glob(os.path.join(srcdir, 'json_spirit', '*.cpp')) +\
                [os.path.split(f)[-1]
                        for f in spring_files],
            defines='BUILDING_SKIRMISH_AI BUILDING_AI',
            target='SkirmishAI',
    )

    # strip but keep debug info
    debug_info = bld.new_task_gen(
            name="save_debug",
            after="cc cxx cxx_link",
            source=bld.env['shlib_PATTERN']%"SkirmishAI",
            target="SkirmishAI.dbg",
            rule="strip --only-keep-debug -o ${TGT[0].abspath(env)} ${SRC[0].abspath(env)}",
        )
    strip = bld.new_task_gen(
            name="strip",
            after="save_debug",
            source=bld.env['shlib_PATTERN']%"SkirmishAI",
            rule="strip ${SRC[0].abspath(env)}",
            always=True,
        )
    
    # build only one variant
    for x in bld.all_task_gen:
        x.env['_VARIANT_'] = bld.env['variant']
