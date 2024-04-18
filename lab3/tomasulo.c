
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "decode.def"
#include "dlite.h"
#include "host.h"
#include "instr.h"
#include "loader.h"
#include "machine.h"
#include "memory.h"
#include "misc.h"
#include "options.h"
#include "regs.h"
#include "sim.h"
#include "stats.h"
#include "syscall.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

/* ECE552 Assignment 3 - BEGIN CODE */
#define INSTR_QUEUE_SIZE 16  // 10 -> 16

#define RESERV_INT_SIZE 5  // 4 -> 5
#define RESERV_FP_SIZE 3  // 2 -> 3
#define FU_INT_SIZE 3  // 2 -> 3
#define FU_FP_SIZE 1

#define FU_INT_LATENCY 5  // 4 -> 5
#define FU_FP_LATENCY 7  // 9 -> 7
/* ECE552 Assignment 3 - END CODE */

/* IDENTIFYING INSTRUCTIONS */

// unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || MD_OP_FLAGS(op) & F_UNCOND)

// conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

// floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

// integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

// load instruction
#define IS_LOAD(op) (MD_OP_FLAGS(op) & F_LOAD)

// store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

// trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP)

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

// prints info about an instruction
#define PRINT_INST(out, instr, str, cycle)      \
    myfprintf(out, "%d: %s", cycle, str);       \
    md_print_insn(instr->inst, instr->pc, out); \
    myfprintf(stdout, "(%d)\n", instr->index);

#define PRINT_REG(out, reg, str, instr)         \
    myfprintf(out, "reg#%d %s ", reg, str);     \
    md_print_insn(instr->inst, instr->pc, out); \
    myfprintf(stdout, "(%d)\n", instr->index);

/* VARIABLES */

// instruction queue for tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
// number of instructions in the instruction queue
static int instr_queue_size = 0;

// The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

// the index of the last instruction fetched
static int fetch_index = 0;  // haven't fetched any instruction yet because starting index is 1

/* ECE552 Assignment 3 - BEGIN CODE */
// common data bus
static instruction_t* commonDataBus    = NULL;
static instruction_t* last_cdb         = NULL;  // track the address of the last instruction on the CDB
static instruction_t* last_cdb_address = NULL;  // track the address of the last instruction on the CDB

enum INT_OR_FP { INT, FP };  // used to access `reserv_l`, `reserv_size_l`, `fu_l`, `fu_size_l`

/* RESERVATION STATIONS */
typedef struct {
    instruction_t* insn;
} reservation_station_entry_t;  // use this data structure because the reservation station is freed when the execution finished. It's easier to free the reservation station entry from the functional unit entry if we use a separate `insn`

static reservation_station_entry_t* reservINT[RESERV_INT_SIZE] = {NULL};
static reservation_station_entry_t* reservFP[RESERV_FP_SIZE]   = {NULL};
static reservation_station_entry_t** reserv_l[2]               = {reservINT, reservFP};
static int reserv_size_l[2]                                    = {RESERV_INT_SIZE, RESERV_FP_SIZE};

/* FUNCTIONAL UNITS */
typedef struct {
    reservation_station_entry_t* rsv_station;
    int latency;  // associated the latency information with the `functional_unit_entry_t`
    int complete_cycle;
} functional_unit_entry_t;  // carry on the same instruction from `reservation_station_entry_t`, but use a different name to distinguish

//  Provided version:
//  static instruction_t* fuINT[FU_INT_SIZE];
//  static instruction_t* fuFP[FU_FP_SIZE];

// Modified version:
static functional_unit_entry_t* fuINT[FU_INT_SIZE] = {NULL};
static functional_unit_entry_t* fuFP[FU_FP_SIZE]   = {NULL};
static functional_unit_entry_t** fu_l[2]           = {fuINT, fuFP};
static int fu_size_l[2]                            = {FU_INT_SIZE, FU_FP_SIZE};
/* ECE552 Assignment 3 - END CODE */

/**
 * Description:
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {
    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    //$ Dispatch Stage
    for (int i = 0; i < INSTR_QUEUE_SIZE; i++) {
        if (instr_queue[i] != NULL) {
            return false;
        }
    }

    //$ Issue Stage
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < reserv_size_l[j]; i++) {
            if (reserv_l[j][i]->insn != NULL) {
                return false;
            }
        }
    }

    //$ Execuation Stage
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < fu_size_l[j]; i++) {
            if (fu_l[j][i]->rsv_station != NULL) {
                return false;
            }
        }
    }

    //$ CDB Stage
    if (commonDataBus != NULL) {
        return false;
    }
    /* ECE552 Assignment 3 - END CODE */
    return true;  // ECE552: you can change this as needed; we've added this so the code provided to you compiles
}

