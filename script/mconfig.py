import re, argparse, sys, os, string, shlex, subprocess, glob, parser, hashlib, json, errno
from collections import OrderedDict, namedtuple
import curses.ascii

# Py3 stuff...
is_py3 = sys.hexversion >= 0x3000000
if is_py3:
    basestring = str
def dirname(fn):
    return os.path.dirname(fn) or '.'

def makedirs(path):
    try:
        os.makedirs(path)
    except OSError as e:
        if e.errno == errno.EEXIST and os.path.isdir(path):
            return
        raise

def indentify(s, indent='    '):
    return s.replace('\n', '\n' + indent)

def log(x):
    sys.stdout.write(x)
    config_log.write(x)

def to_upper_and_underscore(s):
    return s.upper().replace('-', '_')

def argv_to_shell(argv):
    quoteds = []
    for arg in argv:
        if re.match('^[a-zA-Z0-9_\.@/+=,-]+$', arg):
            quoteds.append(arg)
        else:
            quoted = ''
            for c in arg:
                if c == '\n':
                    quoted += r'\n'
                elif c in r'$`\"':
                    quoted += '\\' + c
                elif not curses.ascii.isprint(c):
                    quoted += r'\x%02x' % ord(c)
                else:
                    quoted += c
            quoteds.append('"' + quoted + '"')
    return ' '.join(quoteds)


def init_config_log():
    global config_log
    config_log = open('config.log', 'w')
    config_log.write(argv_to_shell(sys.argv) + '\n')

# a wrapper for subprocess that logs results
# returns (stdout, stderr, status) [even if Popen fails]
def run_command(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs):
    shell = argv_to_shell(cmd)
    config_log.write("Running command '%s'\n" % (shell,))

    isatty = sys.stdout.isatty()
    if isatty:
        sys.stdout.write('>>> ' + shell) # no \n
        sys.stdout.flush()

    try:
        p = subprocess.Popen(cmd, stdout=stdout, stderr=stderr, **kwargs)
    except OSError:
        config_log.write('  OSError\n')
        return '', '', 127
    so, se = [o.decode('utf-8') for o in p.communicate()]

    if isatty:
        sys.stdout.write('\033[2K\r')

    if p.returncode != 0:
        config_log.write('  failed with status %d\n' % (p.returncode,))
    config_log.write('-----------\n')
    config_log.write('  stdout:\n')
    config_log.write(so.rstrip())
    config_log.write('\n  stderr:\n')
    config_log.write(se.rstrip())
    config_log.write('\n-----------\n')
    return so, se, p.returncode

class DependencyNotFoundException(Exception):
    pass

# it must take no arguments, and throw DependencyNotFoundException on failure
class memoize(object):
    def __init__(self, f):
        self.f = f
    def __call__(self):
        if hasattr(self, 'threw'):
            raise self.threw
        elif hasattr(self, 'result'):
            return self.result
        else:
            try:
                self.result = self.f()
                return self.result
            except DependencyNotFoundException as threw:
                self.threw = threw
                raise

class Pending(object):
    def __repr__(self):
        return 'Pending(%x%s)' % (id(self), ('; value=%r' % (self.value,)) if hasattr(self, 'value') else '')
    def resolve(self):
        return self.value
    # xxx py3
    def __getattribute__(self, attr):
        try:
            return object.__getattribute__(self, attr)
        except AttributeError:
            if attr is 'value':
                raise AttributeError
            return PendingAttribute(self, attr)

class PendingOption(Pending, namedtuple('PendingOption', 'opt')):
    def resolve(self):
        return self.opt.value
    def __repr__(self):
        return 'PendingOption(%s)' % (self.opt.name,)
    def __getattribute__(self, attr):
        die

class SettingsGroup(object):
    def __init__(self, group_parent=None, inherit_parent=None, name=None):
        object.__setattr__(self, 'group_parent', group_parent)
        object.__setattr__(self, 'inherit_parent', inherit_parent)
        object.__setattr__(self, 'vals', OrderedDict())
        if name is None:
            name = '<0x%x>' % (id(self),)
        object.__setattr__(self, 'name', name)
    @staticmethod
    def get_meat(self, attr, exctype=KeyError):
        allow_pending = not did_parse_args
        try:
            obj = object.__getattribute__(self, 'vals')[attr]
        except KeyError:
            inherit_parent = object.__getattribute__(self, 'inherit_parent')
            if inherit_parent is not None:
                ret = SettingsGroup.get_meat(inherit_parent, attr, exctype)
                if isinstance(ret, SettingsGroup):
                    ret = self[attr] = ret.specialize(name='%s.%s' % (object.__getattribute__(self, 'name'), attr), group_parent=self)
                return ret
            raise exctype(attr)
        else:
            if isinstance(obj, Pending):
                try:
                    return obj.resolve()
                except:
                    if not allow_pending:
                        raise Exception("setting %r is pending; you need to set it" % (attr,))
                    return obj
            return obj
    def __getattribute__(self, attr):
        try:
            return object.__getattribute__(self, attr)
        except AttributeError:
            return SettingsGroup.get_meat(self, attr, AttributeError)
    def __setattr__(self, attr, val):
        try:
            object.__getattribute__(self, attr)
        except:
            self[attr] = val
        else:
            object.__setattribute__(self, attr, val)
    def __getitem__(self, attr):
        return SettingsGroup.get_meat(self, attr, KeyError)
    def __setitem__(self, attr, val):
        self.vals[attr] = val
    def get(self, attr, default=None):
        try:
            return self[attr]
        except KeyError:
            return default

    def __iter__(self):
        return self.vals.__iter__()
    def items(self):
        return self.vals.items()

    def __str__(self):
        s = 'SettingsGroup %s {\n' % (self.name,)
        o = self
        while True:
            for attr, val in o.vals.items():
                s += '    %s: %s\n' % (attr, indentify(str(val)))
            if o.inherit_parent is None:
                break
            o = o.inherit_parent
            s += '  [inherited from %s:]\n' % (o.name,)
        s += '}'
        return s

    def add_setting_option(self, name, optname, optdesc, default, **kwargs):
        def f(value):
            self[name] = value
        if isinstance(default, str):
            old = default
            default = lambda: expand(old, self)
        opt = Option(optname, optdesc, f, default, **kwargs)
        self[name] = PendingOption(opt)
        return opt

    def specialize(self, name=None, group_parent=None, **kwargs):
        sg = SettingsGroup(inherit_parent=self, group_parent=group_parent, name=name)
        for key, val in kwargs.items():
            sg[key] = val
        return sg

    def new_child(self, name, *args, **kwargs):
        assert name not in self
        sg = SettingsGroup(group_parent=self, name='%s.%s' % (self.name, name), *args, **kwargs)
        self[name] = sg
        return sg

