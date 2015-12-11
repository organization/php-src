/*
   +----------------------------------------------------------------------+
   | Zend OPcache JIT                                                     |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2014 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Dmitry Stogov <dmitry@zend.com>                             |
   +----------------------------------------------------------------------+
*/

/* $Id:$ */

#include <ZendAccelerator.h>

#include "jit/zend_jit_config.h"
#include "jit/zend_jit_context.h"
#include "Optimizer/zend_worklist.h"

void zend_jit_dump_const(zval *zv)
{
	switch (Z_TYPE_P(zv)) {
		case IS_NULL:
			fprintf(stderr, " null");
			break;
		case IS_FALSE:
			fprintf(stderr, " bool(false)");
			break;
		case IS_TRUE:
			fprintf(stderr, " bool(true)");
			break;
		case IS_LONG:
			fprintf(stderr, " int(" ZEND_LONG_FMT ")", Z_LVAL_P(zv));
			break;
		case IS_DOUBLE:
			fprintf(stderr, " float(%g)", Z_DVAL_P(zv));
			break;
		case IS_STRING:
			fprintf(stderr, " string(\"%s\")", Z_STRVAL_P(zv));
			break;
		case IS_ARRAY:
//???		case IS_CONSTANT_ARRAY:
			fprintf(stderr, " array(...)");
			break;
		default:
			fprintf(stderr, " ???");
			break;
	}
}

void zend_jit_dump_var(zend_op_array *op_array, int var_num)
{
	if (var_num < op_array->last_var) {
		fprintf(stderr, "$%s", op_array->vars[var_num]->val);
	} else {
		fprintf(stderr, "$%d", var_num - op_array->last_var);
	}
}

static void zend_jit_dump_info(uint32_t info, zend_class_entry *ce, int is_instanceof)
{
	int first = 1;

	fprintf(stderr, " [");
	if (info & MAY_BE_UNDEF) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "undef");
	}
	if (info & MAY_BE_DEF) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "def");
	}
	if (info & MAY_BE_REF) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "ref");
	}
	if (info & MAY_BE_RC1) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "rc1");
	}
	if (info & MAY_BE_RCN) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "rcn");
	}
	if (info & MAY_BE_CLASS) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "class");
		if (ce) {
			if (is_instanceof) {
				fprintf(stderr, " (instanceof %s)", ce->name->val);
			} else {
				fprintf(stderr, " (%s)", ce->name->val);
			}
		}
	} else if ((info & MAY_BE_ANY) == MAY_BE_ANY) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "any");
	} else {
		if (info & MAY_BE_NULL) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "null");
		}
		if (info & MAY_BE_FALSE) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "false");
		}
		if (info & MAY_BE_TRUE) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "true");
		}
		if (info & MAY_BE_LONG) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "long");
		}
		if (info & MAY_BE_DOUBLE) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "double");
		}
		if (info & MAY_BE_STRING) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "string");
		}
		if (info & MAY_BE_ARRAY) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "array");
			if ((info & MAY_BE_ARRAY_KEY_ANY) != 0 &&
			    (info & MAY_BE_ARRAY_KEY_ANY) != MAY_BE_ARRAY_KEY_ANY) {
				int afirst = 1;
				fprintf(stderr, " [");
				if (info & MAY_BE_ARRAY_KEY_LONG) {
					if (afirst) afirst = 0; else fprintf(stderr, ", ");
					fprintf(stderr, "long");
				}
				if (info & MAY_BE_ARRAY_KEY_STRING) {
					if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "string");
					}
				fprintf(stderr, "]");
			}
			if (info & (MAY_BE_ARRAY_OF_ANY|MAY_BE_ARRAY_OF_REF)) {
				int afirst = 1;
				fprintf(stderr, " of [");
				if ((info & MAY_BE_ARRAY_OF_ANY) == MAY_BE_ARRAY_OF_ANY) {
					if (afirst) afirst = 0; else fprintf(stderr, ", ");
					fprintf(stderr, "any");
				} else {
					if (info & MAY_BE_ARRAY_OF_NULL) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "null");
					}
					if (info & MAY_BE_ARRAY_OF_FALSE) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "false");
					}
					if (info & MAY_BE_ARRAY_OF_TRUE) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "true");
					}
					if (info & MAY_BE_ARRAY_OF_LONG) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "long");
					}
					if (info & MAY_BE_ARRAY_OF_DOUBLE) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "double");
					}
					if (info & MAY_BE_ARRAY_OF_STRING) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "string");
					}
					if (info & MAY_BE_ARRAY_OF_ARRAY) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "array");
					}
					if (info & MAY_BE_ARRAY_OF_OBJECT) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "object");
					}
					if (info & MAY_BE_ARRAY_OF_RESOURCE) {
						if (afirst) afirst = 0; else fprintf(stderr, ", ");
						fprintf(stderr, "resource");
					}
				}
				if (info & MAY_BE_ARRAY_OF_REF) {
					if (afirst) afirst = 0; else fprintf(stderr, ", ");
					fprintf(stderr, "ref");
				}
				fprintf(stderr, "]");
			}
		}
		if (info & MAY_BE_OBJECT) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "object");
			if (ce) {
				if (is_instanceof) {
					fprintf(stderr, " (instanceof %s)", ce->name->val);
				} else {
					fprintf(stderr, " (%s)", ce->name->val);
				}
			}
		}
		if (info & MAY_BE_RESOURCE) {
			if (first) first = 0; else fprintf(stderr, ", ");
			fprintf(stderr, "resource");
		}
	}
	if (info & MAY_BE_ERROR) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "error");
	}
	if (info & MAY_BE_IN_REG) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "reg");
	}
	if (info & MAY_BE_REG_ZVAL) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "reg_zval");
	}
	if (info & MAY_BE_REG_ZVAL_PTR) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "reg_zval_ptr");
	}
	if (info & MAY_BE_IN_MEM) {
		if (first) first = 0; else fprintf(stderr, ", ");
		fprintf(stderr, "mem");
	}
	fprintf(stderr, "]");
}

