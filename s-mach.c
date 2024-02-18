/*
 * System dependent file for Mach
 */

/* 
 * Melinda Shore, mt Xinu, 3/21/90 
 */

#include <mach.h>

#define NSTATES 3

extern char *xmalloc(/* int nbytes */);
extern kern_return_t host_info();
extern task_t task_self();
extern kern_return_t slot_info();
int *cpu_ids;
machine_slot_data_t **last_sinfo, **cur_sinfo, **tmp_sinfo;
task_t self;

/* Called by -version */
void
version()
{
	printf("Mach: numdisks=0\n");
}

/* 
 * Called at the beginning to enquire how many bars are needed,
 * and to determine which cpu slots are filled
 */

int
num_bars()
{
    machine_info_data_t minfo;
    machine_slot_data_t sinfo;
    int i, j = 0, result, ncpus;

    ncpus = 0;
    self = task_self();
    if ((result = host_info(self, &minfo)) != KERN_SUCCESS)  {
        mach_error("xcpustate", result);
        exit(1);
    }
    cpu_ids = (int *)xmalloc(minfo.max_cpus * sizeof (int));
    for (i = 0 ; i < minfo.max_cpus ; i++)  {
        if ((result = slot_info(self, i, &sinfo)) != KERN_SUCCESS)
            mach_error("xcpustate", result);
        else
            if (sinfo.is_cpu && sinfo.running)  {
                ncpus++;
                cpu_ids[j++] = i;
            }
    }
    return ncpus;
}

/*
 * Indicates how many levels each bar has.  For most machines, each bar will
 * have the same stuff.  But one can, for instance, display memory use on one
 * bar, processor levels on others, etc.
 */
void
bar_items(nbars, items)
int nbars;
int items[];    /* nbars items in this */
{
    int i;

    for(i = 0; i < nbars; i++)
        items[i] = NSTATES;
}

/* Called after num_bars to ask for the bar names */
/* ARGSUSED */
char **
label_bars(nbars)
{
    char **names;
    int i;

    names = (char **)xmalloc(nbars * sizeof (char *));
    for (i = 0 ; i < nbars ; i++)  {
        names[i] = xmalloc(10);
        (void) sprintf(names[i], "Slot %d", cpu_ids[i]);
    }
    return names;
}

/* 
 *  Called after the bars are created to perform any machine dependent
 *  initializations.
 */
/* ARGSUSED */
void
init_bars(nbars)
int nbars;
{
    int i;
    kern_return_t result;

    cur_sinfo = (machine_slot_data_t **)xmalloc(nbars *
                                               sizeof (machine_slot_data_t *));
    last_sinfo = (machine_slot_data_t **)xmalloc(nbars *
                                               sizeof (machine_slot_data_t *));
    for (i = 0 ; i < nbars ; i++)  {
        cur_sinfo[i] = (machine_slot_data_t *)xmalloc(sizeof
                                                   (machine_slot_data_t));
        last_sinfo[i] = (machine_slot_data_t *)xmalloc(sizeof
                                                   (machine_slot_data_t));
        if ((result = slot_info(self, cpu_ids[i], cur_sinfo[i])) !=
            KERN_SUCCESS)  {
            mach_error("xcpustate", result);
            exit(1);
        }
    }            
}

/* 
 *  This procedure gets called every interval to compute and display the
 *  bars. It should call draw_bar() with the bar number, the array of
 *  integer values to display in the bar, and the number of values in
 *  the array.
 */
/* ARGSUSED */
void
display_bars(nbars)
{
    int states[CPU_STATE_MAX];
    int nstates;
    int i;
    extern void draw_bar(/*int bar_num, int *states, int num_states*/);
    kern_return_t result;
    
    for (i = 0 ; i < nbars ; i++)  {
        if ((result = slot_info(self, cpu_ids[i], cur_sinfo[i])) != 
            KERN_SUCCESS)
            mach_error("xcpustate", result);
#define delta(cpustate) ((int) (cur_sinfo[i]->cpu_ticks[(cpustate)] - \
    last_sinfo[i]->cpu_ticks[(cpustate)]))

        nstates = 0;
        states[nstates++] = delta(CPU_STATE_USER);
        states[nstates++] = delta(CPU_STATE_SYSTEM);
        states[nstates++] = delta(CPU_STATE_IDLE);
        draw_bar(i, states, nstates);
    }
    tmp_sinfo = last_sinfo;
    last_sinfo = cur_sinfo;
    cur_sinfo = tmp_sinfo;
}
