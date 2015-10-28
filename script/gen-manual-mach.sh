#!/bin/bash
out=generated/manual-mach.inc.h
(mig -user /dev/stdout -server /dev/null -header /dev/stdout /usr/include/mach/{thread_act,mach_vm}.defs |
     egrep -v '^(#ifndef|#define|#endif).*_user_' |
     egrep -v '#include "stdout"' |
     unifdef -D__MigTypeCheck |
     sed -E 's/(mach_msg|memcpy)\(/manual_\1(/g;
             s/^\)/, mach_port_t reply_port)/;
             s/_kernelrpc_//;
             s/^([a-z].*)?kern_return_t[[:blank:]]+([a-z])/\1kern_return_t manual_\2/;
             s/mig_external/static/;
             s/__defined/_manual__defined/g;
             s/mig_get_reply_port\(\)/reply_port/g' |
     awk 'BEGIN { on = 1; }
          /^\/\* Routine / ||
          (/__MIG_check__Reply__/ && /^#[ie]/) { on = /thread_[gs]et_state|vm_remap/; }
          on { print; }
          /#endif.*__AfterMigUserHeader/ { on = 1; }
          ' > $out) || rm -f $out
