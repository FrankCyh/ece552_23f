/* sim-safe.c - sample functional simulator implementation */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved.
 *
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 *
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 *
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 *
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 *
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 *
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 *
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 *
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 *
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */


#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dlite.h"
#include "host.h"
#include "loader.h"
#include "machine.h"
#include "memory.h"
#include "misc.h"
#include "options.h"
#include "regs.h"
#include "sim.h"
#include "stats.h"
#include "syscall.h"


/* ECE552 Assignment 1 - STATS COUNTERS - BEGIN */
static counter_t sim_num_RAW_hazard_q1;
static counter_t sim_num_RAW_hazard_q2;
/* ECE552 Assignment 1 - STATS COUNTERS - END */

/* ECE552 Assignment 1 - BEGIN CODE*/
//# Q1
static counter_t reg_ready_q1[MD_TOTAL_REGS];  // As processor in q1 and q2 have different architecture, they will need two separate `reg_ready` list
static counter_t sim_num_raw_q1_1_cycle = 0;
static counter_t sim_num_raw_q1_2_cycle = 0;
static counter_t timestamp_q1           = 1;  // If there are stalls, the timestamp will not equal `sim_num_insn`, but equal `sim_num_insn` + num_of_stalls.
// As processor in q1 and q2 have different architecture, they will need two separate timestamp. We can't rely on the single `sim_num_insn` as in lab0
// Initialize to 1 to keep consistent with `sim_num_insn` if there is no stall

//# Q2
static counter_t reg_ready_q2[MD_TOTAL_REGS];
static counter_t sim_num_raw_q2_1_cycle = 0;
static counter_t sim_num_raw_q2_2_cycle = 0;
static counter_t timestamp_q2           = 1;

/* ECE552 Assignment 1 - END CODE*/

/*
 * This file implements a functional simulator.  This functional simulator is
 * the simplest, most user-friendly simulator in the simplescalar tool set.
 * Unlike sim-fast, this functional simulator checks for all instruction
 * errors, and the implementation is crafted for clarity rather than speed.
 */

/* simulated registers */
static struct regs_t regs;

/* simulated memory */
static struct mem_t *mem = NULL;

/* track number of refs */
static counter_t sim_num_refs = 0;

/* maximum number of inst's to execute */
static unsigned int max_insts;

/* register simulator-specific options */
void sim_reg_options(struct opt_odb_t *odb) {
    opt_reg_header(odb,
                   "sim-safe: This simulator implements a functional simulator.  This\n"
                   "functional simulator is the simplest, most user-friendly simulator in the\n"
                   "simplescalar tool set.  Unlike sim-fast, this functional simulator checks\n"
                   "for all instruction errors, and the implementation is crafted for clarity\n"
                   "rather than speed.\n");

    /* instruction limit */
    opt_reg_uint(odb, "-max:inst", "maximum number of inst's to execute", &max_insts, /* default */ 0,
                 /* print */ TRUE, /* format */ NULL);
}

/* check simulator-specific option values */
void sim_check_options(struct opt_odb_t *odb, int argc, char **argv) { /* nada */ }