class OptSection(object):
    def __init__(self, desc):
        self.desc = desc
        self.opts = []
        all_opt_sections.append(self)
    def move_to_end(self):
        all_opt_sections.remove(self)
        all_opt_sections.append(self)

class Option(object):
    def __init__(self, name, help, on_set, default=None, bool=False, opposite=None, show=True, section=None, metavar=None, type=str, **kwargs):
        if name.startswith('--'):
            self.is_env = False
            assert set(kwargs).issubset({'nargs', 'choices', 'required', 'metavar'})
        elif name.endswith('='):
            self.is_env = True
            assert len(kwargs) == 0
            assert bool is False
        else:
            raise ValueError("name %r should be '--opt' or 'ENV='" % (name,))
        self.name = name
        self.help = help
        self.default = default
        self.on_set = on_set
        self.show = show
        self.type = type
        if metavar is None:
            metavar = '...'
        self.metavar = metavar
        self.bool = bool
        if bool:
            if opposite is None:
                if name.startswith('--enable-'):
                    opposite = '--disable-' + name[9:]
                elif name.startswith('--with-'):
                    opposite = '--without-' + name[7:]
                else:
                    raise ValueError("need opposite for bool option %r" %(name,))
            self.opposite = opposite
        self.section = section if section is not None else default_opt_section
        self.section.opts.append(self)
        self.argparse_kw = kwargs.copy()
        all_options.append(self)
        if name in all_options_by_name:
            raise KeyError('trying to create Option with duplicate name %r; old is:\n%r' % (name, all_options_by_name[name]))
        all_options_by_name[name] = self
    def __repr__(self):
        value = repr(self.value) if hasattr(self, 'value') else '<none yet>'
        return 'Option(name=%r, help=%r, value=%s, default=%r)' % (self.name, self.help, value, self.default)

    def need(self):
        self.show = True

    def set(self, value):
        if not self.show:
            # If you didn't mention the option in help, you don't get no stinking value.  This is for ignored options only.
            return
        if value is None:
            value = self.default
            if callable(value): # Pending
                value = value()
        self.value = value
        if self.on_set is not None:
            self.on_set(value)

def parse_expander(fmt):
    bits = []
    z = 0
    while True:
        y = fmt.find('(', z)
        if y == -1:
            bits.append(fmt[z:])
            break
        bits.append(fmt[z:y])
        should_shlex_result = False
        if fmt[y+1:y+2] == '*':
            should_shlex_result = True
            y += 1
        try:
            parser.expr(fmt[y+1:])
        except SyntaxError as e:
            offset = e.offset
            if offset == 0 or fmt[y+1+offset-1] != ')':
                raise
            bits.append((compile(fmt[y+1:y+1+offset-1], '<string>', 'eval'), should_shlex_result))
            z = y+1+offset
    return bits

def eval_expand_bit(bit, settings, extra_vars={}):
    dep = eval(bit, {}, settings.specialize(**extra_vars))
    if isinstance(dep, Pending):
        dep = dep.resolve()
    return dep

def expand(fmt, settings, extra_vars={}):
    bits = parse_expander(fmt)
    return ''.join((bit if isinstance(bit, basestring) else eval_expand_bit(bit[0], settings, extra_vars)) for bit in bits)

def expand_argv(argv, settings, extra_vars={}):
    if isinstance(argv, basestring):
        bits = parse_expander(argv)
        shell = ''.join(bit if isinstance(bit, basestring) else '(!)' for bit in bits)
        codes = [bit for bit in bits if not isinstance(bit, basestring)]
        argv = shlex.split(shell)
        out_argv = []
        for arg in argv:
            first = True
            out_argv.append('')
            for bit in arg.split('(!)'):
                if not first:
                    code, should_shlex_result = codes.pop(0)
                    res = eval_expand_bit(code, settings, extra_vars)
                    res = shlex.split(res) if should_shlex_result else [res]
                    out_argv[-1] += res[0]
                    out_argv.extend(res[1:])
                first = False
                out_argv[-1] += bit
        return out_argv
    else:
        return [expand(arg, settings, extra_vars) for arg in argv]

def installation_dirs_group(sg):
    section = OptSection('Fine tuning of the installation directories:')
    for name, optname, optdesc, default in [
        ('prefix', '--prefix', '', '/usr/local'),
        ('exec_prefix', '--exec-prefix', '', '(prefix)'),
        ('bin', '--bindir', '', '(exec_prefix)/bin'),
        ('sbin', '--sbindir', '', '(exec_prefix)/sbin'),
        ('libexec', '--libexecdir', '', '(exec_prefix)/libexec'),
        ('etc', '--sysconfdir', '', '(prefix)/etc'),
        ('var', '--localstatedir', '', '(prefix)/var'),
        ('lib', '--libdir', '', '(prefix)/lib'),
        ('include', '--includedir', '', '(prefix)/include'),
        ('datarootdir', '--datarootdir', '', '(prefix)/share'),
        ('share', '--datadir', '', '(datarootdir)'),
        ('locale', '--localedir', '', '(datarootdir)/locale'),
        ('man', '--mandir', '', '(datarootdir)/man'),
        ('doc', '--docdir', '', '(datarootdir)/doc/(group_parent.package_unix_name)'),
        ('html', '--htmldir', '', '(doc)'),
        ('pdf', '--pdfdir', '', '(doc)'),
    ]:
        sg.add_setting_option(name, optname, optdesc, default, section=section, show=False)
    for ignored in ['--sharedstatedir', '--oldincludedir', '--infodir', '--dvidir', '--psdir']:
        Option(ignored, 'Ignored autotools compatibility setting', None, section=section, show=False)

def _make_argparse(include_unused, include_env):
    parser = argparse.ArgumentParser(
        add_help=False,
        usage='configure [OPTION]... [VAR=VALUE]...',
        prefix_chars=('-' + string.ascii_letters if include_env else '-'),
    )
    parser.add_argument('--help', action='store_true', help='Show this help', dest='__help')
    parser.add_argument('--help-all', action='store_true', help='Show this help, including unused options', dest='__help_all')
    for sect in all_opt_sections:
        def include(opt):
            return (include_unused or opt.show) and (include_env or not opt.is_env)
        if not any(map(include, sect.opts)):
            continue
        ag = parser.add_argument_group(description=sect.desc)
        for opt in sect.opts:
            if not include(opt):
                continue
            kw = opt.argparse_kw
            if not opt.bool:
                kw = kw.copy()
                kw['type'] = opt.type
                kw['metavar'] = opt.metavar
            ag.add_argument(opt.name,
                            action='store_true' if opt.bool else 'store',
                            dest=opt.name[2:],
                            help=opt.help,
                            default=None,
                            **kw)
            if opt.bool and include_unused:
                ag.add_argument(opt.opposite,
                                action='store_false',
                                dest=opt.name[2:],
                                default=None,
                                **kw)
    return parser