//# CDB
/* ECE552 Assignment 3 - BEGIN CODE */
void update_map_table_w_last_cdb() {
    /** Update `map_table` with `last_cdb` instruction. Mark the output register of instruction `cdb` as `NULL` (ready to use)
     */
    if (last_cdb->index == -1) {  // invalid cdb
        return;
    }
    for (int i = 0; i < 3; i++) {
        if (map_table[last_cdb->r_out[i]] != DNA && map_table[last_cdb->r_out[i]] == last_cdb_address) {
            map_table[last_cdb->r_out[i]] = NULL;
        }
    }
}

void update_reserv_station_w_last_cdb_addr(int int_or_fp) {
    /** Update insns in reservation station that is waiting for `cdb` for its input register. Mark them as ready
     */
    if (last_cdb->index == -1) {  // invalid cdb
        return;
    }
    reservation_station_entry_t** reserv_station = reserv_l[int_or_fp];
    int reserv_station_size                      = reserv_size_l[int_or_fp];
    for (int i = 0; i < reserv_station_size; i++) {
        if (reserv_station[i]->insn == NULL) {
            continue;
        }
        for (int j = 0; j < 3; j++) {
            if (reserv_station[i]->insn->Q[j] == last_cdb_address) {
                reserv_station[i]->insn->Q[j] = NULL;
            }
        }
    }
}

void free_reserv_and_fu_resource_at_cdb(instruction_t* cdb) {
    /** Free the resource of `cdb` instruction in reservation station and functional unit
     */
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < fu_size_l[j]; i++) {
            if (fu_l[j][i]->rsv_station && fu_l[j][i]->rsv_station->insn == cdb) {
                fu_l[j][i]->rsv_station->insn = NULL;  // order matters
                fu_l[j][i]->rsv_station       = NULL;
            }
        }
    }
}
/* ECE552 Assignment 3 - END CODE */

/**
 * Description:
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {
    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    if (commonDataBus == NULL) {
        last_cdb->index = -1;  // mark it as invalid, i.e. no cdb value for last iteration
        return;
    }
    commonDataBus->tom_cdb_cycle = current_cycle;
    free_reserv_and_fu_resource_at_cdb(commonDataBus);

    last_cdb_address = commonDataBus;  // copy the address of `commonDataBus` to `last_cdb_address`
    *last_cdb        = *commonDataBus;  // copy the content of `commonDataBus` to `last_cdb`
    commonDataBus    = NULL;
    /* ECE552 Assignment 3 - END CODE */
}

//# Execute
functional_unit_entry_t* find_completed_fu_with_min_index(int current_cycle) {
    /** Find the functional unit that has completed execution. If there are multiple candidates, choose the one with minimum index from both INT and FP functional units
     */
    int min_fu_type_idx = INT;
    int min_fu_idx      = 0;
    int min_idx         = INT_MAX;
    for (int i = 0; i < 2; i++) {
        functional_unit_entry_t** fu = fu_l[i];
        int fu_size                  = fu_size_l[i];
        for (int j = 0; j < fu_size; j++) {
            if (fu[j]->rsv_station && fu[j]->complete_cycle <= current_cycle) {
                if (IS_STORE(fu[j]->rsv_station->insn->op)) {  // Q: store do not write to CDB, thus its resource can be freed in this stage? If so, it can complete with other instruction in the same cycle
                    if (fu[j]->complete_cycle != current_cycle) {  // because function `execute_To_CDB` is called before function `issue_To_execute`. If we release the resource at the end of this cycle, `issue_To_execute` will use it directly in the same cycle as the last cycle of execution, which is wrong
                        fu[j]->rsv_station->insn = NULL;
                        fu[j]->rsv_station       = NULL;
                    }
                    // else wait until next cycle to free the resource
                } else {
                    if (fu[j]->rsv_station->insn->index < min_idx) {
                        min_fu_type_idx = i;
                        min_fu_idx      = j;
                        min_idx         = fu[j]->rsv_station->insn->index;
                    }
                }
            }
        }
    }
    if (min_idx == INT_MAX) {
        return NULL;
    }
    instruction_t* broadcast_insn = fu_l[min_fu_type_idx][min_fu_idx]->rsv_station->insn;
    return broadcast_insn;
}

/**
 * Description:
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {
    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    instruction_t* broadcast_insn = find_completed_fu_with_min_index(current_cycle);
    if (broadcast_insn == NULL) {
        return;
    }
    commonDataBus = broadcast_insn;
    /* ECE552 Assignment 3 - END CODE */
}

/* ECE552 Assignment 3 - BEGIN CODE */
//# Issue
bool has_raw_dependences(instruction_t* insn) {
    /** Check if `insn` has RAW dependences
     */
    for (int i = 0; i < 3; i++) {
        if (insn->Q[i] != NULL) {
            return true;
        }
    }
    return false;
}

