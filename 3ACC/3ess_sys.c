/* 3ess.c: AT&T No. 3 Electronic Switching System, system-specific logic and declarations.
 * Copyright 2020, Astrid Smith
 */

#include <sim_defs.h>

char sim_name[] = "AT&T No. 3 Electronic Switching System";

extern DEVICE cpu_dev;
extern REG cpu_reg[];

// xxx DEVICE structure
DEVICE *sim_devices[] = {
    &cpu_dev,
    NULL
};

int32 sim_emax = 4; // xxx

t_stat
fprint_sym(FILE *ofile, t_addr addr, t_value *val, UNIT *uptr, int32 swtch)
{
        // xxx
        return SCPE_ARG;
}

t_stat
parse_sym(const char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 swtch)
{
        // xxx
        return SCPE_ARG;
}

t_stat
sim_load(FILE *fptr, const char* buf, const char* fnam, t_bool flag)
{
        // xxx
        return SCPE_ARG;
}

const char* sim_stop_messages[] = {
        NULL
};