static void zend_jit_dump_ssa_var_info(zend_jit_func_info *func_info, int ssa_var_num)
{
	uint32_t info = get_ssa_var_info(func_info, ssa_var_num);
	zend_class_entry *ce = NULL;
	int is_instanceof = 0;

	if (ssa_var_num >= 0 &&
	    func_info->ssa_var_info &&
	    func_info->ssa_var_info[ssa_var_num].ce) {
		ce = func_info->ssa_var_info[ssa_var_num].ce;
		is_instanceof = func_info->ssa_var_info[ssa_var_num].is_instanceof;
	}
	zend_jit_dump_info(info, ce, is_instanceof);
}

static void zend_jit_dump_range(zend_ssa_range *r)
{
	if (r->underflow && r->overflow) {
		return;
	}
	fprintf(stderr, " RANGE[");
	if (r->underflow) {
		fprintf(stderr, "--..");
	} else {
		fprintf(stderr, ZEND_LONG_FMT "..", r->min);
	}
	if (r->overflow) {
		fprintf(stderr, "++]");
	} else {
		fprintf(stderr, ZEND_LONG_FMT "]", r->max);
	}
}

static void zend_jit_dump_pi_range(zend_op_array *op_array, zend_ssa_pi_range *r)
{
	zend_jit_func_info *info = JIT_DATA(op_array);

	if (r->range.underflow && r->range.overflow) {
		return;
	}
	fprintf(stderr, " RANGE");
	if (r->negative) {
		fprintf(stderr, "~");
	}
	fprintf(stderr, "[");
	if (r->range.underflow) {
		fprintf(stderr, "-- .. ");
	} else {
		if (r->min_ssa_var >= 0) {
			fprintf(stderr, "#%d(", r->min_ssa_var);
			zend_jit_dump_var(op_array, r->min_var);
			fprintf(stderr, ")");
			if (info->ssa_var_info && info->ssa_var_info[r->min_ssa_var].has_range) {
				zend_jit_dump_range(&info->ssa_var_info[r->min_ssa_var].range);
			}
			if (r->range.min > 0) {
				fprintf(stderr, " + " ZEND_LONG_FMT, r->range.min);
			} else if (r->range.min < 0) {
				fprintf(stderr, " - " ZEND_LONG_FMT, -r->range.min);
			}
			fprintf(stderr, " .. ");
		} else {
			fprintf(stderr, ZEND_LONG_FMT " .. ", r->range.min);
		}
	}
	if (r->range.overflow) {
		fprintf(stderr, "++]");
	} else {
		if (r->max_ssa_var >= 0) {
			fprintf(stderr, "#%d(", r->max_ssa_var);
			zend_jit_dump_var(op_array, r->max_var);
			fprintf(stderr, ")");
			if (info->ssa_var_info && info->ssa_var_info[r->max_ssa_var].has_range) {
				zend_jit_dump_range(&info->ssa_var_info[r->max_ssa_var].range);
			}
			if (r->range.max > 0) {
				fprintf(stderr, " + " ZEND_LONG_FMT, r->range.max);
			} else if (r->range.max < 0) {
				fprintf(stderr, " - " ZEND_LONG_FMT, -r->range.max);
			}
			fprintf(stderr, "]");
		} else {
			fprintf(stderr, ZEND_LONG_FMT "]", r->range.max);
		}
	}
}