/* register simulator-specific statistics */
void sim_reg_stats(struct stat_sdb_t *sdb) {
    stat_reg_counter(sdb, "sim_num_insn", "total number of instructions executed", &sim_num_insn, sim_num_insn, NULL);
    stat_reg_counter(sdb, "sim_num_refs", "total number of loads and stores executed", &sim_num_refs, 0, NULL);
    stat_reg_int(sdb, "sim_elapsed_time", "total simulation time in seconds", &sim_elapsed_time, 0, NULL);
    stat_reg_formula(sdb, "sim_inst_rate", "simulation speed (in insts/sec)", "sim_num_insn / sim_elapsed_time", NULL);

    /* ECE552 Assignment 1 - BEGIN CODE*/

    //# Variable created to collect intermediate statistics
    //$ Q1
    stat_reg_counter(sdb, "sim_num_raw_q1_1_cycle", "total number of 1 cycles stall (independent instruction between load and instruction dependent on the result of load) resulting from RAW hazard in processor 1", &sim_num_raw_q1_1_cycle, sim_num_raw_q1_1_cycle, NULL);
    stat_reg_counter(sdb, "sim_num_raw_q1_2_cycle", "total number of 2 cycles stall (load followed directly by instruction dependent on the result of load) resulting from RAW hazard in processor 1", &sim_num_raw_q1_2_cycle, sim_num_raw_q1_2_cycle, NULL);
    stat_reg_counter(sdb, "timestamp_q1", "final timestamp for processor 1", &timestamp_q1, timestamp_q1, NULL);

    //$ Q2
    stat_reg_counter(sdb, "sim_num_raw_q2_1_cycle", "total number of 1 cycles stall (independent instruction between load and instruction dependent on the result of load) resulting from RAW hazard in processor 1", &sim_num_raw_q2_1_cycle, sim_num_raw_q2_1_cycle, NULL);
    stat_reg_counter(sdb, "sim_num_raw_q2_2_cycle", "total number of 2 cycles stall (load followed directly by instruction dependent on the result of load) resulting from RAW hazard in processor 1", &sim_num_raw_q2_2_cycle, sim_num_raw_q2_2_cycle, NULL);
    stat_reg_counter(sdb, "timestamp_q2", "final timestamp for processor 2", &timestamp_q2, timestamp_q2, NULL);

    //# Required
    stat_reg_counter(sdb, "sim_num_RAW_hazard_q1", "total number of RAW hazards (q1)", &sim_num_RAW_hazard_q1, sim_num_RAW_hazard_q1, NULL);

    stat_reg_counter(sdb, "sim_num_RAW_hazard_q2", "total number of RAW hazards (q2)", &sim_num_RAW_hazard_q2, sim_num_RAW_hazard_q2, NULL);

    stat_reg_formula(sdb, "CPI_from_RAW_hazard_q1", "CPI from RAW hazard (q1)", "1 + (sim_num_raw_q1_1_cycle + 2 * sim_num_raw_q1_2_cycle) / sim_num_insn" /* ECE552 - MUST ADD YOUR FORMULA */, NULL);

    stat_reg_formula(sdb, "CPI_from_RAW_hazard_q2", "CPI from RAW hazard (q2)", "1 + (sim_num_raw_q2_1_cycle + 2 * sim_num_raw_q2_2_cycle) / sim_num_insn" /* ECE552 - MUST ADD YOUR FORMULA */, NULL);

    /* ECE552 Assignment 1 - END CODE*/

    ld_reg_stats(sdb);
    mem_reg_stats(mem, sdb);
}

/* initialize the simulator */
void sim_init(void) {
    sim_num_refs = 0;

    /* allocate and initialize register file */
    regs_init(&regs);

    /* allocate and initialize memory space */
    mem = mem_create("mem");
    mem_init(mem);
}

/* load program into simulated state */
void sim_load_prog(char *fname, /* program to load */
                   int argc, char **argv, /* program arguments */
                   char **envp) /* program environment */
{
    /* load program text and data, set up environment, memory, and regs */
    ld_load_prog(fname, argc, argv, envp, &regs, mem, TRUE);

    /* initialize the DLite debugger */
    dlite_init(md_reg_obj, dlite_mem_obj, dlite_mstate_obj);
}