def _print_help(include_unused=False):
    parser = _make_argparse(include_unused, include_env=True)
    parser.print_help()

def parse_args():
    will_need(pre_parse_args_will_need)
    default_opt_section.move_to_end()
    parser = _make_argparse(include_unused=True, include_env=False)
    args, argv = parser.parse_known_args()
    if args.__help or args.__help_all:
        _print_help(include_unused=args.__help_all)
        sys.exit(1)
    unrecognized_env = []
    def do_env_arg(arg):
        m = re.match('([^- ]+)=(.*)', arg)
        if not m:
            return True # keep for unrecognized
        if m.group(1) + '=' not in all_options_by_name:
            unrecognized_env.append(arg)
        else:
            os.environ[m.group(1)] = m.group(2)
        return False
    unrecognized_argv = list(filter(do_env_arg, argv))
    if unrecognized_argv:
        print ('unrecognized arguments: %s' % (argv_to_shell(unrecognized_argv),))
    if unrecognized_env:
        print ('unrecognized environment: %s' % (argv_to_shell(unrecognized_env),))
    if unrecognized_argv or unrecognized_env:
        _print_help()
        sys.exit(1)

    for opt in all_options:
        try:
            if opt.is_env:
                name = opt.name[:-1]
                opt.set(opt.type(os.environ[name]) if name in os.environ else None)
            else:
                opt.set(getattr(args, opt.name[2:]))
        except DependencyNotFoundException as e:
            def f(): raise e
            post_parse_args_will_need.append(f)
        #print args._unrecognized_args

    global did_parse_args
    did_parse_args = True
    will_need(post_parse_args_will_need)

# -- toolchains --
class Triple(namedtuple('Triple', 'triple arch vendor os abi')):
    def __new__(self, triple):
        if isinstance(triple, Triple):
            return triple
        else:
            bits = triple.split('-')
            numbits = len(bits)
            if numbits > 4:
                raise Exception('strange triple %r' % (triple,))
            if numbits in (2, 3) and bits[1] not in ('unknown', 'none', 'pc'):
                # assume the vendor was left out
                bits.insert(1, None)
            return super(Triple, self).__new__(self, triple, *((bits.pop(0) if bits else None) for i in range(4)))
    def __str__(self):
        return self.triple

class Machine(object):
    def __init__(self, name, settings, triple_help, triple_default):
        self.name = name
        self.settings = settings
        settings.new_child(name)
        def on_set(val):
            self.triple = Triple(val)
        if isinstance(triple_default, basestring):
            triple_help += '; default: %r' % (triple_default,)
        self.triple_option = Option('--' + name, help=triple_help, default=triple_default, on_set=on_set, type=Triple, section=triple_options_section)
        self.triple = PendingOption(self.triple_option)

        self.toolchains = memoize(self.toolchains)
        self.c_tools = memoize(self.c_tools)
        self.darwin_target_conditionals = memoize(self.darwin_target_conditionals)

        self.flags_section = OptSection('Compiler/linker flags (%s):' % (self.name,))
        self.tools_section = OptSection('Tool overrides (%s):' % (self.name,))

    def __eq__(self, other):
        return self.triple == other.triple
    def __ne__(self, other):
        return self.triple != other.triple
    def __repr__(self):
        return 'Machine(name=%r, triple=%s)' % (self.name, repr(self.triple) if hasattr(self, 'triple') else '<none yet>')

    def is_cross(self):
        # This is only really meaningful in GNU land, as it decides whether to
        # prepend the triple (hopefully other targets are sane enough not to
        # have a special separate "cross compilation mode" that skips
        # configuration checks, but...).  Declared here because it may be
        # useful to override.
        if not hasattr(self, '_is_cross'):
            self._is_cross = self.triple != self.settings.build_machine().triple
        return self._is_cross

    def is_darwin(self):
        return (self.triple.os is not None and 'darwin' in self.triple.os) or \
            (self.triple.triple == '' and os.path.exists('/System/Library/Frameworks'))

    def is_ios(self): # memoized
        if not self.is_darwin():
            return False
        tc = self.darwin_target_conditionals()
        return any(tc.get(flag) for flag in ['TARGET_OS_IOS', 'TARGET_OS_SIMULATOR', 'TARGET_OS_IPHONE', 'TARGET_IPHONE_SIMULATOR'])

    def is_macosx(self):
        return self.is_darwin() and not self.is_ios()

    # Get a list of appropriate toolchains.
    def toolchains(self): # memoized
        tcs = []
        if os.path.exists('/usr/bin/xcrun'):
            self.xcode_toolchain = XcodeToolchain(self, self.settings)
            tcs.append(self.xcode_toolchain)
        tcs.append(UnixToolchain(self, self.settings))
        return tcs

    #memoize
    def darwin_target_conditionals(self):
        return calc_darwin_target_conditionals(self.c_tools(), self.settings)
    def will_need_darwin_target_conditionals(self):
        self.c_tools().cpp.required()

    #memoize
    def c_tools(self):
        return CTools(self.settings, self, self.toolchains())

class CLITool(object):
    def __init__(self, name, defaults, env, machine, toolchains, dont_suffix_env=False, section=None):
        if section is None:
            section = machine.tools_section
        self.name = name
        self.defaults = defaults
        self.env = env
        self.toolchains = toolchains
        self.needed = False
        self.machine = machine
        if machine.name != 'host' and not dont_suffix_env:
            env = '%s_FOR_%s' % (env, to_upper_and_underscore(machine.name))
        def on_set(val):
            if val is not None:
                self.argv_from_opt = shlex.split(val)
        self.argv_opt = Option(env + '=', help='Default: %r' % (defaults,), on_set=on_set, show=False, section=section)
        self.argv = memoize(self.argv)

    def __repr__(self):
        return 'CLITool(name=%r, defaults=%r, env=%r)' % (self.name, self.defaults, self.env)

    def optional_nocheck(self):
        self.argv_opt.need()

    def optional(self):
        self.optional_nocheck()
        def f():
            try:
                self.argv()
            except DependencyNotFoundException:
                pass
        post_parse_args_will_need.append(f)

    def required(self):
        self.argv_opt.need()
        post_parse_args_will_need.append(lambda: self.argv())

    def argv(self): # mem
        if not self.argv_opt.show:
            raise Exception("You asked for argv but didn't call required() or optional() before parsing args: %r" % (self,))
        # If the user specified it explicitly, don't question.
        if hasattr(self, 'argv_from_opt'):
            log('Using %s from command line: %s\n' % (self.name, argv_to_shell(self.argv_from_opt)))
            return self.argv_from_opt

        return self.argv_non_opt()

    # overridable
    def argv_non_opt(self):
        failure_notes = []
        for tc in self.toolchains:
            argv = tc.find_tool(self, failure_notes)
            if argv is not None:
                log('Found %s%s: %s\n' % (
                    self.name,
                    (' for %r' % (self.machine.name,) if self.machine is not None else ''),
                    argv_to_shell(argv)))
                return argv

        log('** Failed to locate %s\n' % (self.name,))
        for n in failure_notes:
            log('  note: %s\n' % indentify(n, '  '))
        raise DependencyNotFoundException

    def locate_in_paths(self, prefix, paths):
        for path in paths:
            for default in self.defaults:
                default = prefix + default
                filename = os.path.join(path, default)
                if os.path.exists(filename):
                    return [filename]
        return None