reservation_station_entry_t* find_ready_reserv_station_entry_w_min_idx(int int_or_fp) {
    /** Get the ready instruction with minimum index
     */
    reservation_station_entry_t** reserv_station = reserv_l[int_or_fp];
    int reserv_station_size                      = reserv_size_l[int_or_fp];

    int min_reserv_idx = 0;
    int min_idx        = INT_MAX;
    for (int i = 0; i < reserv_station_size; i++) {
        if (reserv_station[i]->insn && !has_raw_dependences(reserv_station[i]->insn)) {
            if (reserv_station[i]->insn->tom_execute_cycle != 0) {
                continue;  // ignore instruction that has already been issued to execution; these instruction are still in the reservation station
            }
            if (reserv_station[i]->insn->index < min_idx) {
                min_reserv_idx = i;
                min_idx        = reserv_station[i]->insn->index;
            }
        }
    }
    if (min_idx == INT_MAX) {
        return NULL;
    }
    reservation_station_entry_t* earliest_ready_reserv_station = reserv_station[min_reserv_idx];
    return earliest_ready_reserv_station;
}

functional_unit_entry_t* find_free_fu(int int_or_fp) {
    /** Find a free functional unit
     */
    functional_unit_entry_t** fu = fu_l[int_or_fp];
    int fu_size                  = fu_size_l[int_or_fp];
    for (int i = 0; i < fu_size; i++) {
        if (fu[i]->rsv_station == NULL) {
            return fu[i];
        }
    }
    return NULL;
}

void copy_ready_insn_from_reserv_station_to_fu(int int_or_fp, int current_cycle) {
    /** Copy ready instruction from reservation station to functional unit. Not clear reservation station entry because it's required to be cleared after execute stage (in CDB stage)
     */
    while (true) {
        //$ Find a free functional unit
        functional_unit_entry_t* free_fu = find_free_fu(int_or_fp);
        if (free_fu == NULL) {
            return;
        }

        //$ Find the ready instruction with minimum index
        reservation_station_entry_t* ready_reserv = find_ready_reserv_station_entry_w_min_idx(int_or_fp);
        if (ready_reserv == NULL) {
            return;
        }

        //$ Copy the ready instruction from reservation station to functional unit
        free_fu->rsv_station                  = ready_reserv;
        free_fu->complete_cycle               = current_cycle + free_fu->latency - 1;
        ready_reserv->insn->tom_execute_cycle = current_cycle;
    }
}
/* ECE552 Assignment 3 - END CODE */

/**
 * Description:
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {
    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    copy_ready_insn_from_reserv_station_to_fu(INT, current_cycle);
    copy_ready_insn_from_reserv_station_to_fu(FP, current_cycle);
    /* ECE552 Assignment 3 - END CODE */
}

//# Dispatch
/* ECE552 Assignment 3 - BEGIN CODE */
void pop_insn_queue() {
    /** Pop first instruction from the insn queue, and shift all others forward by one position
     */
    for (int i = 0; i < INSTR_QUEUE_SIZE - 1; i++) {
        instr_queue[i] = instr_queue[i + 1];
    }
    instr_queue_size--;
    instr_queue[INSTR_QUEUE_SIZE - 1] = NULL;
}

instruction_t* update_insn_Q_and_map_table(instruction_t* insn) {
    //* Must update `insn` input first, otherwise `insn` that has same input and output register will be have circular dependency
    for (int i = 0; i < 3; i++) {
        if (insn->r_in[i] != DNA) {
            insn->Q[i] = map_table[insn->r_in[i]];
        }
    }

    // Update `insn` output
    for (int i = 0; i < 2; i++) {
        if (insn->r_out[i] != DNA) {
            map_table[insn->r_out[i]] = insn;
        }
    }
    return insn;
}

reservation_station_entry_t* get_free_reserv_station_entry(int int_or_fp, reservation_station_entry_t* rsv_station_entry) {
    /**
     * Get a free reservation station entry for `rsv_station_entry` instruction
     */
    reservation_station_entry_t** reserv_station = reserv_l[int_or_fp];
    int reserv_station_size                      = reserv_size_l[int_or_fp];
    for (int i = 0; i < reserv_station_size; i++) {
        if (reserv_station[i]->insn == NULL) {
            reserv_station[i]->insn = rsv_station_entry;
            return reserv_station[i];
        }
    }
    return NULL;
}
/* ECE552 Assignment 3 - END CODE */