/* print simulator-specific configuration information */
void sim_aux_config(FILE *stream) /* output stream */
{
    /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
void sim_aux_stats(FILE *stream) /* output stream */
{
    /* nada */
}

/* un-initialize simulator-specific state */
void sim_uninit(void) { /* nada */ }


/*
 * configure the execution engine
 */

/*
 * precise architected register accessors
 */

/* next program counter */
#define SET_NPC(EXPR) (regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC (regs.regs_PC)

/* general purpose registers */
#define GPR(N) (regs.regs_R[N])
#define SET_GPR(N, EXPR) (regs.regs_R[N] = (EXPR))

#define DNA (0)

#if defined(TARGET_PISA)

/* general register dependence decoders */
#define DGPR(N) (N)
#define DGPR_D(N) ((N) & ~1)

/* floating point register dependence decoders */
#define DFPR_L(N) (((N) + 32) & ~1)
#define DFPR_F(N) (((N) + 32) & ~1)
#define DFPR_D(N) (((N) + 32) & ~1)

/* miscellaneous register dependence decoders */
#define DHI (0 + 32 + 32)
#define DLO (1 + 32 + 32)
#define DFCC (2 + 32 + 32)
#define DTMP (3 + 32 + 32)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_L(N) (regs.regs_F.l[(N)])
#define SET_FPR_L(N, EXPR) (regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N) (regs.regs_F.f[(N)])
#define SET_FPR_F(N, EXPR) (regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N) (regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N, EXPR) (regs.regs_F.d[(N) >> 1] = (EXPR))

/* miscellaneous register accessors */
#define SET_HI(EXPR) (regs.regs_C.hi = (EXPR))
#define HI (regs.regs_C.hi)
#define SET_LO(EXPR) (regs.regs_C.lo = (EXPR))
#define LO (regs.regs_C.lo)
#define FCC (regs.regs_C.fcc)
#define SET_FCC(EXPR) (regs.regs_C.fcc = (EXPR))

#elif defined(TARGET_ALPHA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_Q(N) (regs.regs_F.q[N])
#define SET_FPR_Q(N, EXPR) (regs.regs_F.q[N] = (EXPR))
#define FPR(N) (regs.regs_F.d[(N)])
#define SET_FPR(N, EXPR) (regs.regs_F.d[(N)] = (EXPR))

/* miscellaneous register accessors */
#define FPCR (regs.regs_C.fpcr)
#define SET_FPCR(EXPR) (regs.regs_C.fpcr = (EXPR))
#define UNIQ (regs.regs_C.uniq)
#define SET_UNIQ(EXPR) (regs.regs_C.uniq = (EXPR))

#else
#error No ISA target defined...
#endif

/* precise architected memory state accessor macros */
#define READ_BYTE(SRC, FAULT) ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_BYTE(mem, addr))
#define READ_HALF(SRC, FAULT) ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_HALF(mem, addr))
#define READ_WORD(SRC, FAULT) ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_WORD(mem, addr))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT) ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_QWORD(mem, addr))
#endif /* HOST_HAS_QWORD */

#define WRITE_BYTE(SRC, DST, FAULT) ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_BYTE(mem, addr, (SRC)))
#define WRITE_HALF(SRC, DST, FAULT) ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_HALF(mem, addr, (SRC)))
#define WRITE_WORD(SRC, DST, FAULT) ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_WORD(mem, addr, (SRC)))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT) ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_QWORD(mem, addr, (SRC)))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST) sys_syscall(&regs, mem_access, mem, INST, TRUE)

/* start simulation, program loaded, processor precise state initialized */
void sim_main(void) {
    /* ECE552 Assignment 1 - BEGIN CODE*/
    int r_out[2], r_in[3];
    /* ECE552 Assignment 1 - END CODE*/

    md_inst_t inst;
    register md_addr_t addr;
    enum md_opcode op;
    register int is_write;
    enum md_fault_type fault;

    fprintf(stderr, "sim: ** starting functional simulation **\n");

    /* set up initial default next PC */
    regs.regs_NPC = regs.regs_PC + sizeof(md_inst_t);

    /* check for DLite debugger entry condition */
    if (dlite_check_break(regs.regs_PC, /* !access */ 0, /* addr */ 0, 0, 0)) dlite_main(regs.regs_PC - sizeof(md_inst_t), regs.regs_PC, sim_num_insn, &regs, mem);

    while (TRUE) {
        /* maintain $r0 semantics */
        regs.regs_R[MD_REG_ZERO] = 0;
#ifdef TARGET_ALPHA
        regs.regs_F.d[MD_REG_ZERO] = 0.0;
#endif /* TARGET_ALPHA */

        /* get the next instruction to execute */
        MD_FETCH_INST(inst, mem, regs.regs_PC);

        /* keep an instruction count */
        sim_num_insn++;

        /* set default reference address and access mode */
        addr     = 0;
        is_write = FALSE;

        /* set default fault - none */
        fault = md_fault_none;

        /* decode the instruction */
        MD_SET_OPCODE(op, inst);

        /* execute the instruction */

        switch (op) {
            /* ECE552 Assignment 1 - BEGIN CODE*/
#define DEFINST(OP, MSK, NAME, OPFORM, RES, FLAGS, O1, O2, I1, I2, I3) \
    case OP:                                                           \
        r_out[0] = (O1);                                               \
        r_out[1] = (O2);                                               \
        r_in[0]  = (I1);                                               \
        r_in[1]  = (I2);                                               \
        r_in[2]  = (I3);                                               \
        SYMCAT(OP, _IMPL);                                             \
        break;
            /* ECE552 Assignment 1 - END CODE*/

#define DEFLINK(OP, MSK, NAME, MASK, SHIFT) \
    case OP: panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#define DECLARE_FAULT(FAULT) \
    {                        \
        fault = (FAULT);     \
        break;               \
    }
#include "machine.def"
        default: panic("attempted to execute a bogus opcode");
        }

        /* ECE552 Assignment 1 - BEGIN CODE*/
        timestamp_q1++;
        timestamp_q2++;

        //# Check `num_cycle_stall_q1` and `num_cycle_stall_q2` for instruction based on input registers
        int num_cycle_stall_q1 = 0;
        int num_cycle_stall_q2 = 0;
        int i;
        //$ Q1
        for (i = 0; i < 3; i++) {
            if (r_in[i] == DNA) {
                continue;
            }
            if (reg_ready_q1[r_in[i]] > timestamp_q1) {
                num_cycle_stall_q1 = (reg_ready_q1[r_in[i]] - timestamp_q1) > num_cycle_stall_q1 ? (reg_ready_q1[r_in[i]] - timestamp_q1) : num_cycle_stall_q1;  // take max of stall stages
                // No need to consider the corner case of store in lab0, because processor in q1 has no forwarding
            }
        }
        if (num_cycle_stall_q1 > 0) {  // If stall needs to be inserted
            // Take maximum value from the stall needed for three input register
            timestamp_q1 += num_cycle_stall_q1;  // must add before calculating the ready time for output registers
            sim_num_RAW_hazard_q1++;

            if (num_cycle_stall_q1 == 1) {
                sim_num_raw_q1_1_cycle++;
            } else if (num_cycle_stall_q1 == 2) {
                sim_num_raw_q1_2_cycle++;
            } else {
                fprintf(stderr, "num_cycle_stall_q2 should be 1 or 2 in processor 1, but is %d\n", num_cycle_stall_q2);
            }
        }

        //$ Q2
        for (i = 0; i < 3; i++) {  // must use two loops, because of the break above
            if (reg_ready_q2[r_in[i]] > timestamp_q2) {
                int num_cycle_stall_q2_tmp = reg_ready_q2[r_in[i]] - timestamp_q2;
                if ((i == 0) && (MD_OP_FLAGS(op) & F_MEM) && (MD_OP_FLAGS(op) & F_STORE)) {
                    // Special case when load instruction is followed by store, no stall is needed in this case
                    num_cycle_stall_q2_tmp = 0;
                    // fprintf(stderr, "Store after load with one independent instruction between detected, op_code%x\n", op);
                    // 73619 store saved this way
                }
                num_cycle_stall_q2 = num_cycle_stall_q2_tmp > num_cycle_stall_q2 ? num_cycle_stall_q2_tmp : num_cycle_stall_q2;  // take max of stall stages
            }
        }
        if (num_cycle_stall_q2 > 0) {
            // Take maximum value from the stall needed for three input register
            timestamp_q2 += num_cycle_stall_q2;  // must add before calculating the ready time for output registers
            sim_num_RAW_hazard_q2++;

            if (num_cycle_stall_q2 == 1) {
                sim_num_raw_q2_1_cycle++;
            } else if (num_cycle_stall_q2 == 2) {
                sim_num_raw_q2_2_cycle++;
            } else {
                fprintf(stderr, "num_cycle_stall_q2 should be 1 or 2 in processor 2, but is %d\n", num_cycle_stall_q2);
            }
        }

        //# Set the `reg_ready` for the output registers
        for (i = 0; i < 2; i++) {
            if (r_out[i] == DNA) {
                continue;
            }

            //$ Q1
            // For processor 1, there's no forwarding, so the output register is always ready 3 stages after the execution stage
            reg_ready_q1[r_out[i]] = timestamp_q1 + 3;

            //$ Q2
            // For processor 2, there is forwarding. But there's also an extra stage of execution,EX2, compared with the 5-stage pipeline model
            if ((MD_OP_FLAGS(op) & F_MEM) && (MD_OP_FLAGS(op) & F_LOAD)) {
                // For load instruction, the output register will be available 4 stages after EX1 starts (include EX1), namely EX1, EX2, ME, and WB. However, as the lab handout says "Assume that the Writeback stage writes to the register ﬁle (RF) in the ﬁrst half of a cycle, while the Decode stage reads from the register ﬁle in the second half of a cycle." We can assume that the output register will be available 3 stages after EX1 starts, going through EX1, EX2, and ME.
                reg_ready_q2[r_out[i]] = timestamp_q2 + 3;
            } else {
                // For non-load instruction, the output register will be available 3 stages after EX1 starts (include EX1), namely EX1, EX2, and ME. However, as processor 2 support forwarding, we can get the result 1 stage earlier, at the end of EX2, through the MX pass.
                reg_ready_q2[r_out[i]] = timestamp_q2 + 2;
            }
        }

        /* ECE552 Assignment 1 - END CODE*/

        if (fault != md_fault_none) fatal("fault (%d) detected @ 0x%08p", fault, regs.regs_PC);

        if (verbose) {
            myfprintf(stderr, "%10n [xor: 0x%08x] @ 0x%08p: ", sim_num_insn, md_xor_regs(&regs), regs.regs_PC);
            md_print_insn(inst, regs.regs_PC, stderr);
            if (MD_OP_FLAGS(op) & F_MEM) myfprintf(stderr, "  mem: 0x%08p", addr);
            fprintf(stderr, "\n");
            /* fflush(stderr); */
        }

        if (MD_OP_FLAGS(op) & F_MEM) {
            sim_num_refs++;
            if (MD_OP_FLAGS(op) & F_STORE) is_write = TRUE;
        }

        /* check for DLite debugger entry condition */
        if (dlite_check_break(regs.regs_NPC, is_write ? ACCESS_WRITE : ACCESS_READ, addr, sim_num_insn, sim_num_insn)) dlite_main(regs.regs_PC, regs.regs_NPC, sim_num_insn, &regs, mem);

        /* go to the next instruction */
        regs.regs_PC = regs.regs_NPC;
        regs.regs_NPC += sizeof(md_inst_t);

        /* finish early? */
        if (max_insts && sim_num_insn >= max_insts) return;
    }
}