class UnixToolchain(object):
    def __init__(self, machine, settings):
        self.machine = machine
        self.settings = settings

    def find_tool(self, tool, failure_notes):
        # special cases
        if tool.name == 'cpp':
            try:
                cc = self.machine.c_tools().cc.argv()
            except DependencyNotFoundException:
                pass
            else:
                return cc + ['-E']
        return self.find_tool_normal(tool, failure_notes)

    def find_tool_normal(self, tool, failure_notes):
        prefix = ''
        if self.machine.is_cross():
            prefix = self.machine.triple.triple + '-'
            failure_notes.append('detected cross compilation, so searched for %s-%s' % (self.machine.triple.triple, tool.name))
        return tool.locate_in_paths(prefix, self.settings.tool_search_paths)

def calc_darwin_target_conditionals(ctools, settings):
    if not os.path.exists(settings.out):
        os.makedirs(settings.out)
    fn = os.path.join(settings.out, '_calc_darwin_target_conditionals.c')
    with open(fn, 'w') as fp:
        fp.write('#include <TargetConditionals.h>\n')
    so, se, st = run_command(ctools.cpp.argv() + ['-dM', fn])
    if st:
        log('* Error: Darwin platform but no TargetConditionals.h?\n')
        raise DependencyNotFoundException
    # note: TARGET_CPU are no good because there could be multiple arches
    return {env: bool(int(val)) for (env, val) in re.findall('^#define (TARGET_OS_[^ ]*)\s+(0|1)\s*$', so, re.M)}