void zend_jit_dump_ssa_var(zend_op_array *op_array, int ssa_var_num, int var_num, int pos)
{
	(void) pos;
	zend_jit_func_info *info = JIT_DATA(op_array);

	if (ssa_var_num >= 0) {
		fprintf(stderr, "#%d(", ssa_var_num);
	} else {
		fprintf(stderr, "#?(");
	}
	zend_jit_dump_var(op_array, var_num);
	fprintf(stderr, ")");

	if (info->ssa.vars) {
		if (ssa_var_num >= 0 && info->ssa.vars[ssa_var_num].no_val) {
			fprintf(stderr, " NOVAL");
		}
		zend_jit_dump_ssa_var_info(info, ssa_var_num);
		if (ssa_var_num >= 0 && info->ssa_var_info[ssa_var_num].has_range) {
			zend_jit_dump_range(&info->ssa_var_info[ssa_var_num].range);
		}
	}
}

static void zend_jit_dump_var_set(
	zend_op_array *op_array,
	const char *name,
	zend_bitset set)
{
	int first = 1;
	uint32_t i;

	fprintf(stderr, "    ; %s = {", name);
	for (i = 0; i < op_array->last_var + op_array->T; i++) {
		if (zend_bitset_in(set, i)) {
			if (first) {
				first = 0;
			} else {
				fprintf(stderr, ", ");
			}
			zend_jit_dump_var(op_array, i);
		}
	}
	fprintf(stderr, "}\n");
}