/**
 * Description:
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {
    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    if (instr_queue_size == 0) {  // corner: `instr_queue_size == 0 ` in first and last few iterations
        return;
    }

    instruction_t* frontmost_instr = instr_queue[0];

    // Branches are not dispatched
    if (IS_COND_CTRL(frontmost_instr->op) || IS_UNCOND_CTRL(frontmost_instr->op)) {  // corner: Conditional and unconditional branches are not dispatched
        pop_insn_queue();
        return;
    }

    // Otherwise, instruction are dispatched only if there is a reservation station entry available of its `op` type
    reservation_station_entry_t* rsv_station_entry = USES_INT_FU(frontmost_instr->op) ? get_free_reserv_station_entry(INT, (reservation_station_entry_t*)frontmost_instr) : get_free_reserv_station_entry(FP, (reservation_station_entry_t*)frontmost_instr);
    if (rsv_station_entry == NULL) {  // no reservation station entry available
        return;
    }
    rsv_station_entry->insn->tom_issue_cycle = current_cycle;
    pop_insn_queue();
    update_insn_Q_and_map_table(rsv_station_entry->insn);
    /* ECE552 Assignment 3 - END CODE */
}

//# Fetch
/**
 * Description:
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace) {
    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    if (instr_queue_size >= INSTR_QUEUE_SIZE || fetch_index == sim_num_insn) {  // corner: `instr_queue_size` is full or we have fetch all instructions
        return;
    }

    fetch_index += 1;

    // Trap instructions are ignored, and does not count toward cycle count
    while (IS_TRAP(get_instr(trace, fetch_index)->op)) {
        fetch_index += 1;
    };

    instr_queue[instr_queue_size++] = get_instr(trace, fetch_index);
    /* ECE552 Assignment 3 - END CODE */
}

/**
 * Description:
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {
    fetch(trace);

    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    if (instr_queue[0] == NULL) {  // corner: `instr_queue[0] == NULL` last few iterations
        return;
    }
    if (instr_queue[instr_queue_size - 1]->tom_dispatch_cycle == 0) {  // corner: if `instr_queue` is full, then `instr_queue[instr_queue_size - 1]` will be the instruction that has already been dispatched. We don't want to dispatch in this case
        instr_queue[instr_queue_size - 1]->tom_dispatch_cycle = current_cycle;
    }
    /* ECE552 Assignment 3 - END CODE */
}

//# runTomasulo
/**
 * Description:
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace) {
    // initialize instruction queue
    int i;
    for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
        instr_queue[i] = NULL;
    }

    /* ECE552 Assignment 3 - BEGIN CODE */
    //$ Initialize reservation stations
    for (i = 0; i < RESERV_INT_SIZE; i++) {
        reservINT[i]       = (reservation_station_entry_t*)malloc(sizeof(reservation_station_entry_t));
        reservINT[i]->insn = NULL;
    }

    for (i = 0; i < RESERV_FP_SIZE; i++) {
        reservFP[i]       = (reservation_station_entry_t*)malloc(sizeof(reservation_station_entry_t));
        reservFP[i]->insn = NULL;
    }

    // initialize functional units
    for (i = 0; i < FU_INT_SIZE; i++) {
        fuINT[i]                 = (functional_unit_entry_t*)malloc(sizeof(functional_unit_entry_t));
        fuINT[i]->rsv_station    = NULL;
        fuINT[i]->latency        = FU_INT_LATENCY;
        fuINT[i]->complete_cycle = 0;
    }

    for (i = 0; i < FU_FP_SIZE; i++) {
        fuFP[i]                  = (functional_unit_entry_t*)malloc(sizeof(functional_unit_entry_t));
        fuFP[i]->rsv_station     = NULL;
        fuFP[i]->latency         = FU_FP_LATENCY;
        fuINT[i]->complete_cycle = 0;
    }

    last_cdb        = (instruction_t*)malloc(sizeof(instruction_t));
    last_cdb->index = -1;  // use -1 to mark it as invalid as initial value
    /* ECE552 Assignment 3 - END CODE */

    //$ Initialize `map_table` to no producers
    int reg;
    for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
        map_table[reg] = NULL;
    }

    //$ Main loop
    int cycle = 1;
    while (true) {
        /* ECE552: YOUR CODE GOES HERE */
        /* ECE552 Assignment 3 - BEGIN CODE */
        //* Each stage will affect the following stage, so we need to do it in the reverse order
        CDB_To_retire(cycle);
        execute_To_CDB(cycle);
        issue_To_execute(cycle);
        dispatch_To_issue(cycle);
        fetch_To_dispatch(trace, cycle);

        //* Update the RAW harzard (only hazard in Tomasulo algorithm) in reservation station and functional unit after all stages are done. Because according to the lab handout, if insn A is waiting for a value from insn B, if insn B is in Writeback on cycle 9, then A can enter Execute on cycle 10
        update_map_table_w_last_cdb();
        update_reserv_station_w_last_cdb_addr(INT);
        update_reserv_station_w_last_cdb_addr(FP);
        /* ECE552 Assignment 3 - END CODE */

        cycle++;

        if (is_simulation_done(sim_num_insn)) {
            break;
        }
    }

    return cycle;
}

// gcc: 1681443
// go: 1695064
// compress: 1851550