# Reads a binary or XML plist (on OS X)
def read_plist(gunk):
    import plistlib
    if sys.version_info >= (3, 0):
        return plistlib.loads(gunk) # it can do it out of the box
    else:
        if gunk.startswith('bplist'):
            p = subprocess.Popen('plutil -convert xml1 - -o -'.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            gunk, _ = p.communicate(gunk)
            assert p.returncode == 0

        return plistlib.readPlistFromString(gunk)

class XcodeToolchain(object):
    def __init__(self, machine, settings):
        self.machine = machine
        prefix = (machine.name + '-') if machine.name != 'host' else ''
        section = OptSection('Xcode SDK options (%s):' % (machine.name,))
        name = '--%sxcode-sdk' % (prefix,)
        self.sdk_opt = Option(name, help='Use Xcode SDK - `xcodebuild -showsdks` lists; typical values: macosx, iphoneos, iphonesimulator, watchos, watchsimulator', on_set=None, section=section)
        name = '--%sxcode-archs' % (prefix,)
        self.arch_opt = Option(name, help='Comma-separated list of -arch settings for use with an Xcode toolchain', on_set=self.on_set_arch, section=section)
        self.ok = False

    def on_set_arch(self, arch):
        self.sdk = self.sdk_opt.value
        some_explicit_xcode_request = bool(self.sdk or arch)
        tarch = arch
        if not arch and self.machine.triple.arch is not None: 
            tarch = self.machine.triple.arch
            if tarch == 'arm':
                #log("Warning: treating 'arm' in triple %r as '-arch armv7'; you can specify a triple like 'armv7-apple-darwin10', or override with %r\n" % (self.machine.triple.triple, self.arch_opt.name))
                tarch = 'armv7'
            elif tarch == 'armv8': # XXX is this right?
                tarch = 'arm64'
        if not self.sdk:
            is_armish = tarch is not None and tarch.startswith('arm')
            self.sdk = 'iphoneos' if is_armish else 'macosx'
        self.is_ios = 'macos' not in self.sdk
        # this is used for arch and also serves as a check
        sdk_platform_path, _, code = run_command(['/usr/bin/xcrun', '--sdk', self.sdk, '--show-sdk-platform-path'])
        if code == 127:
            log('* Failed to run /usr/bin/xcrun\n')
            if some_explicit_xcode_request:
                raise DependencyNotFoundException
            return
        elif code:
            log('* Xcode SDK %r not found\n' % (self.sdk,))
            if some_explicit_xcode_request:
                raise DependencyNotFoundException
            return
        self.sdk_platform_path = sdk_platform_path.rstrip()
        log('Xcode SDK platform path: %r\n' % (self.sdk_platform_path,))

        self.archs = self.get_archs(arch, tarch)
        if self.archs is None:
            log("*** %s default Xcode SDK for %r because %s; pass %s=arch1,arch2 to override\n" % (
                "Can't use" if some_explicit_xcode_request else "Not using",
                self.machine.name,
                ("triple architecture %r doesn't seem to be valid" % (tarch,)) if tarch else "I couldn't guess a list of architectures from the SDK",
                self.arch_opt.name,
            ))
            if some_explicit_xcode_request:
                raise DependencyNotFoundException
            return
        log('Using architectures for %r: %s\n' % (self.machine.name, repr(self.archs) if self.archs != [] else '(native)'))
        self.ok = True

    def get_archs(self, arch, tarch):
        if arch:
            return re.sub('\s', '', arch).split(',')
        if tarch:
            # we need to validate it
            sod, sed, code = run_command(['/usr/bin/xcrun', '--sdk', self.sdk, 'ld', '-arch', tarch])
            if 'unsupported arch' in sed:
                return None
            return [tarch]
        triple = self.machine.triple
        # try to divine appropriate architectures
        # this may fail with future versions of Xcode, but at least we tried
        if self.sdk_platform_path.endswith('MacOSX.platform'):
            # Assume you just wanted to build natively
            return []
        xcspecs = glob.glob('%s/Developer/Library/Xcode/Specifications/*Architectures.xcspec' % (self.sdk_platform_path,)) + \
                  glob.glob('%s/Developer/Library/Xcode/PrivatePlugIns/*/Contents/Resources/Device.xcspec' % (self.sdk_platform_path,))
        for spec in xcspecs:
            def f():
                try:
                    pl = read_plist(open(spec, 'rb').read())
                except:
                    raise
                    return
                if not isinstance(pl, list):
                    return
                for item in pl:
                    if not isinstance(item, dict):
                        return
                    if item.get('ArchitectureSetting') != 'ARCHS_STANDARD':
                        return
                    archs = item.get('RealArchitectures')
                    if not isinstance(archs, list) and not all(isinstance(arch, basestring) for arch in archs):
                        return
                    return archs
            archs = f()
            if archs is not None:
                return archs
            log('(Failed to divine architectures from %r for some reason...)\n' % (spec,))

        # give up
        return None

    def arch_flags(self):
        return [flag for arch in self.archs for flag in ('-arch', arch)]

    def find_tool(self, tool, failure_notes):
        # special cases
        if tool.name == 'cpp':
            argv = ['/usr/bin/xcrun', '--sdk', self.sdk, 'cc', '-E']
            sod, sed, code = run_command(['/usr/bin/xcrun', '--sdk', self.sdk, '-f', tool.name])
            if code != 0:
                failure_notes.append(sed)
                return None
            return argv
        return self.find_tool_normal(tool, failure_notes)

    def find_tool_normal(self, tool, failure_notes):
        if not self.ok:
            return None
        arch_flags = self.arch_flags() if tool.name in {'cc', 'cxx', 'nm'} else []
        argv = ['/usr/bin/xcrun', '--sdk', self.sdk, tool.name] + arch_flags
        sod, sed, code = run_command(['/usr/bin/xcrun', '--sdk', self.sdk, '-f', tool.name])
        if code != 0:
            failure_notes.append(sed)
            return None
        # note: we can't just use the found path because xcrun sets some env magic
        return argv

# Just a collection of common tools, plus flag options
class CTools(object):
    def __init__(self, settings, machine, toolchains):
        group = settings[machine.name]

        tools = [
            ('cc',   ['cc', 'gcc', 'clang'],     'CC'),
            ('cpp',  None,                       'CPP'),
            ('cxx',  ['c++', 'g++', 'clang++'],  'CXX'),
            ('ar',),
            ('nm',),
            ('ranlib',),
            ('strip',),
            # GNU
            ('objdump', ['objdump', 'gobjdump'], 'OBJDUMP'),
            ('objcopy', ['objcopy', 'gobjcopy'], 'OBJCOPY'),
            # OS X
            ('lipo',),
            ('dsymutil',),
        ]
        for spec in tools:
            if len(spec) == 1:
                name, defaults, env = spec[0], [spec[0]], spec[0].upper()
            else:
                name, defaults, env = spec
            tool = CLITool(name, defaults, env, machine, toolchains)
            setattr(self, name, tool)

        suff = ''
        if machine.name != 'host':
            suff = '_FOR_' + to_upper_and_underscore(machine.name)
        suff += '='
        group.app_cflags = []
        group.app_cxxflags = []
        group.app_ldflags = []
        group.app_cppflags = []
        self.cflags_opt = group.add_setting_option('cflags', 'CFLAGS'+suff, 'Flags for $CC', [], section=machine.flags_section, type=shlex.split)
        self.cxxflags_opt = group.add_setting_option('cxxflags', 'CXXFLAGS'+suff, 'Flags for $CXX', [], section=machine.flags_section, type=shlex.split)
        self.ldflags_opt = group.add_setting_option('ldflags', 'LDFLAGS'+suff, 'Flags for $CC/$CXX when linking', [], section=machine.flags_section, type=shlex.split)
        self.cppflags_opt = group.add_setting_option('cppflags', 'CPPFLAGS'+suff, 'Flags for $CC/$CXX when not linking (supposed to be used for preprocessor flags)', [], section=machine.flags_section, type=shlex.split)
        settings.enable_werror_opt.need()
        settings.enable_debug_info_opt.need()


# A nicer - but optional - way of doing multiple tests that will print all the
# errors in one go and exit cleanly
def will_need(tests):
    failures = 0
    for test in tests:
        try:
            test()
        except DependencyNotFoundException:
            failures += 1
    if failures > 0:
        log('(%d failure%s.)\n' % (failures, 's' if failures != 1 else ''))
        sys.exit(1)

def relpath_if_within(tree, fn):
    rp = os.path.relpath(fn, tree)
    return None if rp.startswith('..'+os.path.sep) else rp

real_out = memoize(lambda: os.path.realpath(settings_root.out))
def clean_files(fns, settings):
    ro = real_out()
    for fn in fns:
        if not os.path.exists(fn) or os.path.isdir(fn):
            continue
        real_fn = os.path.realpath(fn)
        if not settings.allow_autoclean_outside_out and not relpath_if_within(ro, real_fn) and real_fn not in safe_to_clean:
            log("* Would clean %r as previous build leftover, but it isn't in settings.out (%r) so keeping it for safety.\n" % (fn, ro))
            continue
        log('Removing %r\n' % (fn,))
        os.remove(real_fn)
def plan_clean_target(fns, settings):
    ro = real_out()
    actions = []
    for fn in fns:
        real_fn = os.path.realpath(fn)
        if not settings.allow_autoclean_outside_out and not relpath_if_within(ro, real_fn) and real_fn not in safe_to_clean:
            actions.append(('log', "* Would clean %r, but it isn't in settings.out (%r) so keeping it for safety." % (fn, ro)))
            continue
        actions.append(('remove', fn))
    return actions

safe_to_clean = set()
def mark_safe_to_clean(fn, settings=None):
    fn = expand(fn, settings)
    safe_to_clean.add(os.path.realpath(fn))

def list_mconfig_scripts(settings):
    real_src = os.path.realpath(settings.src)
    res = []
    for mod in sys.modules.values():
        if hasattr(mod, '__file__') and relpath_if_within(real_src, os.path.realpath(mod.__file__)):
            if is_py3:
                fn = mod.__loader__.path
            else:
                fn = mod.__file__
                if fn.endswith('.pyc') or fn.endswith('.pyo'):
                    if os.path.exists(fn[:-1]):
                        fn = fn[:-1]
                    else:
                        # who knows?
                        continue
            res.append(fn)
    return res

def write_file_loudly(fn, data, perm=None):
    fn = relpath_if_within(os.getcwd(), fn) or fn
    log('Writing %s\n' % (fn,))
    with open(fn, 'w') as fp:
        fp.write(data)
    if perm is not None:
        try:
            os.chmod(fn, perm)
        except Exception as e:
            log('chmod: %r' % (e,))

class Emitter(object):
    def __init__(self, settings):
        self.settings = settings
        self.distclean_paths = self.default_distclean_paths()
        self.all_outs = set()
    def pre_output(self):
        assert not hasattr(self, 'did_output')
        self.did_output = True
    def set_default_rule(self, rule):
        self.default_rule = rule
    def filename_rel(self, fn):
        return os.path.relpath(fn, dirname(self.settings.emit_fn))
    def filename_rel_and_escape(self, fn):
        return self.filename_escape(self.filename_rel(fn))
    def add_command(self, settings, outs, ins, argvs, phony=False, *args, **kwargs):
        if kwargs.get('expand', True):
            ev = {'raw_outs': outs, 'raw_ins': ins, 'raw_argvs': argvs}
            outs = ev['outs'] = [expand(x, settings, ev) for x in outs]
            ins = ev['ins'] = [expand(x, settings, ev) for x in ins]
            argvs = [expand_argv(x, settings, ev) for x in argvs]
        if 'expand' in kwargs:
            del kwargs['expand']
        if kwargs.get('mkdirs', True):
            for dirname in set(map(os.path.dirname, outs)):
                if dirname:
                    argvs.insert(0, ['mkdir', '-p', dirname])
        if 'mkdirs' in kwargs:
            del kwargs['mkdirs']
        if not phony:
            self.all_outs.update(outs)
            if settings.enable_rule_hashing:
                sha = hashlib.sha1(json.dumps((outs, ins, argvs)).encode('utf-8')).hexdigest()
                if sha not in prev_rule_hashes:
                    clean_files(outs, settings)
                cur_rule_hashes.add(sha)
        return self.add_command_raw(outs, ins, argvs, phony, *args, **kwargs)

    def default_distclean_paths(self):
        return [
            ['file', 'config.log'],
            ['file', 'config.status'],
            ['file', 'build.ninja'],
            ['file', 'Makefile'],
            ['dir', self.settings.out],
        ]

    def emit(self, fn=None):
        if fn is None:
            fn = self.settings.emit_fn
        output = self.output()
        write_file_loudly(fn, output)

class UnixEmitter(Emitter):
    def add_unix_distclean(self):
        argvs = []
        for kind, path in self.distclean_paths:
            assert kind in ['file', 'dir']
            argvs.append(['rm', ('-rf' if kind == 'dir' else '-f'), path])
        self.add_command_raw(['distclean'], [], argvs, phony=True)
# In the future it may be desirable to use make variables and nontrivial ninja rules for efficiency.

class MakefileEmitter(UnixEmitter):
    def __init__(self, settings):
        Emitter.__init__(self, settings)
        self.banner = '# Generated by mconfig.py'
        self.makefile_bits = [self.banner]
        self.main_mk = settings.get('main_mk')
        if self.main_mk is None:
            self.main_mk = lambda: os.path.join(settings.out, 'main.mk')

    def add_all(self):
        if hasattr(self, 'default_rule'):
            if self.default_rule != 'all':
                self.add_command_raw(['all'], [self.default_rule], [], phony=True)
        else:
            log('Warning: %r: no default rule\n' % (self,))

    def add_clean(self):
        argvs = []
        for a, b in plan_clean_target(sorted(self.all_outs), self.settings):
            if a == 'log':
                argvs.append(['@echo', b])
            elif a == 'remove':
                argvs.append(['rm', '-f', b])
        self.add_command_raw(['clean'], [], argvs, phony=True)
        self.add_unix_distclean()

    @staticmethod
    def filename_escape(fn):
        if re.search('[\n\0]', fn):
            raise ValueError("your awful filename %r can't be encoded in make (probably)" % (fn,))
        return re.sub(r'([ :\$\\])', r'\\\1', fn)

    # depfile = ('makefile', filename) or ('msvc',)
    def add_command_raw(self, outs, ins, argvs, phony=False, depfile=None):
        bit = ''
        outs = ' '.join(map(self.filename_rel_and_escape, outs))
        ins = ' '.join(map(self.filename_rel_and_escape, ins))
        if phony:
            bit += '.PHONY: %s\n' % (outs,)
        bit += '%s:%s%s\n' % (outs, ' ' if ins else '', ins)
        for argv in argvs:
            bit += '\t' + argv_to_shell(argv) + '\n'
        if depfile is not None:
            if depfile[0] != 'makefile':
                raise ValueError("don't support depfile of type %r" % (depfile[0],))
            bit += '-include %s\n' % (self.filename_rel_and_escape(depfile[1]),)
        if 'all' in outs:
            self.makefile_bits.insert(0, bit)
        else:
            self.makefile_bits.append(bit)

    def output(self):
        self.pre_output()
        self.add_all()
        self.add_clean()
        return '\n'.join(self.makefile_bits)

    def emit(self):
        makefile = self.settings.emit_fn
        if self.settings.auto_rerun_config:
            main_mk = self.main_mk()
            makedirs(os.path.dirname(main_mk))
            cs_argvs = [['echo', 'Running config.status...'], ['./config.status']]
            self.add_command_raw([makefile], list_mconfig_scripts(self.settings), cs_argvs)
            Emitter.emit(self, main_mk)
            # Write the stub
            # TODO is there something better than shell?
            # TODO avoid deleting partial output?
            stub = '''
%(banner)s
_out := $(shell "$(MAKE_COMMAND)" -s -f %(main_mk_arg)s %(makefile_arg)s >&2 || echo fail)
ifneq ($(_out),fail)
include %(main_mk)s
endif
'''.lstrip() \
            % {
                'makefile_arg': argv_to_shell([makefile]),
                'main_mk_arg': argv_to_shell([main_mk]),
                'main_mk': self.filename_rel_and_escape(main_mk),
                'banner': self.banner,
            }
            write_file_loudly(makefile, stub)
        else:
            Emitter.emit(self)

    def default_outfile(self):
        return 'Makefile'

class NinjaEmitter(UnixEmitter):
    def __init__(self, settings):
        Emitter.__init__(self, settings)
        self.ninja_bits = []
        self.ruleno = 0
    @staticmethod
    def filename_escape(fn):
        if re.search('[\n\0]', fn):
            raise ValueError("your awful filename %r can't be encoded in ninja (probably)" % (fn,))
        return re.sub(r'([ :\$])', r'$\1', fn)

    def add_command(self, settings, outs, ins, argvs, *args, **kwargs):
        if self.settings.auto_rerun_config:
            kwargs['order_only_ins'] = kwargs.get('order_only_ins', []) + ['build.ninja']
        Emitter.add_command(self, settings, outs, ins, argvs, *args, **kwargs)

    def add_command_raw(self, outs, ins, argvs, phony=False, depfile=None, order_only_ins=[]):
        bit = ''
        if phony:
            if len(argvs) == 0:
                self.ninja_bits.append('build %s: phony %s%s\n' % (
                    ' '.join(map(self.filename_rel_and_escape, outs)),
                    ' '.join(map(self.filename_rel_and_escape, ins)),
                    '' if not order_only_ins else (' || ' + ' '.join(map(self.filename_rel_and_escape, order_only_ins))),
                ))
                return
            outs2 = ['__phony_' + out for out in outs]
            bit += 'build %s: phony %s\n' % (' '.join(map(self.filename_rel_and_escape, outs)), ' '.join(map(self.filename_rel_and_escape, outs2)))
            outs = outs2
        rule_name = 'rule_%d' % (self.ruleno,)
        self.ruleno += 1
        bit += 'rule %s\n' % (rule_name,)
        bit += '  command = %s\n' % (' && $\n    '.join(map(argv_to_shell, argvs)))
        if depfile:
            if depfile[0] not in ('makefile', 'msvc'):
                raise ValueError("don't support depfile of type %r" % (depfile[0],))
            bit += '  deps = %s\n' % ({'makefile': 'gcc', 'msvc': 'msvc'}[depfile[0]],)
            bit += '  depfile = %s\n' % (self.filename_rel_and_escape(depfile[1]),)
        bit += 'build %s: %s' % (' '.join(map(self.filename_rel_and_escape, outs),), rule_name)
        if ins:
            bit += ' | %s' % (' '.join(map(self.filename_rel_and_escape, ins),))
        bit += '\n'
        self.ninja_bits.append(bit)

    def add_configstatus_rule(self):
        # Unlike with make, we don't need to do this separately, before the
        # other rules are read, because ninja automatically rereads rules when
        # build.ninja has changed.
        cs_argvs = [['echo', 'Running config.status...'], ['./config.status']]
        self.add_command_raw(['build.ninja'], list_mconfig_scripts(self.settings), cs_argvs)

    def add_default(self):
        if hasattr(self, 'default_rule'):
            self.ninja_bits.append('default %s\n' % (self.default_rule,))
        else:
            log('Warning: %r: no default rule\n' % (self,))

    def output(self):
        self.pre_output()
        if self.settings.auto_rerun_config:
            self.add_configstatus_rule()
        self.add_default()
        self.add_command_raw(['clean'], [], [['ninja', '-t', 'clean']], phony=True)
        self.add_unix_distclean()
        return '\n'.join(self.ninja_bits)

    def default_outfile(self):
        return 'build.ninja'


def add_emitter_option():
    def on_set_generate(val):
        if val not in emitters:
            raise DependencyNotFoundException('Unknown build script type: %s (options: %s)' % (val, ' '.join(emitters.keys())))
        settings_root.emitter = emitters[val](settings_root)
    Option(
        '--generate',
        'The type of build script to generate.  Options: %s (default makefile)' % (', '.join(emitters.keys()),),
        on_set_generate, default='makefile', section=output_section)
    settings_root.add_setting_option('emit_fn', '--outfile', 'Output file.  Default: Makefile, build.ninja, etc.', section=output_section, default=lambda: settings_root.emitter.default_outfile())

def config_status():
    return '#!/bin/sh\n' + argv_to_shell([sys.executable] + sys.argv) + '\n'

def finish_and_emit():
    settings_root.emitter.emit()
    if settings_root.enable_rule_hashing:
        emit_rule_hashes()
    write_file_loudly('config.status', config_status(), 0o755)

def check_rule_hashes():
    if not settings_root.enable_rule_hashing:
        return
    global prev_rule_hashes, cur_rule_hashes
    cur_rule_hashes = set()
    rule_path = os.path.join(settings_root.out, 'mconfig-hashes.txt')
    try:
        fp = open(rule_path)
    except IOError:
        prev_rule_hashes = set()
        return
    prev_rule_hashes = set(json.load(fp))
    fp.close()

def emit_rule_hashes():
    makedirs(settings_root.out)
    rule_path = os.path.join(settings_root.out, 'mconfig-hashes.txt')
    with open(rule_path, 'w') as fp:
        json.dump(list(cur_rule_hashes), fp)

def get_else_and(container, key, def_func, transform_func=lambda x: x):
    try:
        val = container[key]
    except KeyError:
        return def_func()
    else:
        return transform_func(val)

def default_is_cxx(filename):
    root, ext = os.path.splitext(filename)
    return ext in ('cc', 'cpp', 'cxx', 'mm')


def get_cflags(mach_settings, is_cxx):
    return (mach_settings.app_cppflags +
        (mach_settings.app_cxxflags if is_cxx else mach_settings.app_cflags) +
        mach_settings.cppflags +
        (mach_settings.cxxflags if is_cxx else mach_settings.cflags))
def get_cc_cmd(my_settings, mach_settings, tools, fn, extra_cflags=[]):
    is_cxx = get_else_and(my_settings, 'override_is_cxx', lambda: default_is_cxx(fn))
    include_args = ['-I'+expand(inc, my_settings) for inc in my_settings.c_includes]
    dbg = ['-g'] if my_settings.enable_debug_info else []
    werror = ['-Werror'] if my_settings.enable_werror else []
    cflags = expand_argv(get_else_and(my_settings, 'override_cflags', lambda: get_cflags(mach_settings, is_cxx)), my_settings)
    cc = expand_argv(get_else_and(my_settings, 'override_cc', lambda: (tools.cxx if is_cxx else tools.cc).argv()), my_settings)
    return (cc + dbg + werror + include_args + cflags + extra_cflags, is_cxx)

# emitter:      the emitter to add rules to
# machine:      machine
# settings:     settings object; will inspect {c,cxx,cpp,ld}flags
# sources:      list of source files
# headers:      *optional* list of header files that will be used in the future to
#               generate IDE projects - unused for makefile/ninja due to
#               depfiles
# objs:         list of .o files or other things to add to the link
# link_out:     optional linker output
# link_type:    'exec', 'dylib', 'staticlib', 'obj'; default exec
# settings_cb:  (filename) -> None or a settings object to override the default
#               the following keys are accepted:
#                 override_cxx: True ($CXX) or False ($CC); ignored in IDE native mode
#                 override_cc:  override cc altogther; ignored in IDE native mode
#                 override_obj_fn: the .o file
#                 extra_deps: dependencies
# force_cli:    don't use IDEs' native C/C++ compilation mechanism
# expand:       call expand on filenames
# extra_cflags: convenience argument for extra CFLAGS (can also use settings/settings_cb)
def build_c_objs(emitter, machine, settings, sources, headers=[], settings_cb=None, force_cli=False, expand=True, extra_cflags=[]):
    tools = machine.c_tools()
    any_was_cxx = False
    obj_fns = []
    ldflag_sets = set()
    my_settings = settings
    if expand:
        _expand = lambda x: globals()['expand'](x, my_settings)
        _expand_argv = lambda x: expand_argv(x, my_settings)
    else:
        _expand = _expand_argv = lambda x: x
    env = {} # todo: ...
    headers = list(map(_expand, headers))
    extra_cflags = _expand_argv(extra_cflags)
    for fn in map(_expand, sources):
        my_settings = settings
        if settings_cb is not None:
            s = settings_cb(fn)
            if s is not None:
                my_settings = s
        obj_fn = get_else_and(my_settings, 'override_obj_fn', lambda: guess_obj_fn(fn, settings), _expand)
        mach_settings = my_settings[machine.name]
        extra_deps = list(map(_expand, my_settings.get('extra_compile_deps', [])))
        cmd, is_cxx = get_cc_cmd(my_settings, mach_settings, tools, fn, extra_cflags)
        any_was_cxx = any_was_cxx or is_cxx
        dep_fn = os.path.splitext(obj_fn)[0] + '.d'

        # we must relativize here only so that .d files work properly
        if obj_fn.startswith('/'):
            obj_fn = emitter.filename_rel(obj_fn)
        if fn.startswith('/'):
            fn = emitter.filename_rel(fn)

        cmd += ['-c', '-o', obj_fn, '-MMD', '-MF', dep_fn, fn]

        env = {
            'outs': [obj_fn],
            'ins': [fn] + extra_deps,
            'cmds': [cmd],
        }

        mce = settings.get('modify_compile')
        if mce is not None:
            mce(env)
        emitter.add_command(my_settings, env['outs'], env['ins'], env['cmds'], depfile=('makefile', dep_fn), expand=False, mkdirs=True)

        for lset in my_settings.get('obj_ldflag_sets', ()):
            ldflag_sets.add(tuple(lset))
        obj_fns.append(obj_fn)

    return obj_fns, any_was_cxx, ldflag_sets

def link_c_objs(emitter, machine, settings, link_type, link_out, objs, link_with_cxx=None, force_cli=False, expand=True, ldflags_from_sets=[]):
    if expand:
        _expand = lambda x: globals()['expand'](x, settings)
        _expand_argv = lambda x: expand_argv(x, settings)
        link_out = _expand(link_out)
        objs = list(map(_expand, objs))
    else:
        _expand = _expand_argv = lambda x: x
    tools = machine.c_tools()
    assert link_type in ('exec', 'dylib', 'staticlib', 'obj')
    if link_type in ('exec', 'dylib'):
        assert link_with_cxx in (False, True)
        cc_for_link = _expand_argv(get_else_and(settings, 'override_ld', lambda: (tools.cxx if link_with_cxx else tools.cc).argv()))
        if link_type == 'dylib':
            typeflag = ['-dynamiclib'] if machine.is_darwin() else ['-shared']
        else:
            typeflag = []
        mach_settings = settings[machine.name]
        ldflags = get_else_and(settings, 'override_ldflags', lambda:
            mach_settings.app_ldflags + mach_settings.ldflags,
            _expand_argv)
        cmds = [cc_for_link + typeflag + ['-o', link_out] + objs + ldflags_from_sets + ldflags]
        if machine.is_darwin() and settings.enable_debug_info:
            cmds.append(tools.dsymutil.argv() + [link_out])
    elif link_type == 'staticlib':
        cmds = [tools.ar.argv() + ['rcs'] + objs]
    elif link_type == 'obj':
        cmds = [tools.cc.argv() + ['-Wl,-r', '-nostdlib', '-o', link_out] + objs]
    env = {
        'outs': [link_out],
        'ins': objs,
        'cmds': cmds,
    }
    mce = settings.get('modify_link')
    if mce is not None:
        mce(env)
    emitter.add_command(settings, env['outs'], env['ins'], env['cmds'], expand=False, mkdirs=True)

def build_and_link_c_objs(emitter, machine, settings, link_type, link_out, sources, headers=[], objs=[], settings_cb=None, force_cli=False, expand=True, extra_deps=[], extra_cflags=[], extra_ldflags=[]):
    more_objs, link_with_cxx, ldflag_sets = build_c_objs(emitter, machine, settings, sources, headers, settings_cb, force_cli, expand, extra_cflags)
    ldflags_from_sets = [flag for lset in ldflag_sets for flag in lset]
    link_c_objs(emitter, machine, settings, link_type, link_out, objs + more_objs, link_with_cxx, force_cli, expand, ldflags_from_sets)

def will_build_and_link_c(machine, link_types=set(), c=True, cxx=False):
    c = machine.c_tools()
    if c:
        c.cc.required()
    if cxx:
        c.cxx.required()
    if link_types:
        c.dsymutil.optional()
    if 'staticlib' in link_types:
        c.ar.required()

def guess_obj_fn(fn, settings):
    rel = os.path.relpath(fn, settings.src)
    if not rel.startswith('..'+os.path.sep):
        rel = os.path.splitext(rel)[0] + '.o'
        return os.path.join(settings.out, rel)
    raise ValueError("can't guess .o filename for %r, as it's not in settings.src" % (fn,))


# -- init code --

init_config_log()

did_parse_args = False

all_options = []
all_options_by_name = {}
all_opt_sections = []
default_opt_section = OptSection('Uncategorized options:')
pre_parse_args_will_need = []
post_parse_args_will_need = []

settings_root = SettingsGroup(name='root')
settings_root.package_unix_name = Pending()
installation_dirs_group(settings_root.new_child('idirs'))

output_section = OptSection('Output options:')

triple_options_section = OptSection('System types:')
settings_root.build_machine = memoize(lambda: Machine('build', settings_root, 'the machine doing the build', lambda: Triple('')))
settings_root.host_machine = memoize(lambda: settings_root.build_machine() and Machine('host', settings_root, 'the machine that will run the compiled program', lambda: settings_root.build_machine().triple))
# ...'the machine that the program will itself compile programs for',

settings_root.tool_search_paths = os.environ['PATH'].split(':')

settings_root.src = dirname(sys.argv[0])
settings_root.out = os.path.join(os.getcwd(), 'out')

settings_root.enable_rule_hashing = True
settings_root.allow_autoclean_outside_out = False
post_parse_args_will_need.append(check_rule_hashes)
settings_root.auto_rerun_config = True

settings_root.c_includes = []

settings_root.enable_werror_opt = settings_root.add_setting_option('enable_werror', '--enable-werror', 'Turn warnings to errors (default on)', default=True, bool=True, show=False)
settings_root.enable_debug_info_opt = settings_root.add_setting_option('enable_debug_info', '--enable-debug-info', 'Enable -g', default=False, bool=True, show=False)

emitters = {
    'makefile': MakefileEmitter,
    'ninja': NinjaEmitter,
}

pre_parse_args_will_need.append(add_emitter_option)

# --