static void zend_jit_dump_block_info(
	zend_op_array *op_array,
	zend_basic_block *block,
	int n)
{
	(void) op_array;

	fprintf(stderr, "  BB%d:\n", n);
	fprintf(stderr, "    ; lines=[%d-%d]\n", block[n].start, block[n].end);
	if ((block[n].flags & ZEND_BB_REACHABLE) == 0) {
		fprintf(stderr, "    ; unreachable\n");
	}
	if (block[n].flags & ZEND_BB_TARGET) {
		fprintf(stderr, "    ; jump target\n");
	}
	if (block[n].flags & ZEND_BB_FOLLOW) {
		fprintf(stderr, "    ; fallthrough control flow\n");
	}
	if (block[n].flags & ZEND_BB_ENTRY) {
		fprintf(stderr, "    ; entry block\n");
	}
	if (block[n].flags & ZEND_BB_TRY) {
		fprintf(stderr, "    ; try\n");
	}
	if (block[n].flags & ZEND_BB_CATCH) {
		fprintf(stderr, "    ; catch\n");
	}
	if (block[n].flags & ZEND_BB_FINALLY) {
		fprintf(stderr, "    ; fnally\n");
	}
	if (block[n].flags & ZEND_BB_GEN_VAR) {
		fprintf(stderr, "    ; gen var\n");
	}
	if (block[n].flags & ZEND_BB_KILL_VAR) {
		fprintf(stderr, "    ; kill var\n");
	}
	if (block[n].flags & ZEND_BB_LOOP_HEADER) {
		fprintf(stderr, "    ; loop header\n");
	}
	if (block[n].flags & ZEND_BB_IRREDUCIBLE_LOOP) {
		fprintf(stderr, "    ; entry to irreducible loop\n");
	}
	if (block[n].loop_header >= 0) {
		fprintf(stderr, "    ; part of loop from block %d\n", block[n].loop_header);
	}
	if (block[n].successors[0] >= 0 || block[n].successors[1] >= 0) {
		fprintf(stderr, "    ; successors={");
		if (block[n].successors[0] >= 0) {
			fprintf(stderr, "%d", block[n].successors[0]);
		}
		if (block[n].successors[1] >= 0) {
			fprintf(stderr, ", %d", block[n].successors[1]);
		}
		fprintf(stderr, "}\n");
	}
	if (block[n].predecessors_count) {
		zend_jit_func_info *info = JIT_DATA(op_array);
		int *predecessors = info->cfg.predecessors;
		int j;

		fprintf(stderr, "    ; predecessors={");
		for (j = 0; j < block[n].predecessors_count; j++) {
			fprintf(stderr, j ? ", %d" : "%d", predecessors[block[n].predecessor_offset + j]);
		}
		fprintf(stderr, "}\n");
	}
	if (block[n].idom >= 0) {
		fprintf(stderr, "    ; idom=%d\n", block[n].idom);
	}
	if (block[n].level >= 0) {
		fprintf(stderr, "    ; level=%d\n", block[n].level);
	}
	if (block[n].children >= 0) {
		int j = block[n].children;
		fprintf(stderr, "    ; children={%d", j);
		j = block[j].next_child;
		while (j >= 0) {
			fprintf(stderr, ", %d", j);
			j = block[j].next_child;
		}
		fprintf(stderr, "}\n");
	}
}

void zend_jit_dump_ssa_bb_header(zend_op_array *op_array, uint32_t line)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_basic_block *blocks = info->cfg.blocks;
	zend_ssa_block *ssa_blocks = info->ssa.blocks;
	int blocks_count = info->cfg.blocks_count;
	int n;

	if (!blocks) return;

	for (n = 0; n < blocks_count; n++) {
		if (line == blocks[n].start) break;
	}
	if (n < blocks_count && line == blocks[n].start) {
		zend_jit_dump_block_info(op_array, blocks, n);
		if (ssa_blocks && ssa_blocks[n].phis) {
			zend_ssa_phi *p = ssa_blocks[n].phis;

			do {
				int j;

				fprintf(stderr, "    ");
				zend_jit_dump_ssa_var(op_array, p->ssa_var, p->var, blocks[n].start);
				if (p->pi < 0) {
					fprintf(stderr, " = Phi(");
					for (j = 0; j < blocks[n].predecessors_count; j++) {
						if (j > 0) {
							fprintf(stderr, ", ");
						}
						zend_jit_dump_ssa_var(op_array, p->sources[j], p->var, blocks[n].start);
					}
					fprintf(stderr, ")\n");
				} else {
					fprintf(stderr, " = Pi(");
					zend_jit_dump_ssa_var(op_array, p->sources[0], p->var, blocks[n].start);
					fprintf(stderr, " &");
					zend_jit_dump_pi_range(op_array, &p->constraint);
					fprintf(stderr, ")\n");
				}
				p = p->next;
			} while (p);
		}
	}
}

