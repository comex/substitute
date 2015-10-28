#!/bin/bash
out=generated/manual-mach.inc.h
pat='/thread_[gs]et_state|vm_remap/'
(mig -user /dev/stdout -server /dev/null -header /dev/stdout /usr/include/mach/{thread_act,mach_vm}.defs |
     egrep -v '^(#ifndef|#define|#endif).*_user_' |
     egrep -v '#include "stdout"' |
     unifdef -D__MigTypeCheck |
     sed -E 's/(mach_msg|memcpy)\(/manual_\1(/g;
             s/^\)/, mach_port_t reply_port)/;
             s/_kernelrpc_//g;
             s/(Request|Reply)__/\1__manual_/g;
             s/^([a-z].*)?kern_return_t[[:blank:]]+([a-z])/\1kern_return_t manual_\2/;
             s/mig_external/static/;
             s/mig_get_reply_port\(\)/reply_port/g' |
     awk 'BEGIN { on = 1; }
          /^\/\* Routine / ||
          (/__MIG_check__Reply__/ && /^#[ie]/) { on = '"$pat"'; }
          on { print; }
          /typedef struct {/ { xon = 1; accum = ""; }
          xon { accum = accum $0 "\n"; }
          xon && /} / { if('"$pat"') print accum; xon = 0; }
          /#endif.*__AfterMigUserHeader/ { on = 1; }
          ' > $out) || rm -f $out
