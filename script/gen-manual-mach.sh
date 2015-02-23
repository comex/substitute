#!/bin/bash
out=generated/manual-mach.inc.h
(mig -user /dev/stdout -server /dev/null -header /dev/null /usr/include/mach/{thread_act,vm_map}.defs |
     unifdef -U__MigTypeCheck |
     grep -v '#define USING_VOUCHERS' |
     sed -E 's/(mach_msg|memcpy)\(/manual_\1(/g;
             s/^\)/, mach_port_t reply_port)/;
             s/mig_external kern_return_t /static kern_return_t manual_/;
             s/_kernelrpc_//;
             s/mig_get_reply_port\(\)/reply_port/g' |
     awk 'BEGIN { on = 1 }
          /^\/\* Routine / { on = /thread_[gs]et_state|vm_remap/; }
          on { print }' > $out) || rm -f $out