void zend_jit_dump_ssa_line(zend_op_array *op_array, const zend_basic_block *b, uint32_t line)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_ssa_op *ssa_ops = info->ssa.ops;
	zend_op *opline = op_array->opcodes + line;
	const char *name = zend_get_opcode_name(opline->opcode);
	uint32_t flags = zend_get_opcode_flags(opline->opcode);
	int n = 0;

	fprintf(stderr, "    ");
	if (ssa_ops) {
		if (ssa_ops[line].result_def >= 0) {
			if (ssa_ops[line].result_use >= 0) {
				zend_jit_dump_ssa_var(op_array, ssa_ops[line].result_use, EX_VAR_TO_NUM(opline->result.var), line);
				fprintf(stderr, " -> ");
			}
			zend_jit_dump_ssa_var(op_array, ssa_ops[line].result_def, EX_VAR_TO_NUM(opline->result.var), line);
			fprintf(stderr, " = ");
		}
	} else if (opline->result_type == IS_CV || opline->result_type == IS_VAR || opline->result_type == IS_TMP_VAR) {
		zend_jit_dump_var(op_array, EX_VAR_TO_NUM(opline->result.var));
		fprintf(stderr, " = ");
	}
	fprintf(stderr, "%s", name ? (name + 5) : "???");

	if (ZEND_VM_OP1_JMP_ADDR & flags) {
		if (b) {
			fprintf(stderr, " BB%d", b->successors[n++]);
		} else {
			fprintf(stderr, " .OP_%u", (uint32_t)(OP_JMP_ADDR(opline, opline->op1) - op_array->opcodes));
		}
	} else if (ZEND_VM_OP1_NUM & flags) {
		fprintf(stderr, " %d", opline->op1.num);
	} else if (opline->op1_type == IS_CONST) {
		zend_jit_dump_const(RT_CONSTANT(op_array, opline->op1));
	} else if (opline->op1_type == IS_CV || opline->op1_type == IS_VAR || opline->op1_type == IS_TMP_VAR) {
	    fprintf(stderr, " ");
	    if (ssa_ops) {
			zend_jit_dump_ssa_var(op_array, ssa_ops[line].op1_use, EX_VAR_TO_NUM(opline->op1.var), line);
			if (ssa_ops[line].op1_def >= 0) {
			    fprintf(stderr, " -> ");
				zend_jit_dump_ssa_var(op_array, ssa_ops[line].op1_def, EX_VAR_TO_NUM(opline->op1.var), line);
			}
		} else {
			zend_jit_dump_var(op_array, EX_VAR_TO_NUM(opline->op1.var));
		}
	}
	if (ZEND_VM_OP2_JMP_ADDR & flags) {
		if (b) {
			fprintf(stderr, " BB%d", b->successors[n]);
		} else {
			fprintf(stderr, " .OP_%u", (uint32_t)(OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes));
		}
	} else if (ZEND_VM_OP2_NUM & flags) {
		fprintf(stderr, " %d", opline->op2.num);
	} else if (opline->op2_type == IS_CONST) {
		zend_jit_dump_const(RT_CONSTANT(op_array, opline->op2));
	} else if (opline->op2_type == IS_CV ||  opline->op2_type == IS_VAR || opline->op2_type == IS_TMP_VAR) {
	    fprintf(stderr, " ");
	    if (ssa_ops) {
			zend_jit_dump_ssa_var(op_array, ssa_ops[line].op2_use, EX_VAR_TO_NUM(opline->op2.var), line);
			if (ssa_ops[line].op2_def >= 0) {
			    fprintf(stderr, " -> ");
				zend_jit_dump_ssa_var(op_array, ssa_ops[line].op2_def, EX_VAR_TO_NUM(opline->op2.var), line);
			}
		} else {
			zend_jit_dump_var(op_array, EX_VAR_TO_NUM(opline->op2.var));
		}
	}
	if (ZEND_VM_EXT_JMP_ADDR & flags) {
		if (opline->opcode != ZEND_CATCH || !opline->result.num) {
			if (b) {
				fprintf(stderr, " BB%d", b->successors[n++]);
			} else {
				fprintf(stderr, " .OP_" ZEND_LONG_FMT, ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value));
			}
		}
	}
	fprintf(stderr, "\n");
}

