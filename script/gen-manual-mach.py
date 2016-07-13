import subprocess, re
# i wish there was a good simple tooling/ast library that could do this more cleanly
# maybe libTooling, but instability and dependencies...

desired = ['thread_get_state', 'thread_set_state', 'mach_vm_remap']

def pipe(cmd, stdin='', expected_returncode=0):
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    stdout, _ = p.communicate(stdin)
    if p.returncode != expected_returncode:
        raise Exception('process failed: %s => %d (not %d)' % (cmd, p.returncode, expected_returncode))
    return stdout
text = pipe('mig -user /dev/stdout -server /dev/null -header /dev/stdout /usr/include/mach/{thread_act,mach_vm}.defs')
text = pipe('unifdef -D__MigTypeCheck -Umig_external -UUseStaticTemplates', text, expected_returncode=1)
# remove voucher/etc code
text = re.sub('/\* BEGIN (VOUCHER|MIG_STRNCPY_ZEROFILL) CODE \*/.*?/\* END \\1 CODE \*/',
              '', text, flags=re.S)
# and union
text = re.sub('\/\* union of all requests.*?#endif \/\* !__RequestUnion[^\n]*',
              '', text, flags=re.S)
# simplify function names
text = text.replace('_kernelrpc_', '')
# extern->static
text = re.sub('(extern|mig_internal|mig_external)', 'static', text)
# add extra argument reply_port
text = re.sub('^\)', ', mach_port_t reply_port)', text, flags=re.M)
text = text.replace('mig_get_reply_port()', 'reply_port')
# don't use stdlib functions
text = re.sub(r'\b(mach_msg|memcpy)\b', r'manual_\1', text)
# change symbols to avoid collision: types
text = re.sub('(Request|Reply)__', r'\1__manual_', text)
# and functions
text = re.sub('^([a-z].+|)kern_return_t\s+([a-z])', r'\1kern_return_t manual_\2', text, flags=re.M)

text = re.sub('^#(if|ifdef|define|endif)\s.*_check__Reply.*', '', text, flags=re.M)

# filter out unused parts
patterns = '(^/\* Routine [^ ]* \*/.*?(?:^}|reply_port\);)|' \
            '#if __MIG_check__Reply.*?#endif /\* __MIG_check__Reply[^\n]*|' \
            '#ifdef\s+__MigPackStructs.*?typedef struct {.*?(?:Request|Reply)__.*?#endif|' \
            '#ifndef\s+LimitCheck.*?msgh_local_port)'
bits = re.split(patterns, text, flags=re.M | re.S)
out = ''
for bit in bits:
    #print bit; print ';;;'
    if 'subsystem_to_name_map' in bit:
        continue
    if 'LimitCheck' in bit or any(d in bit for d in desired):
        out += bit + '\n'
#print out;die

open('generated/manual-mach.inc.h', 'w').write(out)
