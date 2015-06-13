import re, argparse, sys, os
from collections import OrderedDict, namedtuple
import curses.ascii

def indentify(s, indent='    '):
    return s.replace('\n', '\n' + indent)

def argv_to_shell(argv):
    quoteds = []
    for arg in argv:
        if re.match('^[a-zA-Z0-9_\.@/+-]+$', arg):
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

class PendingOption(namedtuple('PendingOption', 'opt')):
    def need(self):
        self.opt.show = True
    def __str__(self):
        return 'PendingOption(%s)' % (self.opt.name,)

class SettingsGroup(object):
    def __init__(self, parent=None):
        object.__setattr__(self, 'parent', parent)
        object.__setattr__(self, 'vals', OrderedDict())
    @staticmethod
    def get_meat(self, attr, exctype):
        try:
            obj = object.__getattribute__(self, 'vals')[attr]
        except KeyError:
            if self.parent is not None:
                return SettingsGroup.get_meat(object.__getattribute__(self, 'parent'), attr, exctype)
            raise exctype(attr)
        else:
            if isinstance(obj, PendingOption):
                raise Exception("setting %s is pending a command line option, probably because you didn't 'need' it")
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
        return self.__getattribute__(attr)
    def __setitem__(self, attr, val):
        self.vals[attr] = val

    def __iter__(self):
        return self.vals.__iter__()
    def items(self):
        return self.vals.items()

    def __str__(self):
        s = 'SettingsGroup {\n'
        for attr, val in self.vals.items():
            s += '    %s: %s\n' % (attr, indentify(str(val)))
        s += '}'
        return s

    def relative_lookup(name):
        if name.startswith('..'):
            return self.parent.relative_lookup(name)
        else:
            bits = name.split('.', 1)
            if len(bits) == 1:
                return (self, bits[0])
            else:
                return self[bits[0]].relative_lookup(bits[1])
    def need(self, name):
        if name not in self.vals:
            raise KeyError('need setting that has not been set')
        obj = self.vals[name]
        if hasattr(obj, 'need'):
            obj.need()

    def add_setting_option(self, name, optname, optdesc, default, **kwargs):
        def f(value):
            self[name] = value
        opt = Option(optname, optdesc, default, f, **kwargs)
        self[name] = PendingOption(opt)

class OptSection(object):
    def __init__(self, desc):
        self.desc = desc
        self.opts = []
        all_opt_sections.append(self)
    def move_to_end(self):
        all_opt_sections.remove(self)
        all_opt_sections.append(self)

class Option(object):
    def __init__(self, name, help, default, on_set, bool=False, show=False, section=None, **kwargs):
        self.name = name
        self.help = help
        self.default = Expansion(default) if isinstance(default, str) else default
        self.on_set = on_set
        self.show = show
        self.bool = bool
        self.section = section if section is not None else default_opt_section
        self.section.opts.append(self)
        assert set(kwargs).issubset({'nargs', 'type', 'choices', 'required', 'metavar'})
        self.argparse_kw = kwargs.copy()
        all_options.append(self)
        if name in all_options_by_name:
            raise KeyError('trying to create Option with duplicate name %r; old is:\n%r' % (name, all_options_by_name[name]))
        all_options_by_name[name] = self
    def __repr__(self):
        return 'Option(name=%r, help=%r, value=%r, default=%r)' % (self.name, self.help, self.value, self.default)
    def set(self, value):
        self.value = value
        if self.on_set is not None:
            self.on_set(value)

class Expansion(object):
    def __init__(self, fmt):
        self.fmt = fmt
        self.deps = re.findall('\((.*?)\)', fmt)
    def __repr__(self):
        return 'Expansion(%r)' % (self.fmt,)

class InstallationDirsGroup(SettingsGroup):
    def __init__(self):
        section = OptSection('Fine tuning of the installation directories:')
        SettingsGroup.__init__(self)
        for name, optname, optdesc, default in [
            ('prefix', 'prefix', '', '/usr/local'),
            ('exec_prefix', 'exec-prefix', '', '(prefix)'),
            ('bin', 'bindir', '', '(exec_prefix)/bin'),
            ('sbin', 'sbindir', '', '(exec_prefix)/sbin'),
            ('libexec', 'libexecdir', '', '(exec_prefix)/libexec'),
            ('etc', 'sysconfdir', '', '(prefix)/etc'),
            ('var', 'localstatedir', '', '(prefix)/var'),
            ('lib', 'libdir', '', '(prefix)/lib'),
            ('include', 'includedir', '', '(prefix)/include'),
            ('datarootdir', 'datarootdir', '', '(prefix)/share'),
            ('share', 'datadir', '', '(datarootdir)'),
            ('locale', 'localedir', '', '(datarootdir)/locale'),
            ('man', 'mandir', '', '(datarootdir)/man'),
            ('doc', 'docdir', '', '(datarootdir)/doc/(..package_unix_name)'),
            ('html', 'htmldir', '', '(doc)'),
            ('pdf', 'pdfdir', '', '(doc)'),
        ]:
            self.add_setting_option(name, optname, optdesc, default, section=section)
        for ignored in ['sharedstatedir', 'oldincludedir', 'infodir', 'dvidir', 'psdir']:
            Option(ignored, 'Ignored autotools compatibility setting', '', None, section=section)

def _make_argparse(include_all):
    parser = argparse.ArgumentParser(add_help=False, usage='configure [OPTION]... [VAR=VALUE]...')
    parser.add_argument('--help', action='store_true', help='Show this help', dest='__help')
    parser.add_argument('--help-all', action='help', help='Show this help, including unused options')
    for sect in all_opt_sections:
        if not include_all and not any(opt.show for opt in sect.opts):
            continue
        ag = parser.add_argument_group(description=sect.desc)
        for opt in sect.opts:
            if not include_all and not opt.show: continue
            ag.add_argument('--' + opt.name,
                            action='store_true' if opt.bool else 'store',
                            dest=opt.name,
                            help=opt.help,
                            **opt.argparse_kw)
    return parser

def parse_args():
    default_opt_section.move_to_end()
    parser = _make_argparse(include_all=True)
    args, argv = parser.parse_known_args()
    if args.__help:
        parser = _make_argparse(include_all=False)
        parser.print_help()
        sys.exit(0)
    def do_env_arg(arg):
        m = re.match('([^ ]+)=(.*)', arg)
        if m:
            os.environ[m.group(1)] = m.group(2)
            return False
        return True
    argv = list(filter(do_env_arg, argv))
    if argv:
        parser = _make_argparse(include_all=False)
        parser.error('unrecognized arguments: %s' % (argv_to_shell(argv),))

    for opt in all_options:
        opt.set(getattr(args, opt.name))


all_options = []
all_options_by_name = {}
all_opt_sections = []
default_opt_section = OptSection('Uncategorized options:')

settings_root = SettingsGroup()
settings_root.idirs = InstallationDirsGroup()

# --
settings_root.package_unix_name = 'substitute'
settings_root.idirs.need('pdf')

parse_args()
print settings_root