static void zend_jit_dump_ssa(zend_op_array *op_array)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	int blocks_count = info->cfg.blocks_count;
	int ssa_vars_count = info->ssa.vars_count;
	uint32_t i, j;
	int k;

	if (op_array->function_name) {
		if (op_array->scope && op_array->scope->name) {
			fprintf(stderr, "\n%s::%s", op_array->scope->name->val, op_array->function_name->val);
		} else {
			fprintf(stderr, "\n%s", op_array->function_name->val);
		}
	} else {
		fprintf(stderr, "\n%s", "$_main");
	}
	if (info->clone_num > 0) {
		fprintf(stderr, "__clone_%d", info->clone_num);
	}
	fprintf(stderr, ": ; (lines=%d, args=%d/%d, vars=%d, tmps=%d, blocks=%d, ssa_vars=%d",
		op_array->last,
		op_array->num_args,
		info->num_args,
		op_array->last_var,
		op_array->T,
		blocks_count,
		ssa_vars_count);
	if (info->flags & ZEND_JIT_FUNC_RECURSIVE) {
		fprintf(stderr, ", recursive");
		if (info->flags & ZEND_JIT_FUNC_RECURSIVE_DIRECTLY) {
			fprintf(stderr, " directly");
		}
		if (info->flags & ZEND_JIT_FUNC_RECURSIVE_INDIRECTLY) {
			fprintf(stderr, " indirectly");
		}
	}
	if (info->flags & ZEND_FUNC_IRREDUCIBLE) {
		fprintf(stderr, ", irreducable");
	}
	if (info->flags & ZEND_FUNC_NO_LOOPS) {
		fprintf(stderr, ", no_loops");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_IN_MEM_CVS) {
		fprintf(stderr, ", no_in_mem_cvs");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_USED_ARGS) {
		fprintf(stderr, ", no_used_args");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_SYMTAB) {
		fprintf(stderr, ", no_symtab");
	}
	if (info->flags & ZEND_JIT_FUNC_NO_FRAME) {
		fprintf(stderr, ", no_frame");
	}
	if (info->flags & ZEND_JIT_FUNC_INLINE) {
		fprintf(stderr, ", inline");
	}
	if (info->return_value_used == 0) {
		fprintf(stderr, ", no_return_value");
	} else if (info->return_value_used == 1) {
		fprintf(stderr, ", return_value");
	}
	fprintf(stderr, ")\n");

	if (info->num_args > 0) {
		for (i = 0; i < MIN(op_array->num_args, info->num_args ); i++) {
			fprintf(stderr, "    ; arg %d ", i);
			zend_jit_dump_info(info->arg_info[i].info.type, info->arg_info[i].info.ce, info->arg_info[i].info.is_instanceof);
			zend_jit_dump_range(&info->arg_info[i].info.range);
			fprintf(stderr, "\n");
		}
	}
	
	fprintf(stderr, "    ; return ");
	zend_jit_dump_info(info->return_info.type, info->return_info.ce, info->return_info.is_instanceof);
	zend_jit_dump_range(&info->return_info.range);
	fprintf(stderr, "\n");

#if 1
	for (k = 0; k < op_array->last_var; k++) {
		fprintf(stderr, "    ; ");
		if (info->ssa_var_info && (info->ssa_var_info[k].type & (MAY_BE_DEF|MAY_BE_UNDEF|MAY_BE_IN_MEM)) == (MAY_BE_DEF|MAY_BE_IN_MEM)) {
			fprintf(stderr, "preallocated ");
		}
		fprintf(stderr, "CV ");
		zend_jit_dump_ssa_var(op_array, k, k, -1);
		fprintf(stderr, "\n");
	}
#else
	if (info->flags & ZEND_JIT_FUNC_HAS_PREALLOCATED_CVS) {
		for (k = 0; k < op_array->last_var; k++) {
			if ((info->ssa_var_info[k].type & (MAY_BE_DEF|MAY_BE_UNDEF|MAY_BE_IN_MEM)) == (MAY_BE_DEF|MAY_BE_IN_MEM)) {
				fprintf(stderr, "    ; preallocate ");
				zend_jit_dump_ssa_var(op_array, k, k, -1);
				fprintf(stderr, "\n");
			}
		}
	}
#endif
	for (k = 0; k < info->cfg.blocks_count; k++) {
		const zend_basic_block *b = info->cfg.blocks + k;

		zend_jit_dump_ssa_bb_header(op_array,  b->start);
		for (j = b->start; j <= b->end; j++) {
			zend_jit_dump_ssa_line(op_array, b, j);
		}
		/* Insert implicit JMP, introduced by block sorter, if necessary */
		if (b->successors[0] >= 0 &&
		    b->successors[1] < 0 &&
		    b->successors[0] != k + 1) {
			switch (op_array->opcodes[b->end].opcode) {
				case ZEND_JMP:
					break;
				default:
					fprintf(stderr, "    JMP BB%d [implicit]\n", info->cfg.blocks[k].successors[0]);
					break;
			}
		}
	}
}

void zend_jit_dump(zend_op_array *op_array, uint32_t flags)
{
	int j;
	zend_jit_func_info *info = JIT_DATA(op_array);
	zend_basic_block *blocks = info->cfg.blocks;
	zend_ssa_block *ssa_blocks = info->ssa.blocks;
	int blocks_count = info->cfg.blocks_count;

	if (flags & JIT_DUMP_CFG) {
		fprintf(stderr, "CFG (lines=%d, blocks=%d)\n", op_array->last, blocks_count);
		for (j = 0; j < blocks_count; j++) {
			zend_jit_dump_block_info(op_array, blocks, j);
		}
	}

	if (flags & JIT_DUMP_DOMINATORS) {
		fprintf(stderr, "Dominators Tree\n");
		for (j = 0; j < blocks_count; j++) {
			zend_jit_dump_block_info(op_array, blocks, j);
		}
	}

	if (flags & JIT_DUMP_PHI_PLACEMENT) {
		fprintf(stderr, "SSA Phi() Placement\n");
		for (j = 0; j < blocks_count; j++) {
			if (ssa_blocks && ssa_blocks[j].phis) {
				zend_ssa_phi *p = ssa_blocks[j].phis;
				int first = 1;

				fprintf(stderr, "  BB%d:\n", j);
				if (p->pi >= 0) {
					fprintf(stderr, "    ; pi={");
				} else {
					fprintf(stderr, "    ; phi={");
				}
				do {
					if (first) {
						first = 0;
					} else {
						fprintf(stderr, ", ");
					}
					zend_jit_dump_var(op_array, p->var);
					p = p->next;
				} while (p);
				fprintf(stderr, "}\n");
			}
		}
	}

	if (flags & JIT_DUMP_VAR) {
		fprintf(stderr, "CV Variables for \"");
		if (op_array->function_name) {
			if (op_array->scope && op_array->scope->name) {
				fprintf(stderr, "%s::%s\":\n", op_array->scope->name->val, op_array->function_name->val);
			} else {
				fprintf(stderr, "%s\":\n", op_array->function_name->val);
			}
		} else {
			fprintf(stderr, "%s\":\n", "$_main");
		}
		for (j = 0; j < op_array->last_var; j++) {
			fprintf(stderr, "  %2d: $%s\n", j, op_array->vars[j]->val);
		}
	}

	if ((flags & JIT_DUMP_VAR_TYPES) && info->ssa.vars) {
		fprintf(stderr, "SSA Variable Types for \"");
		if (op_array->function_name) {
			if (op_array->scope && op_array->scope->name) {
				fprintf(stderr, "%s::%s\":\n", op_array->scope->name->val, op_array->function_name->val);
			} else {
				fprintf(stderr, "%s\":\n", op_array->function_name->val);
			}
		} else {
			fprintf(stderr, "%s\":\n", "$_main");
		}
		for (j = 0; j < info->ssa.vars_count; j++) {
			fprintf(stderr, "  #%d(", j);
			zend_jit_dump_var(op_array, info->ssa.vars[j].var);
			fprintf(stderr, ")");
			if (info->ssa.vars[j].scc >= 0) {
				if (info->ssa.vars[j].scc_entry) {
					fprintf(stderr, " *");
				}  else {
					fprintf(stderr, "  ");
				}
				fprintf(stderr, "SCC=%d", info->ssa.vars[j].scc);
			}
			zend_jit_dump_ssa_var_info(info, j);
			if (info->ssa_var_info && info->ssa_var_info[j].has_range) {
				zend_jit_dump_range(&info->ssa_var_info[j].range);
			}
			fprintf(stderr, "\n");
		}
	}

	if (flags & JIT_DUMP_SSA) {
		zend_jit_dump_ssa(op_array);
	}

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
