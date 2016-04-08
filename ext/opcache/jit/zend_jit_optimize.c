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
#include <stdlib.h>

#include "jit/zend_jit.h"
#include "jit/zend_jit_config.h"
#include "jit/zend_jit_context.h"
#include "jit/zend_jit_codegen.h"
#include "Optimizer/zend_worklist.h"
#include "Optimizer/zend_dump.h"

#define FILL_ARRAY(to, val, type, n)									\
	do {																\
		type *to__ = to;												\
		type val__ = val;												\
		int i = n;														\
		while (i-- > 0)													\
			to__[i] = val__;											\
	} while (0)

struct block_order {
	int block_num;
	int last_visited;
};

static int compare_blocks(const void *p1, const void *p2)
{
	const struct block_order *b1 = p1, *b2 = p2;

	return b2->last_visited - b1->last_visited;
}

/* To prefer a branch, push on the other one.  That way the preferred branch
   will be visited second, resulting in an earlier and more contiguous reverse
   post-order.  */
static int choose_branch(zend_func_info *info, zend_worklist *in, int i)
{
	int s0, s1, s0_loop, s1_loop;

	s0 = info->ssa.cfg.blocks[i].successors[0];
	s1 = info->ssa.cfg.blocks[i].successors[1];

	if (s1 < 0)
		return 0;

	s0_loop = info->ssa.cfg.blocks[s0].loop_header;
	s1_loop = info->ssa.cfg.blocks[s1].loop_header;

	if (s0_loop == s1_loop)
		return 0;

	/* Prefer the successor whose loop header is latest in the linear block
	   order.  This will generally prefer inner loops.  */
	return zend_worklist_push(in, (s1_loop > s0_loop) ? s0 : s1);
}

static int compute_block_map(zend_func_info *info, struct block_order *order, int *block_map)
{
	int blocks_count = 0, next;

	for (next = 0; next < info->ssa.cfg.blocks_count; ) {
		int from = order[next].block_num;

		if (!order[next].last_visited) {
			block_map[from] = -1;
		} else {
			block_map[from] = next;
			blocks_count++;
		}
		next++;
	}

	return blocks_count;
}

static int compute_block_order(zend_op_array *op_array, struct block_order *order, int *block_map, int *live_blocks)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	zend_worklist in;
	int blocks_count = info->ssa.cfg.blocks_count;
	int i;
	int visit_count = 0;
	ALLOCA_FLAG(use_heap);

	ZEND_ASSERT(blocks_count);

	ZEND_WORKLIST_ALLOCA(&in, blocks_count, use_heap);

	for (i = 0; i < blocks_count; i++) {
		order[i].block_num = i;
		order[i].last_visited = 0;
	}

	ZEND_ASSERT(info->ssa.cfg.blocks[0].start == 0);

	zend_worklist_push(&in, 0);

	/* Currently, we don't do the SSA thing if there are jumps to unknown
	   targets or catch blocks.  Thus we don't have to add them as roots for the
	   DFS; if they are not found from the entry block, they are dead.  */
	ZEND_ASSERT(!op_array->last_try_catch);

	while (zend_worklist_len(&in)) {
		int i = zend_worklist_peek(&in);
		zend_basic_block *block = info->ssa.cfg.blocks + i;

		if (choose_branch(info, &in, i))
			continue;

		if (block->successors[0] >= 0
			&& zend_worklist_push(&in, block->successors[0]))
			continue;

		if (block->successors[1] >= 0
			&& zend_worklist_push(&in, block->successors[1]))
			continue;

		order[i].last_visited = ++visit_count;

		zend_worklist_pop(&in);
	}

	/* FIXME: There must be some way to avoid this N log N step.  */
	qsort(order, blocks_count, sizeof(*order), compare_blocks);

	*live_blocks = compute_block_map(info, order, block_map);

	ZEND_WORKLIST_FREE_ALLOCA(&in, use_heap);

	return SUCCESS;
}

static int remap_block(int *block_map, int from)
{
	return (from < 0) ? from : block_map[from];
}

/* Sort blocks in reverse post-order.  */
static int zend_jit_sort_blocks(zend_jit_context *ctx, zend_op_array *op_array)
{
//??? Block sorter leads to generation of incorrect code
#if 0
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	struct block_order *order;
	int *block_map;
	zend_basic_block *blocks;
	int blocks_count;
	int i, k;
	uint32_t j;

	order = alloca(sizeof(struct block_order) * info->ssa.cfg.blocks_count);

	/* block_map: an array mapping old block_num to new block_num.  */
	block_map = alloca(sizeof(int) * info->ssa.cfg.blocks_count);
	FILL_ARRAY(block_map, -1, int, info->ssa.cfg.blocks_count);

	if (compute_block_order(op_array, order, block_map, &blocks_count) != SUCCESS)
		return FAILURE;

	ZEND_ASSERT(blocks_count <= info->ssa.cfg.blocks_count);

	/* Check if block order is still the same */
	if (blocks_count == info->ssa.cfg.blocks_count) {
		for (i = 0; i < blocks_count; i++) {
			if (block_map[i] != i) {
				break;
			}
		}
		if (i == blocks_count) {
			return SUCCESS;
		}
	}		

	/* FIXME: avoid double allocation */
	blocks = zend_arena_calloc(&ctx->arena, blocks_count, sizeof(*blocks));

	for (i = 0; i < info->ssa.cfg.blocks_count; i++) {
		if (block_map[i] >= 0) {
			zend_basic_block *bb = &blocks[block_map[i]];
			zend_ssa_phi *p;

			*bb = info->ssa.cfg.blocks[i];
			bb->flags &= ~(ZEND_BB_TARGET | ZEND_BB_FOLLOW);
			for (j = bb->start; j <= bb->end; j++) {
				info->ssa.cfg.block_map[j] = block_map[i];
			}
			for (k = 0; k < bb->predecessors_count; k++) {
				bb->predecessors[k] = remap_block(block_map, bb->predecessors[k]);
			}
			bb->successors[0] = remap_block(block_map, bb->successors[0]);
			bb->successors[1] = remap_block(block_map, bb->successors[1]);
			bb->idom = remap_block(block_map, bb->idom);
			bb->loop_header = remap_block(block_map, bb->loop_header);
			bb->children = remap_block(block_map, bb->children);
			bb->next_child = remap_block(block_map, bb->next_child);

			p = bb->phis;
			while (p) {
				p->pi = remap_block(block_map, p->pi);
				p->block = remap_block(block_map, p->block);
				p = p->next;
			}
		}
	}

	for (i = 0; i < blocks_count; i++) {
		zend_basic_block *bb = &blocks[i];
		zend_op *op = &op_array->opcodes[bb->end];

		switch (op->opcode) {
			case ZEND_JMP:
				if (bb->successors[0] == i + 1 && bb->end > bb->start) {
					bb->end--;
					blocks[i + 1].flags |= ZEND_BB_FOLLOW;
				} else {
					blocks[bb->successors[0]].flags |= ZEND_BB_TARGET;
				}
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
			case ZEND_JMPZNZ:
				if (bb->successors[0] == i + 1) {
					op->op2.jmp_addr = op_array->opcodes + blocks[bb->successors[1]].start;
					blocks[bb->successors[1]].flags |= ZEND_BB_TARGET;
					blocks[bb->successors[0]].flags |= ZEND_BB_FOLLOW;
					switch (op->opcode) {
						case ZEND_JMPZNZ:
							op->opcode = ZEND_JMPZ;
							op->extended_value = 0;
							break;
						case ZEND_JMPZ:
							op->opcode = ZEND_JMPNZ;
							break;
						case ZEND_JMPNZ:
							op->opcode = ZEND_JMPZ;
							break;
						case ZEND_JMPZ_EX:
							op->opcode = ZEND_JMPNZ_EX;
							break;
						case ZEND_JMPNZ_EX:
							op->opcode = ZEND_JMPZ_EX;
							break;
						default:
							ASSERT_NOT_REACHED();
							break;
					}
				} else if (bb->successors[1] == i + 1) {
					blocks[bb->successors[0]].flags |= ZEND_BB_TARGET;
					blocks[bb->successors[1]].flags |= ZEND_BB_FOLLOW;
					switch (op->opcode) {
						case ZEND_JMPZNZ:
							op->opcode = ZEND_JMPNZ;
							op->op2.jmp_addr = op_array->opcodes + blocks[bb->successors[0]].start;
							op->extended_value = 0;
							break;
						case ZEND_JMPZ:
						case ZEND_JMPNZ:
						case ZEND_JMPZ_EX:
						case ZEND_JMPNZ_EX:
							break;
						default:
							ASSERT_NOT_REACHED();
							break;
					}
				} else {
					blocks[bb->successors[0]].flags |= ZEND_BB_TARGET;
					blocks[bb->successors[1]].flags |= ZEND_BB_TARGET;
					switch (op->opcode) {
						case ZEND_JMPZNZ:
							break;
						case ZEND_JMPZ:
							op->opcode = ZEND_JMPZNZ;
//???
							op->extended_value = (char*)(op_array->opcodes + blocks[bb->successors[1]].start) - (char*)op;
							op->op2.jmp_addr = op_array->opcodes + blocks[bb->successors[0]].start;
							break;
						case ZEND_JMPNZ:
							op->opcode = ZEND_JMPZNZ;
							op->extended_value = (char*)(op_array->opcodes + blocks[bb->successors[0]].start) - (char*)op;
							op->op2.jmp_addr = op_array->opcodes + blocks[bb->successors[1]].start;
							break;
						case ZEND_JMPZ_EX:
						case ZEND_JMPNZ_EX:
						default:
							/* JMPZ_EX and JMPNZ_EX are used in the
							   implementation of && and ||.  Neither successor
							   of this kind of block should be a named jump
							   target, so one of them should always follow
							   directly.  */
							ASSERT_NOT_REACHED();
							break;
					}
				}
				ZEND_VM_SET_OPCODE_HANDLER(op);
				break;
			case ZEND_JMP_SET:
			case ZEND_COALESCE:
			case ZEND_CATCH:
			case ZEND_FE_RESET_R:
			case ZEND_FE_RESET_RW:
			case ZEND_ASSERT_CHECK:
				blocks[bb->successors[0]].flags |= ZEND_BB_TARGET;
				blocks[bb->successors[1]].flags |= ZEND_BB_FOLLOW;
				break;
			case ZEND_OP_DATA:
				if ((op-1)->opcode == ZEND_FE_FETCH_R || (op-1)->opcode == ZEND_FE_FETCH_RW) {
					blocks[bb->successors[0]].flags |= ZEND_BB_TARGET;
					blocks[bb->successors[1]].flags |= ZEND_BB_FOLLOW;
				} else if (bb->successors[0] >= 0) {
					blocks[bb->successors[0]].flags |= ZEND_BB_FOLLOW;
				}
				break;
			case ZEND_RETURN:
			case ZEND_RETURN_BY_REF:
#if ZEND_EXTENSION_API_NO > PHP_5_4_X_API_NO
			case ZEND_GENERATOR_RETURN:
#endif
			case ZEND_EXIT:
			case ZEND_THROW:
				break;
			default:
				if (bb->successors[0] >= 0) {
					blocks[bb->successors[0]].flags |= ZEND_BB_FOLLOW;
				}
				break;
		}
	}

	info->ssa.cfg.blocks = blocks;
	info->ssa.cfg.blocks_count = blocks_count;
#endif
	return SUCCESS;
}

//#define LOG_SSA_RANGE
//#define LOG_NEG_RANGE
#define SYM_RANGE
#define NEG_RANGE
#define RANGE_WARMAP_PASSES 16

#define FOR_EACH_DEFINED_VAR(line, MACRO) \
	do { \
		if (info->ssa.ops[line].op1_def >= 0) { \
			MACRO(info->ssa.ops[line].op1_def); \
		} \
		if (info->ssa.ops[line].op2_def >= 0) { \
			MACRO(info->ssa.ops[line].op2_def); \
		} \
		if (info->ssa.ops[line].result_def >= 0) { \
			MACRO(info->ssa.ops[line].result_def); \
		} \
		if (op_array->opcodes[line].opcode == ZEND_OP_DATA) { \
			if (info->ssa.ops[line-1].op1_def >= 0) { \
				MACRO(info->ssa.ops[line-1].op1_def); \
			} \
			if (info->ssa.ops[line-1].op2_def >= 0) { \
				MACRO(info->ssa.ops[line-1].op2_def); \
			} \
			if (info->ssa.ops[line-1].result_def >= 0) { \
				MACRO(info->ssa.ops[line-1].result_def); \
			} \
		} else if (line+1 < op_array->last && \
		           op_array->opcodes[line+1].opcode == ZEND_OP_DATA) { \
			if (info->ssa.ops[line+1].op1_def >= 0) { \
				MACRO(info->ssa.ops[line+1].op1_def); \
			} \
			if (info->ssa.ops[line+1].op2_def >= 0) { \
				MACRO(info->ssa.ops[line+1].op2_def); \
			} \
			if (info->ssa.ops[line+1].result_def >= 0) { \
				MACRO(info->ssa.ops[line+1].result_def); \
			} \
		} \
	} while (0)


#define FOR_EACH_VAR_USAGE(_var, MACRO) \
	do { \
		zend_ssa_phi *p = info->ssa.vars[_var].phi_use_chain; \
		int use = info->ssa.vars[_var].use_chain; \
		while (use >= 0) { \
			FOR_EACH_DEFINED_VAR(use, MACRO); \
			use = zend_ssa_next_use(info->ssa.ops, _var, use); \
		} \
		p = info->ssa.vars[_var].phi_use_chain; \
		while (p) { \
			MACRO(p->ssa_var); \
			p = zend_ssa_next_use_phi(&info->ssa, _var, p); \
		} \
	} while (0)

static void zend_jit_func_arg_info(zend_jit_context      *ctx,
                                   zend_op_array         *op_array,
                                   int                    num,
                                   int                    recursive,
                                   int                    widening,
                                   zend_ssa_var_info *ret)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	zend_ssa *ssa = &info->ssa;
	uint32_t tmp = 0;
	zend_class_entry *tmp_ce = NULL;
	int tmp_is_instanceof = -1; /* unknown */
	int tmp_has_range = -1;
	zend_ssa_range tmp_range = {0,0,0,0};

	if (info && info->caller_info) {
		zend_call_info *call_info = info->caller_info;

		do {
			if (call_info->num_args >= info->num_args &&
			    call_info->recursive <= recursive) {
				zend_op_array *op_array = call_info->caller_op_array;
				zend_func_info *caller_info = ZEND_FUNC_INFO(op_array);
				zend_op *opline = call_info->arg_info[num].opline;
				int line = opline - op_array->opcodes; 

				tmp |= OP1_INFO() & (MAY_BE_ANY | MAY_BE_ARRAY_KEY_ANY | MAY_BE_ARRAY_OF_ANY | MAY_BE_ARRAY_OF_REF);

				// Class type inference
				if ((OP1_INFO() & MAY_BE_OBJECT) &&
				    caller_info &&
				    caller_info->ssa.ops &&
				    caller_info->ssa.var_info &&
				    caller_info->ssa.ops[line].op1_use >= 0 &&
				    caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].ce) {

					if (tmp_is_instanceof < 0) {
						tmp_ce = caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].ce;
						tmp_is_instanceof = caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].is_instanceof;
					} else if (caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].ce &&
					           caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].ce == tmp_ce) {
						if (caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].is_instanceof) {
							tmp_is_instanceof = 1;
						}
					} else {
						tmp_ce = NULL;
						tmp_is_instanceof = 0;
					}
				} else {
					tmp_ce = NULL;
					tmp_is_instanceof = 0;
				}

				if (opline->op1_type == IS_CONST) {
					if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op1)) == IS_NULL) {
						if (tmp_has_range < 0) {
							tmp_has_range = 1;
							tmp_range.underflow = 0;
							tmp_range.min = 0;
							tmp_range.max = 0;
							tmp_range.overflow = 0;
						} else if (tmp_has_range) {
							if (!tmp_range.underflow) {
								tmp_range.min = MIN(tmp_range.min, 0);
							}
							if (!tmp_range.overflow) {
								tmp_range.max = MAX(tmp_range.max, 0);
							}
						}
					} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op1)) == IS_FALSE) {
						if (tmp_has_range < 0) {
							tmp_has_range = 1;
							tmp_range.underflow = 0;
							tmp_range.min = 0;
							tmp_range.max = 0;
							tmp_range.overflow = 0;
						} else if (tmp_has_range) {
							if (!tmp_range.underflow) {
								tmp_range.min = MIN(tmp_range.min, 0);
							}
							if (!tmp_range.overflow) {
								tmp_range.max = MAX(tmp_range.max, 0);
							}
						}
					} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op1)) == IS_TRUE) {
						if (tmp_has_range < 0) {
							tmp_has_range = 1;
							tmp_range.underflow = 0;
							tmp_range.min = 1;
							tmp_range.max = 1;
							tmp_range.overflow = 0;
						} else if (tmp_has_range) {
							if (!tmp_range.underflow) {
								tmp_range.min = MIN(tmp_range.min, 1);
							}
							if (!tmp_range.overflow) {
								tmp_range.max = MAX(tmp_range.max, 1);
							}
						}
					} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op1)) == IS_LONG) {
						if (tmp_has_range < 0) {
							tmp_has_range = 1;
							tmp_range.underflow = 0;
							tmp_range.min = Z_LVAL_P(RT_CONSTANT(op_array, opline->op1));
							tmp_range.max = Z_LVAL_P(RT_CONSTANT(op_array, opline->op1));
							tmp_range.overflow = 0;
						} else if (tmp_has_range) {
							if (!tmp_range.underflow) {
								tmp_range.min = MIN(tmp_range.min, Z_LVAL_P(RT_CONSTANT(op_array, opline->op1)));
							}
							if (!tmp_range.overflow) {
								tmp_range.max = MAX(tmp_range.max, Z_LVAL_P(RT_CONSTANT(op_array, opline->op1)));
							}
						}
					} else {
						tmp_has_range = 0;
					}
				} else if (caller_info &&
				           caller_info->ssa.var_info &&
				           caller_info->ssa.ops[line].op1_use >= 0) {
					if (caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].has_range) {
						if (tmp_has_range < 0) {
							tmp_has_range = 1;
							tmp_range = caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].range;
						} else if (tmp_has_range) {
							if (caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].range.underflow) {
								tmp_range.underflow = 1;
								tmp_range.min = LONG_MIN;
							} else {
								tmp_range.min = MIN(tmp_range.min, caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].range.min);
							}
							if (caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].range.overflow) {
								tmp_range.overflow = 1;
								tmp_range.max = LONG_MAX;
							} else {
								tmp_range.max = MAX(tmp_range.max, caller_info->ssa.var_info[caller_info->ssa.ops[line].op1_use].range.max);
							}
						}
					} else if (!widening) {
						tmp_has_range = 1;
						tmp_range.underflow = 1;
						tmp_range.min = LONG_MIN;
						tmp_range.max = LONG_MAX;
						tmp_range.overflow = 1;
					}
				} else {
					tmp_has_range = 0;
				}
			}
			call_info = call_info->next_caller;
		} while (call_info);
	}
	ret->type = tmp;
	if (tmp_is_instanceof < 0) {
		tmp_is_instanceof = 0;
	}
	ret->ce = tmp_ce;
	ret->is_instanceof = tmp_is_instanceof;
	if (tmp_has_range < 0) {
		tmp_has_range = 0;
	}
	ret->has_range = tmp_has_range;
	ret->range = tmp_range;
}

int zend_jit_optimize_ssa(zend_jit_context *ctx, zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);

	if ((ZCG(accel_directives).jit_opt & JIT_OPT_SSA) >= JIT_OPT_SSA_O1) {
		if (zend_jit_sort_blocks(ctx, op_array) != SUCCESS) {
			return FAILURE;
		}
	}

	if (zend_ssa_compute_use_def_chains(&ctx->arena, op_array, &info->ssa) != SUCCESS) {
		return FAILURE;
	}

	if (zend_ssa_find_false_dependencies(op_array, &info->ssa) != SUCCESS) {
		return FAILURE;
	}

	if ((ZCG(accel_directives).jit_opt & JIT_OPT_SSA) >= JIT_OPT_SSA_O1) {
		zend_ssa_find_sccs(op_array, &info->ssa);

		if (zend_ssa_inference(&ctx->arena, op_array, ctx->main_script, &info->ssa) != SUCCESS) {
			return FAILURE;
		}
	}

	return SUCCESS;
}

static void zend_jit_check_no_used_args(zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	int i, num_args;

	if (info->flags & ZEND_FUNC_VARARG) {
		return;
	}

	if (info->num_args && info->num_args > 0) {
		num_args = MIN(op_array->num_args, info->num_args);
		for (i = 0; i < num_args; i++) {
			if (info->arg_info[i].ssa_var < 0 ||
				(info->ssa.var_info[info->arg_info[i].ssa_var].type & (MAY_BE_ANY | MAY_BE_REF))) {
				return;
			}
		}
		info->flags |= ZEND_JIT_FUNC_NO_USED_ARGS;
	}
}

static void zend_jit_check_no_symtab(zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	zend_ssa *ssa = &info->ssa;
	int b, i;

	if (!info ||
	    !op_array->function_name ||
	    !info->ssa.var_info ||
	    (op_array->last_try_catch != 0) ||
	    (op_array->fn_flags & ZEND_ACC_GENERATOR) ||
	    (info->flags & ZEND_FUNC_INDIRECT_VAR_ACCESS)) {
		return;
	}
	for (b = 0; b < info->ssa.cfg.blocks_count; b++) {
		if ((info->ssa.cfg.blocks[b].flags & ZEND_BB_REACHABLE) == 0) {
			continue;
		}
		for (i = info->ssa.cfg.blocks[b].start; i <= info->ssa.cfg.blocks[b].end; i++) {
			zend_op *opline = op_array->opcodes + i;

			switch (opline->opcode) {
				case ZEND_NOP:
				case ZEND_JMP:
					break;
				case ZEND_ASSIGN_ADD:
				case ZEND_ASSIGN_SUB:
				case ZEND_ASSIGN_MUL:
				case ZEND_ASSIGN_SL:
				case ZEND_ASSIGN_SR:
				case ZEND_ASSIGN_BW_OR:
				case ZEND_ASSIGN_BW_AND:
				case ZEND_ASSIGN_BW_XOR:
				case ZEND_ASSIGN_POW:
					if (opline->extended_value) {
						return;
					}
				case ZEND_ADD:
				case ZEND_SUB:
				case ZEND_MUL:
				case ZEND_SL:
				case ZEND_SR:
				case ZEND_BW_OR:
				case ZEND_BW_AND:
				case ZEND_BW_XOR:
				case ZEND_BOOL_XOR:
				case ZEND_POW:
					if (OP1_MAY_BE(MAY_BE_OBJECT|MAY_BE_RESOURCE)) {
						return;
					}
					if (OP2_MAY_BE(MAY_BE_OBJECT|MAY_BE_RESOURCE)) {
						return;
					}
					goto check_ops;
				case ZEND_ASSIGN_DIV:
				case ZEND_ASSIGN_MOD:
					if (opline->extended_value) {
						return;
					}
				case ZEND_DIV:
				case ZEND_MOD:
					if (opline->op2_type == IS_CONST) {
						if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op2)) == IS_NULL) {
							return;
						} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op2)) == IS_FALSE) {
							return;
						} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op2)) == IS_LONG) {
							if (Z_LVAL_P(RT_CONSTANT(op_array, opline->op2)) == 0) return;
						} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op2)) == IS_DOUBLE) {
							if (Z_DVAL_P(RT_CONSTANT(op_array, opline->op2)) == 0) return;
						} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->op2)) == IS_STRING) {
							/* TODO: division by zero */
							return;
						}
					} else {
						if (info->ssa.ops[i].op2_use < 0 ||
						    !info->ssa.var_info[info->ssa.ops[i].op2_use].has_range ||
						    (info->ssa.var_info[info->ssa.ops[i].op2_use].range.min <= 0 &&
						     info->ssa.var_info[info->ssa.ops[i].op2_use].range.max >= 0)) {
							return;
						}
					}
					goto check_ops;
				case ZEND_ASSIGN_CONCAT:
					if (opline->extended_value) {
						return;
					}
				case ZEND_FAST_CONCAT:
				case ZEND_CONCAT:
					if (OP1_MAY_BE(MAY_BE_ARRAY|MAY_BE_OBJECT|MAY_BE_RESOURCE)) {
						return;
					}
					if (OP2_MAY_BE(MAY_BE_ARRAY|MAY_BE_OBJECT|MAY_BE_RESOURCE)) {
						return;
					}
					goto check_ops;
				case ZEND_IS_IDENTICAL:
				case ZEND_IS_NOT_IDENTICAL:
					goto check_ops;
				case ZEND_IS_EQUAL:
				case ZEND_IS_NOT_EQUAL:
				case ZEND_IS_SMALLER:
				case ZEND_IS_SMALLER_OR_EQUAL:
				case ZEND_CASE:
					if (OP1_MAY_BE(MAY_BE_OBJECT)) {
						return;
					}
					if (OP2_MAY_BE(MAY_BE_OBJECT)) {
						return;
					}
					goto check_ops;
				case ZEND_BW_NOT:
				case ZEND_BOOL_NOT:
				case ZEND_JMPZ:
				case ZEND_JMPNZ:
				case ZEND_JMPZNZ:
				case ZEND_BOOL:
					if (OP1_MAY_BE(MAY_BE_OBJECT)) {
						return;
					}
					goto check_op1;
				case ZEND_ECHO:
					if (OP1_MAY_BE(MAY_BE_OBJECT|MAY_BE_ARRAY)) {
						return;
					}
					goto check_op1;
				case ZEND_ROPE_INIT:
				case ZEND_ROPE_ADD:
				case ZEND_ROPE_END:
					if (OP2_MAY_BE(MAY_BE_OBJECT|MAY_BE_ARRAY)) {
						return;
					}
					goto check_op2;
				case ZEND_PRE_INC:
				case ZEND_PRE_DEC:
					/* TODO: string offset? */
					goto check_cv1;
				case ZEND_ASSIGN:
					/* TODO: stirng offset with negativ indeces */
					if (OP1_MAY_BE(MAY_BE_OBJECT)) {
						return;
					}
					if (OP1_MAY_BE(MAY_BE_RC1) &&
					    OP1_MAY_BE(MAY_BE_ARRAY_OF_ARRAY|MAY_BE_ARRAY_OF_OBJECT|MAY_BE_ARRAY_OF_RESOURCE|MAY_BE_OBJECT|MAY_BE_RESOURCE)) {
					    return;
					}
					goto check_cv2;
				case ZEND_RETURN:
					goto check_cv1;
				case ZEND_RECV:
					if (info->num_args < 0 || (int)opline->op1.num-1 >= info->num_args) {
						return;
					}
				case ZEND_RECV_INIT:
					if (op_array->arg_info && (int)opline->op1.num-1 < op_array->num_args) {
					    /* TODO: type check */
						if (op_array->arg_info[opline->op1.num-1].class_name) {
							return;
						} else if (op_array->arg_info[opline->op1.num-1].type_hint) {
							return;
						}
					}
					/* TODO: constant resolution may cause warning */
					if (opline->opcode == ZEND_RECV_INIT &&
					    (Z_TYPE_P(RT_CONSTANT(op_array, opline->op2)) == IS_CONSTANT ||
					     Z_TYPE_P(RT_CONSTANT(op_array, opline->op2)) == IS_CONSTANT_AST)) {
						return;
					}
					break;
				case ZEND_SEND_VAL_EX:
				case ZEND_SEND_VAR_EX:
					return;
				case ZEND_SEND_VAR:
					goto check_cv1;
				case ZEND_SEND_VAR_NO_REF:
					if ((opline->extended_value & (ZEND_ARG_COMPILE_TIME_BOUND|ZEND_ARG_SEND_BY_REF)) != ZEND_ARG_COMPILE_TIME_BOUND) {
						return;
					}
					goto check_cv1;
//???				case ZEND_SEND_REF:
//???					if (opline->extended_value == ZEND_DO_FCALL_BY_NAME) {
//???						return;
//???					}
//???					break;
				case ZEND_DO_FCALL:
				case ZEND_DO_ICALL:
				case ZEND_DO_UCALL:
				case ZEND_DO_FCALL_BY_NAME:
					{
						zend_call_info *call_info = info->callee_info;

						while (call_info && call_info->caller_call_opline != opline) {
							call_info = call_info->next_callee;
						}
						if (call_info->callee_func->type == ZEND_INTERNAL_FUNCTION) {
							if (zend_get_func_info(call_info, &info->ssa) & FUNC_MAY_WARN) {
								/* any warning in internal function causes symtab construction */
								return;
							}
						}
					}
					break;
				default:
					return;
					break;
check_ops:
					if (opline->op1_type == IS_CV &&
					    info->ssa.ops[i].op1_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op1_use].type & MAY_BE_UNDEF)) {
					    return;
					} else if (opline->op1_type == IS_TMP_VAR &&
					    info->ssa.ops[i].op1_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op1_use].type & (MAY_BE_ARRAY_OF_ARRAY|MAY_BE_ARRAY_OF_OBJECT|MAY_BE_ARRAY_OF_RESOURCE|MAY_BE_OBJECT|MAY_BE_RESOURCE))) {
					    return;
					} else if (opline->op1_type == IS_VAR &&
					    info->ssa.ops[i].op1_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op1_use].type & MAY_BE_RC1) &&
					    (info->ssa.var_info[info->ssa.ops[i].op1_use].type & (MAY_BE_ARRAY_OF_ARRAY|MAY_BE_ARRAY_OF_OBJECT|MAY_BE_ARRAY_OF_RESOURCE|MAY_BE_OBJECT|MAY_BE_RESOURCE))) {
					    return;
					}
check_op2:
					if (opline->op2_type == IS_TMP_VAR &&
					    info->ssa.ops[i].op2_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op2_use].type & (MAY_BE_ARRAY_OF_ARRAY|MAY_BE_ARRAY_OF_OBJECT|MAY_BE_ARRAY_OF_RESOURCE|MAY_BE_OBJECT|MAY_BE_RESOURCE))) {
					    return;
					} else if (opline->op2_type == IS_VAR &&
					    info->ssa.ops[i].op2_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op2_use].type & MAY_BE_RC1) &&
					    (info->ssa.var_info[info->ssa.ops[i].op2_use].type & (MAY_BE_ARRAY_OF_ARRAY|MAY_BE_ARRAY_OF_OBJECT|MAY_BE_ARRAY_OF_RESOURCE|MAY_BE_OBJECT|MAY_BE_RESOURCE))) {
					    return;
					}
check_cv2:
					if (opline->op2_type == IS_CV &&
					    info->ssa.ops[i].op2_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op2_use].type & MAY_BE_UNDEF)) {
					    return;
					}
					break;
check_op1:
					if (opline->op1_type == IS_TMP_VAR &&
					    info->ssa.ops[i].op1_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op1_use].type & (MAY_BE_ARRAY_OF_ARRAY|MAY_BE_ARRAY_OF_OBJECT|MAY_BE_ARRAY_OF_RESOURCE|MAY_BE_OBJECT|MAY_BE_RESOURCE))) {
					    return;
					} else if (opline->op1_type == IS_VAR &&
					    info->ssa.ops[i].op1_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op1_use].type & MAY_BE_RC1) &&
					    (info->ssa.var_info[info->ssa.ops[i].op1_use].type & (MAY_BE_ARRAY_OF_ARRAY|MAY_BE_ARRAY_OF_OBJECT|MAY_BE_ARRAY_OF_RESOURCE|MAY_BE_OBJECT|MAY_BE_RESOURCE))) {
					    return;
					}
check_cv1:
					if (opline->op1_type == IS_CV &&
					    info->ssa.ops[i].op1_use >= 0 &&
					    (info->ssa.var_info[info->ssa.ops[i].op1_use].type & MAY_BE_UNDEF)) {
					    return;
					}
					break;
			}
		}
	}
	info->flags |= ZEND_JIT_FUNC_NO_SYMTAB;
}

static zend_func_info* zend_jit_create_clone(zend_jit_context *ctx, zend_func_info *info)
{
	zend_func_info *clone;

	clone = zend_arena_calloc(&ctx->arena, 1, sizeof(zend_func_info));
	if (!clone) {
		return NULL;
	}
	memcpy(clone, info, sizeof(zend_func_info));
	/* TODO: support for multiple clones */
	clone->clone_num = 1;
	clone->ssa.var_info = zend_arena_calloc(&ctx->arena, info->ssa.vars_count, sizeof(zend_ssa_var_info));
	if (!clone->ssa.var_info) {
		return NULL;
	}
	memcpy(clone->ssa.var_info, info->ssa.var_info, sizeof(zend_ssa_var_info) * info->ssa.vars_count);
	return clone;
}

int zend_jit_is_return_value_used(zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	int used = -1;

	if (info->caller_info) {
		zend_call_info *call_info = info->caller_info;

		while (call_info) {
			if (used == -1) {
				used = !(call_info->caller_call_opline->result_type == IS_UNUSED);
			} else if (used == 0) {
				if (!(call_info->caller_call_opline->result_type == IS_UNUSED)) {
					return -1;
				}
			} else if (used > 0) {
				if (call_info->caller_call_opline->result_type == IS_UNUSED) {
					return -1;
				}
			}
			call_info = call_info->next_caller;
		}
	}
	return used;
}

static void zend_jit_check_no_frame(zend_jit_context *ctx, zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	int b, i;

	if (!info ||
	    !op_array->function_name ||
	    !info->ssa.var_info ||
	    !(info->flags & ZEND_JIT_FUNC_NO_IN_MEM_CVS) ||
	    !(info->flags & ZEND_JIT_FUNC_NO_SYMTAB) ||
	    info->num_args < op_array->num_args) {
		return;
	}
	if (info->flags & ZEND_JIT_FUNC_NO_FRAME) {
		return;
	}
	if (!info->clone_num && (info->clone || !info->caller_info)) {
		return;
	} 
	for (b = 0; b < info->ssa.cfg.blocks_count; b++) {
		if ((info->ssa.cfg.blocks[b].flags & ZEND_BB_REACHABLE) == 0) {
			continue;
		}
		for (i = info->ssa.cfg.blocks[b].start; i <= info->ssa.cfg.blocks[b].end; i++) {
			zend_op *opline = op_array->opcodes + i;
			if (!zend_opline_supports_jit(op_array, opline)) {
				return;
			} else if (opline->opcode == ZEND_DO_FCALL ||
			           opline->opcode == ZEND_DO_ICALL ||
			           opline->opcode == ZEND_DO_UCALL ||
			           opline->opcode == ZEND_DO_FCALL_BY_NAME) {
				zend_call_info *call_info = info->callee_info;

				while (call_info) {
					if (call_info->caller_call_opline == opline) {
						break;
					}
					call_info = call_info->next_callee;
				}
				if (!call_info) {
					return;
				} else if (call_info->callee_func->type == ZEND_USER_FUNCTION &&
				           &call_info->callee_func->op_array == op_array) {
					/* ignore directly recursive calls */
				} else if (call_info->callee_func->type == ZEND_USER_FUNCTION &&
				           call_info->clone &&
				           (call_info->clone->flags & ZEND_JIT_FUNC_NO_FRAME)) {
					/* ignore calls to other functions without frames */
					// FIXME: order of functions matters
				} else {
					return;
				}
			}
		}
	}
	/* only clones may be used without frames */
	if (info->clone_num) {
		info->flags |= ZEND_JIT_FUNC_NO_FRAME;
	} else if (!info->clone && info->caller_info) {
		zend_call_info *call_info;

		info->clone = zend_jit_create_clone(ctx, info);
		info->clone->return_value_used = zend_jit_is_return_value_used(op_array);
		if (info->num_args == 0 && !(info->flags & ZEND_FUNC_VARARG)) {
			info->clone->flags |= ZEND_JIT_FUNC_NO_USED_ARGS;			
		}
		info->clone->flags |= ZEND_JIT_FUNC_NO_FRAME;
		call_info = info->caller_info;
		while (call_info) {
//			if (call_info->num_args == info->num_args) {
				call_info->clone = info->clone;
//			}
			call_info = call_info->next_caller;
		}
	}
}

static void zend_jit_check_inlining(zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);

	if (info->caller_info &&
	    op_array->last_try_catch == 0 &&
	    (info->flags & (ZEND_FUNC_INDIRECT_VAR_ACCESS|ZEND_FUNC_RECURSIVE)) == 0 &&
	    !(op_array->fn_flags & ZEND_ACC_GENERATOR) &&
	    !(op_array->fn_flags & ZEND_ACC_RETURN_REFERENCE) &&
	    !op_array->last_try_catch &&
	    !info->callee_info) {

		if (info->clone) {
			info = info->clone;
		}
		if ((info->flags & ZEND_FUNC_NO_LOOPS) &&
		    (info->flags & ZEND_JIT_FUNC_NO_IN_MEM_CVS) &&
		    (info->flags & ZEND_JIT_FUNC_NO_SYMTAB) && 
		    (info->flags & ZEND_JIT_FUNC_NO_FRAME)) { 
			info->flags |= ZEND_JIT_FUNC_INLINE;
		}
	}
}

static void zend_jit_mark_reg_args(zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	int i;

	if (info->ssa.var_info &&
	    info->clone_num &&
	    info->num_args &&
	    !(info->flags & ZEND_JIT_FUNC_NO_USED_ARGS) &&
		!(info->flags & ZEND_FUNC_VARARG)) {
		for (i = 0; i < info->num_args; i++) {
			if (info->arg_info[i].ssa_var >= 0 &&
				(info->ssa.var_info[info->arg_info[i].ssa_var].type & (MAY_BE_ANY | MAY_BE_REF))) {
				if ((info->ssa.var_info[info->arg_info[i].ssa_var].type & MAY_BE_IN_REG)) {
					if (info->ssa.var_info[info->arg_info[i].ssa_var].type & (MAY_BE_LONG|MAY_BE_FALSE|MAY_BE_TRUE)) {
						info->arg_info[i].info.type |= MAY_BE_IN_REG;
						info->flags |= ZEND_JIT_FUNC_HAS_REG_ARGS;
#if defined(__GNUC__) && !defined(i386)
					} else if (info->ssa.var_info[info->arg_info[i].ssa_var].type & MAY_BE_DOUBLE) {
						info->arg_info[i].info.type |= MAY_BE_IN_REG;
						info->flags |= ZEND_JIT_FUNC_HAS_REG_ARGS;
#endif
                	}
//???				} else if ((info->ssa.var_info[info->arg_info[i].ssa_var].type & MAY_BE_TMP_ZVAL)) {
//???					if (!(info->ssa.var_info[info->arg_info[i].ssa_var].type & (MAY_BE_STRING|MAY_BE_ARRAY|MAY_BE_OBJECT|MAY_BE_RESOURCE))) {
//???						info->arg_info[i].info.type |= MAY_BE_TMP_ZVAL;
//???						info->flags |= ZEND_JIT_FUNC_HAS_REG_ARGS;
//???					}
				}
			}
		}
	}
}

void zend_jit_mark_tmp_zvals(zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	zend_call_info *call_info = info->callee_info;

	if (!info || !info->ssa.var_info) {
		return;
	}

	while (call_info) {
		if ((call_info->callee_func->type == ZEND_INTERNAL_FUNCTION ||
		     (call_info->clone &&
		      (call_info->clone->return_info.type & (MAY_BE_IN_REG/*???|MAY_BE_TMP_ZVAL*/)))) &&
		    info->ssa.ops[call_info->caller_call_opline - op_array->opcodes].result_def >= 0) {
			
			int var = info->ssa.ops[call_info->caller_call_opline - op_array->opcodes].result_def;

			if (call_info->callee_func->type == ZEND_INTERNAL_FUNCTION &&
			    (info->ssa.var_info[var].type & (MAY_BE_RCN|MAY_BE_REF))) {
				call_info = call_info->next_callee;
				continue;
			}
//???
#if 0
			if (!info->ssa.vars[var].phi_use_chain) {
				int may_be_tmp = 1;
				int use = info->ssa.vars[var].use_chain;

				while (use >= 0) {
					if (!zend_opline_supports_jit(op_array, op_array->opcodes + use)) {
						may_be_tmp = 0;
						break;
					}
					use = zend_ssa_next_use(info->ssa.ops, var, use);
				}

            	if (may_be_tmp) {
					info->ssa.var_info[var].type |= MAY_BE_TMP_ZVAL;
				}
			}
#endif
		}
		call_info = call_info->next_callee;
	}
}

int zend_jit_optimize_vars(zend_jit_context *ctx, zend_op_array *op_array)
{
//???	zend_jit_mark_tmp_zvals(op_array);

	zend_jit_mark_reg_zvals(op_array);

//???	zend_jit_check_no_used_args(op_array);

	zend_jit_check_no_symtab(op_array);

//???	zend_jit_check_no_frame(ctx, op_array);

//???	zend_jit_check_inlining(op_array);

//???	zend_jit_mark_reg_args(op_array);

	return SUCCESS;
}

static int zend_jit_collect_recv_arg_info(zend_jit_context *ctx, zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	zend_op *opline = op_array->opcodes;
	zend_op *end = opline + op_array->last;

	if (op_array->num_args == 0) {
		return SUCCESS;
	}

	info->arg_info = zend_arena_calloc(&ctx->arena, op_array->num_args, sizeof(*info->arg_info));

	while (opline < end) {
		if (opline->opcode == ZEND_RECV ||
		    opline->opcode == ZEND_RECV_INIT) {
			info->arg_info[opline->op1.num - 1].ssa_var =
				info->ssa.ops[opline - op_array->opcodes].result_def;
			info->arg_info[opline->op1.num - 1].info.type =
				(MAY_BE_ANY | MAY_BE_ARRAY_KEY_ANY | MAY_BE_ARRAY_OF_ANY | MAY_BE_ARRAY_OF_REF);
			if (opline->op1.num == op_array->num_args) {
				break;
			}
		}
		opline++;		    
	}

	return SUCCESS;
}

static void zend_jit_infer_return_types(zend_jit_context *ctx)
{
	zend_func_info *info;
	int i;
	int worklist_len;
	zend_bitset worklist, visited;
	zend_ssa_var_info tmp;
	zend_bitset *varlist;

	worklist_len = zend_bitset_len(ctx->call_graph.op_arrays_count);
	worklist = (zend_bitset)alloca(sizeof(zend_ulong) * worklist_len);
	memset(worklist, 0, sizeof(zend_ulong) * worklist_len);
	visited = (zend_bitset)alloca(sizeof(zend_ulong) * worklist_len);
	memset(visited, 0, sizeof(zend_ulong) * worklist_len);

	varlist = (zend_bitset*)alloca(sizeof(zend_bitset) * ctx->call_graph.op_arrays_count);
	memset(varlist, 0, sizeof(zend_bitset) * ctx->call_graph.op_arrays_count);

	for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
		info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);
		if (info) {
			zend_call_info *call_info = info->caller_info;

			varlist[i] = (zend_bitset)alloca(sizeof(zend_ulong) * zend_bitset_len(info->ssa.vars_count));
			memset(varlist[i], 0, sizeof(zend_ulong) * zend_bitset_len(info->ssa.vars_count));
			while (call_info) {
				zend_func_info *func_info = ZEND_FUNC_INFO(call_info->caller_op_array);
				if (func_info &&
				    func_info->num <= info->num &&
					func_info->ssa.ops &&
					func_info->ssa.ops[call_info->caller_call_opline - call_info->caller_op_array->opcodes].result_def >= 0) {
					zend_bitset_incl(worklist, info->num);
					break;
				}
				call_info = call_info->next_caller;
			}
		}
	}
	
	while (!zend_bitset_empty(worklist, worklist_len)) {
		i = zend_bitset_first(worklist, worklist_len);
		zend_bitset_excl(worklist, i);
		info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);

		/* perform incremental type inference */
		zend_infer_types_ex(ctx->call_graph.op_arrays[i], ctx->main_script, &info->ssa, varlist[i]);

		/* calculate return type */		
		zend_func_return_info(ctx->call_graph.op_arrays[i], ctx->main_script, 1, 0, &tmp);
		if (info->return_info.type != tmp.type ||
		    info->return_info.ce != tmp.ce ||
		    info->return_info.is_instanceof != tmp.is_instanceof ||
		    !zend_bitset_in(visited, i)) {
			zend_call_info *call_info = info->caller_info;

			zend_bitset_incl(visited, i);
			info->return_info.type = tmp.type;
			info->return_info.ce = tmp.ce;
			info->return_info.is_instanceof = tmp.is_instanceof;
			while (call_info) {
				zend_op_array *op_array = call_info->caller_op_array;
				zend_func_info *info = ZEND_FUNC_INFO(op_array);

				if (info && info->ssa.ops && info->ssa.ops[call_info->caller_call_opline - op_array->opcodes].result_def >= 0) {
					zend_bitset_incl(varlist[info->num], info->ssa.ops[call_info->caller_call_opline - op_array->opcodes].result_def);
					zend_bitset_incl(worklist, info->num);
				}
				call_info = call_info->next_caller;
			}
		}
	}
}

#define IP_COLLECT_DEP_EX(v) do { \
		int i = 0; \
		while (i < deps) { \
			if (dep[i] == v) { \
				break; \
			} \
			i++; \
		} \
		if (i == deps) { \
			dep[deps++] = v; \
		} \
	} while (0)

#define IP_COLLECT_DEP(_var) do { \
		if (!info->ssa.vars[_var].no_val) { \
			int v = xlat[i].vars + _var; \
			if (ip_var[v].kind == IP_NONE) { \
				zend_worklist_stack_push(&stack, v); \
				ip_var[v].kind = IP_VAR; \
				ip_var[v].op_array_num = i; \
				ip_var[v].num = _var; \
				ip_var[v].scc = -1; \
			} \
			IP_COLLECT_DEP_EX(v); \
		} \
	} while (0)

#define IP_MARK_SCC_ENTRY(var) do { \
		if (!info->ssa.vars[var].no_val) { \
			int v = xlat[i].vars + var; \
			if (ip_var[v].kind == IP_VAR && ip_var[v].scc >= 0) { \
				ip_var[v].scc_entry = 1; \
			} \
		} \
	} while (0);

typedef struct _zend_jit_ip_xlat {
	int ret;
	int args;
	int vars;
} zend_jit_ip_xlat;

typedef enum _zend_jit_ip_var_kind {
	IP_NONE,
	IP_VAR,
	IP_RET,
	IP_ARG
} zend_jit_ip_var_kind;

typedef struct _zend_jit_ip_var {
	zend_uchar            kind;         /* zend_jit_ip_var_kind */
	zend_uchar            scc_entry;
	int                   op_array_num;
	int                   num;
	int                   deps;
	int                  *dep;
	int                   scc;
	int                   next_scc_var;
} zend_jit_ip_var;

static void zend_jit_ip_check_scc_var(zend_jit_ip_var *ip_var, int var, int *sccs, int *index, int *dfs, int *root, zend_worklist_stack *stack)
{
	int j;

	dfs[var] = *index;
	(*index)++;
	root[var] = var;

	for (j = 0; j < ip_var[var].deps; j++) {
		int var2 = ip_var[var].dep[j];

		if (dfs[var2] < 0) {
			zend_jit_ip_check_scc_var(ip_var, var2, sccs, index, dfs, root, stack);
		}
		if (ip_var[var2].scc < 0 && dfs[root[var]] >= dfs[root[var2]]) {
		    root[var] = root[var2];
		}
	}		

	if (root[var] == var) {
		ip_var[var].scc = *sccs;
		while (stack->len > 0) {
			int var2 = zend_worklist_stack_peek(stack);
			if (dfs[var2] <= dfs[var]) {
				break;
			}
			zend_worklist_stack_pop(stack);
			ip_var[var2].scc = *sccs;
		}
		(*sccs)++;
	} else {
		zend_worklist_stack_push(stack, var);
	}
}

static int zend_jit_ip_find_sccs(zend_jit_ip_var *ip_var, int ip_vars)
{
	int index = 0, sccs = 0;
	int j, i;
	zend_worklist_stack stack;
	int *root = alloca(sizeof(int) * ip_vars);
	int *dfs = alloca(sizeof(int) * ip_vars);
	ALLOCA_FLAG(stack_use_heap);

	ZEND_WORKLIST_STACK_ALLOCA(&stack, ip_vars, stack_use_heap);
	memset(dfs, -1, sizeof(int) * ip_vars);

	/* Find SCCs */
	for (j = 0; j < ip_vars; j++) {
		if (ip_var[j].kind != IP_NONE) {
			if (dfs[j] < 0) {
				zend_jit_ip_check_scc_var(ip_var, j, &sccs, &index, dfs, root, &stack);
			}
		}
	}

	/* Revert SCC order */
	for (j = 0; j < ip_vars; j++) {
		if (ip_var[j].scc >= 0) {
			ip_var[j].scc = sccs - (ip_var[j].scc + 1);
		}
	}
	
	for (j = 0; j < ip_vars; j++) {
		if (ip_var[j].kind != IP_NONE && ip_var[j].scc >= 0) {
			if (root[j] == j) {
				ip_var[j].scc_entry = 1;
			}
			for (i = 0; i < ip_var[j].deps; i++) {			
				if (ip_var[ip_var[j].dep[i]].scc != ip_var[j].scc) {
					ip_var[ip_var[j].dep[i]].scc_entry = 1;
				}
			}
		}
	}

	ZEND_WORKLIST_STACK_FREE_ALLOCA(&stack, stack_use_heap);

	return sccs;
}

static void zend_jit_ip_find_vars(zend_jit_context *ctx,
                                  int               with_args,
                                  zend_jit_ip_var  *ip_var,
                                  int               ip_vars,
                                  zend_jit_ip_xlat *xlat)
{
	zend_op_array *op_array;
	zend_func_info *info, *caller_info;
	zend_call_info *call_info;
	int i;    /* op_array number */
	int j;    /* aegument or SSA variable number */
	int n;    /* ip variable number */
	int deps; /* number of dependent variables */
	zend_worklist_stack stack;
	int *dep = alloca(sizeof(int) * ip_vars);
	ALLOCA_FLAG(stack_use_heap);

	ZEND_WORKLIST_STACK_ALLOCA(&stack, ip_vars, stack_use_heap);
	if (with_args) {
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			op_array = ctx->call_graph.op_arrays[i];
			info = ZEND_FUNC_INFO(op_array);
			if (info && info->ssa.vars && info->num_args > 0 && info->caller_info) {
				if (info->num_args > 0) {
					int num_args = MIN(info->num_args, op_array->num_args);
					/* IP vars for arguments */
					for (j = 0; j < num_args; j++) {
						if (!info->ssa.vars[info->arg_info[j].ssa_var].no_val) {
							n = xlat[i].args + j;
							ip_var[n].kind = IP_ARG;
							ip_var[n].op_array_num = i;
							ip_var[n].num = j;
							ip_var[n].scc = -1;
							ip_var[n].deps = 1;
							ip_var[n].dep = zend_arena_calloc(&ctx->arena, 1, sizeof(int));
							ip_var[n].dep[0] = xlat[i].vars + info->arg_info[j].ssa_var;
							n = xlat[i].vars + info->arg_info[j].ssa_var;
							zend_worklist_stack_push(&stack, n);
							ip_var[n].kind = IP_VAR;
							ip_var[n].op_array_num = i;
							ip_var[n].num = info->arg_info[j].ssa_var;
							ip_var[n].scc = -1;
						}
					}
				}
			}
		}
	} else {
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			op_array = ctx->call_graph.op_arrays[i];
			info = ZEND_FUNC_INFO(op_array);
			if (info && info->ssa.vars && info->caller_info) {
				call_info = info->caller_info;
				do {
					caller_info = ZEND_FUNC_INFO(call_info->caller_op_array);
					if (caller_info &&
					    caller_info->num <= info->num && /* FIXME: it's analyzed before */
						caller_info->ssa.ops &&
						caller_info->ssa.ops[call_info->caller_call_opline - call_info->caller_op_array->opcodes].result_def >= 0) {

						n = xlat[i].ret;
						zend_worklist_stack_push(&stack, n);
						ip_var[n].kind = IP_RET;
						ip_var[n].op_array_num = i;
						ip_var[n].num = 0;
						ip_var[n].scc = -1;
						ip_var[n].deps = 0;
						break;
					}
					call_info = call_info->next_caller;
				} while (call_info);
			}
		}
	}
	while (stack.len) {
		n = zend_worklist_stack_pop(&stack);
		i = ip_var[n].op_array_num;
		op_array = ctx->call_graph.op_arrays[i];
		info = ZEND_FUNC_INFO(op_array);
		deps = 0;

		if (ip_var[n].kind == IP_RET) {
			call_info = info->caller_info;
			while (call_info) {
				caller_info = ZEND_FUNC_INFO(call_info->caller_op_array);

				if (caller_info &&
				    caller_info->ssa.ops &&
				    caller_info->ssa.ops[call_info->caller_call_opline - call_info->caller_op_array->opcodes].result_def >= 0) {

				    int v = xlat[caller_info->num].vars + caller_info->ssa.ops[call_info->caller_call_opline - call_info->caller_op_array->opcodes].result_def;

					if (ip_var[v].kind != IP_VAR) {
						ip_var[v].kind = IP_VAR;
						ip_var[v].op_array_num = caller_info->num;
						ip_var[v].num = caller_info->ssa.ops[call_info->caller_call_opline - call_info->caller_op_array->opcodes].result_def;
						ip_var[v].scc = -1;
						zend_worklist_stack_push(&stack, v);
					}
					IP_COLLECT_DEP_EX(v);
				}

				call_info = call_info->next_caller;
			}
		} else if (ip_var[n].kind == IP_VAR) { 
			zend_ssa_phi *p;
			int use;

			j = ip_var[n].num;
			FOR_EACH_VAR_USAGE(j, IP_COLLECT_DEP);

#ifdef SYM_RANGE
			/* Process symbolic control-flow constraints */
			p = info->ssa.vars[j].sym_use_chain;
			while (p) {
				IP_COLLECT_DEP(p->ssa_var);
				p = p->sym_use_chain;
			}
#endif

			use = info->ssa.vars[j].use_chain;
			while (use >= 0) {
				switch (op_array->opcodes[use].opcode) {
					case ZEND_RETURN:
					case ZEND_RETURN_BY_REF:					
						if (info->caller_info && 
						    info->ssa.ops[use].op1_use >= 0) {
							int v = xlat[i].ret;

							if (ip_var[v].kind != IP_RET) {
								ip_var[v].kind = IP_RET;
								ip_var[v].op_array_num = i;
								ip_var[v].num = 0;
								ip_var[v].scc = -1;
								ip_var[v].deps = 0;
								zend_worklist_stack_push(&stack, v);
							}
							IP_COLLECT_DEP_EX(v);
						}
						break;
					case ZEND_SEND_VAL:
					case ZEND_SEND_VAR:
					case ZEND_SEND_REF:
					case ZEND_SEND_VAR_NO_REF:
//???
					case ZEND_SEND_VAL_EX:
					case ZEND_SEND_VAR_EX:
						if (with_args && info->ssa.ops[use].op1_use >= 0) {
							call_info = info->callee_info;
							while (call_info) {
								if (call_info->callee_func->type == ZEND_USER_FUNCTION &&
								    op_array->opcodes[use].op2.num - 1 < call_info->num_args &&
								    call_info->arg_info[op_array->opcodes[use].op2.num - 1].opline == op_array->opcodes + use &&
								    ZEND_FUNC_INFO(&call_info->callee_func->op_array)->num_args > 0 &&
								    ip_var[xlat[ZEND_FUNC_INFO(&call_info->callee_func->op_array)->num].args + op_array->opcodes[use].op2.num - 1].kind == IP_ARG) {
									IP_COLLECT_DEP_EX(xlat[ZEND_FUNC_INFO(&call_info->callee_func->op_array)->num].args + op_array->opcodes[use].op2.num - 1);
									break;
								}
								call_info = call_info->next_callee;
							}
						}
						break;
					default:
						break;
				}
				use = zend_ssa_next_use(info->ssa.ops, j, use);
			}
		}

		ip_var[n].deps = deps;
		if (deps) {
			ip_var[n].dep = zend_arena_calloc(&ctx->arena, deps, sizeof(int));
			memcpy(ip_var[n].dep, dep, sizeof(int) * deps);
		}
	}

	ZEND_WORKLIST_STACK_FREE_ALLOCA(&stack, stack_use_heap);
}

#ifdef NEG_RANGE
static int zend_jit_ip_check_inner_cycles(zend_jit_context *ctx, zend_jit_ip_var *ip_var, zend_bitset worklist, zend_bitset visited, int var)
{
	int i;

	if (zend_bitset_in(worklist, var)) {
		return 1;
	}
	zend_bitset_incl(worklist, var);
	for (i = 0; i < ip_var[var].deps; i++) {
		if (ip_var[ip_var[var].dep[i]].scc == ip_var[var].scc) {
			zend_op_array *op_array = ctx->call_graph.op_arrays[ip_var[ip_var[var].dep[i]].op_array_num];
			zend_func_info *info = ZEND_FUNC_INFO(op_array);

			if (ip_var[ip_var[var].dep[i]].kind == IP_VAR &&
			    info->ssa.vars[ip_var[ip_var[var].dep[i]].num].definition_phi &&
			    info->ssa.vars[ip_var[ip_var[var].dep[i]].num].definition_phi->pi >= 0 &&
			    info->ssa.vars[ip_var[ip_var[var].dep[i]].num].definition_phi->sources[0] != ip_var[var].num) {
				/* Don't process symbolic dependencies */
			    continue;									
			}
			if (!ip_var[ip_var[var].dep[i]].scc_entry &&
			    !zend_bitset_in(visited, ip_var[var].dep[i]) &&
    		    zend_jit_ip_check_inner_cycles(ctx, ip_var, worklist, visited, ip_var[var].dep[i])) {
				return 1;
			}
		}
	}
	zend_bitset_incl(visited, var);
	return 0;
}
#endif

static void zend_jit_ip_infer_ranges_warmup(zend_jit_context *ctx, zend_jit_ip_var *ip_var, int ip_vars, int *scc, int scc_num)
{
	int worklist_len = zend_bitset_len(ip_vars);
	zend_bitset worklist = alloca(sizeof(zend_ulong) * worklist_len);
	zend_bitset visited = alloca(sizeof(zend_ulong) * worklist_len);
	int j, n;
#ifdef NEG_RANGE
	int has_inner_cycles = 0;
	
	memset(worklist, 0, sizeof(zend_ulong) * worklist_len);
	memset(visited, 0, sizeof(zend_ulong) * worklist_len);
	for (j = scc[scc_num]; j >= 0; j = ip_var[j].next_scc_var) {
		if (!zend_bitset_in(visited, j) &&
		    zend_jit_ip_check_inner_cycles(ctx, ip_var, worklist, visited, j)) {
			has_inner_cycles = 1;
			break;
		}
	}
#endif

	memset(worklist, 0, sizeof(zend_ulong) * worklist_len);

	for (n = 0; n < RANGE_WARMAP_PASSES; n++) {
		j = scc[scc_num];
		while (j >= 0) {
			if (ip_var[j].scc_entry) {
				zend_bitset_incl(worklist, j);
			}
			j = ip_var[j].next_scc_var;
		}

		memset(visited, 0, sizeof(zend_ulong) * worklist_len);

		while (!zend_bitset_empty(worklist, worklist_len)) {
			zend_op_array *op_array;
			zend_func_info *info;

			j = zend_bitset_first(worklist, worklist_len);
			zend_bitset_excl(worklist, j);
			op_array = ctx->call_graph.op_arrays[ip_var[j].op_array_num];
			info = ZEND_FUNC_INFO(op_array);
			if (ip_var[j].kind == IP_VAR) {
				zend_ssa_range tmp;

				if (zend_inference_calc_range(op_array, &info->ssa, ip_var[j].num, 0, 0, &tmp)) {
#ifdef NEG_RANGE
					if (!has_inner_cycles &&
					    info->ssa.var_info[ip_var[j].num].has_range &&
					    info->ssa.vars[ip_var[j].num].definition_phi &&
					    info->ssa.vars[ip_var[j].num].definition_phi->pi >= 0 &&
					    info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative &&
					    info->ssa.vars[ip_var[j].num].definition_phi->constraint.min_ssa_var < 0 &&
					    info->ssa.vars[ip_var[j].num].definition_phi->constraint.min_ssa_var < 0) {
						if (tmp.min == info->ssa.var_info[ip_var[j].num].range.min &&
						    tmp.max == info->ssa.var_info[ip_var[j].num].range.max) {
							if (info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative == NEG_INIT) {
#ifdef LOG_NEG_RANGE
								fprintf(stderr, "#%d INVARIANT\n", j);
#endif
								info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative = NEG_INVARIANT;
							}
						} else if (tmp.min == info->ssa.var_info[ip_var[j].num].range.min &&
						           tmp.max == info->ssa.var_info[ip_var[j].num].range.max + 1 &&
						           tmp.max < info->ssa.vars[ip_var[j].num].definition_phi->constraint.range.min) {
							if (info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative == NEG_INIT ||
							    info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative == NEG_INVARIANT) {
#ifdef LOG_NEG_RANGE
								fprintf(stderr, "#%d LT\n", j);
#endif
								info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative = NEG_USE_LT;
//???NEG
							} else if (info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative == NEG_USE_GT) {
#ifdef LOG_NEG_RANGE
								fprintf(stderr, "#%d UNKNOWN\n", j);
#endif
								info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative = NEG_UNKNOWN;
							}
						} else if (tmp.max == info->ssa.var_info[ip_var[j].num].range.max &&
						           tmp.min == info->ssa.var_info[ip_var[j].num].range.min - 1 &&
						           tmp.min > info->ssa.vars[ip_var[j].num].definition_phi->constraint.range.max) {
							if (info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative == NEG_INIT ||
							    info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative == NEG_INVARIANT) {
#ifdef LOG_NEG_RANGE
								fprintf(stderr, "#%d GT\n", j);
#endif
								info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative = NEG_USE_GT;
//???NEG
							} else if (info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative == NEG_USE_LT) {
#ifdef LOG_NEG_RANGE
								fprintf(stderr, "#%d UNKNOWN\n", j);
#endif
								info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative = NEG_UNKNOWN;
							}
						} else {
#ifdef LOG_NEG_RANGE
							fprintf(stderr, "#%d UNKNOWN\n", j);
#endif
							info->ssa.vars[ip_var[j].num].definition_phi->constraint.negative = NEG_UNKNOWN;
						}
					}
#endif
					if (zend_inference_narrowing_meet(&info->ssa.var_info[ip_var[j].num], &tmp)) {
						int i;

						zend_bitset_incl(visited, j);
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								if (ip_var[ip_var[j].dep[i]].kind == IP_VAR &&
								    info->ssa.vars[ip_var[ip_var[j].dep[i]].num].definition_phi &&
								    info->ssa.vars[ip_var[ip_var[j].dep[i]].num].definition_phi->pi >= 0 &&
								    info->ssa.vars[ip_var[ip_var[j].dep[i]].num].definition_phi->sources[0] != ip_var[j].num) {
									/* Don't process symbolic dependencies during widening */
								    continue;									
								}
								if (!zend_bitset_in(visited, ip_var[j].dep[i])) {
									zend_bitset_incl(worklist, ip_var[j].dep[i]);
								}
							}
						}
					}
				}
			} else if (ip_var[j].kind == IP_RET) {
				zend_ssa_var_info tmp;

				zend_func_return_info(op_array, ctx->main_script, 1, 1, &tmp);
				if (tmp.has_range) {
					if (zend_inference_widening_meet(&info->return_info, &tmp.range)) {
						int i;

						zend_bitset_incl(visited, j);
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								if (!zend_bitset_in(visited, ip_var[j].dep[i])) {
									zend_bitset_incl(worklist, ip_var[j].dep[i]);
								}
							}
						}
					}
				}
			} else if (ip_var[j].kind == IP_ARG) {
				zend_ssa_var_info tmp;

				zend_jit_func_arg_info(ctx, op_array, ip_var[j].num, 1, 1, &tmp);
				if (tmp.has_range) {
					if (zend_inference_narrowing_meet(&info->arg_info[ip_var[j].num].info, &tmp.range)) {
						int i;

						zend_bitset_incl(visited, j);
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								if (!zend_bitset_in(visited, ip_var[j].dep[i])) {
									zend_bitset_incl(worklist, ip_var[j].dep[i]);
								}
							}
						}
					}
				}
			}
		}
	}
}

static void zend_jit_ip_infer_ranges(zend_jit_context *ctx, int with_args)
{
	zend_op_array *op_array;
	zend_func_info *info;
	int i, j, ip_vars, sccs;
	zend_jit_ip_var *ip_var;
	zend_jit_ip_xlat *xlat;
	int *scc;
	int worklist_len;
	zend_bitset worklist;

	/* calculate maximum possible number of variables involved into IP graph */
	ip_vars = 0;
	xlat = alloca(sizeof(zend_jit_ip_xlat) * ctx->call_graph.op_arrays_count);
	if (with_args) {
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			op_array = ctx->call_graph.op_arrays[i];
			info = ZEND_FUNC_INFO(op_array);
			if (info && info->ssa.vars && ((info->num_args > 0 && info->caller_info) || info->callee_info)) {
				if (info->caller_info) {
					xlat[i].ret = ip_vars;
					ip_vars += 1; // for return value
				}
				if (info->num_args > 0) {
					xlat[i].args = ip_vars;
					ip_vars += MIN(info->num_args, op_array->num_args);
				}
				xlat[i].vars = ip_vars;
				ip_vars += info->ssa.vars_count;
			}
		}
	} else {
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			op_array = ctx->call_graph.op_arrays[i];
			info = ZEND_FUNC_INFO(op_array);
			if (info && info->ssa.vars && (info->caller_info || info->callee_info)) {
				if (info->caller_info) {
					xlat[i].ret = ip_vars;
					ip_vars += 1; // for return value
				}
				xlat[i].vars = ip_vars;
				ip_vars += info->ssa.vars_count;
			}
		}
	}

	/* Collect IP variables into single array */
	ip_var = zend_arena_calloc(&ctx->arena, ip_vars, sizeof(zend_jit_ip_var));
	zend_jit_ip_find_vars(ctx, with_args, ip_var, ip_vars, xlat);

	/* Find Strongly Connected Components */
	sccs = zend_jit_ip_find_sccs(ip_var, ip_vars);
	scc = alloca(sizeof(int) * sccs);
	memset(scc, -1, sizeof(int) * sccs);
	for (j = 0; j < ip_vars; j++) {
		if (ip_var[j].kind == IP_VAR && !ip_var[j].scc_entry) {
			/* check if this variable depended on SSA variables not included into IP graph */
			op_array = ctx->call_graph.op_arrays[ip_var[j].op_array_num];
			info = ZEND_FUNC_INFO(op_array);
			
			if (info->ssa.vars[ip_var[j].num].definition_phi) {
				zend_ssa_phi *p = info->ssa.vars[ip_var[j].num].definition_phi;

				if (p->pi >= 0) {
					if (!info->ssa.vars[p->sources[0]].no_val &&
					    ip_var[xlat[info->num].vars + p->sources[0]].kind == IP_NONE) {
						ip_var[j].scc_entry = 1;
					}
				} else {
					for (i = 0; i < info->ssa.cfg.blocks[p->block].predecessors_count; i++) {
						if (p->sources[i] >= 0 &&
						    !info->ssa.vars[p->sources[i]].no_val &&
						    ip_var[xlat[info->num].vars + p->sources[i]].kind == IP_NONE) {
							ip_var[j].scc_entry = 1;
							break;
						}
					}
				}				
			} else if (info->ssa.vars[ip_var[j].num].definition >= 0) {
				int line = info->ssa.vars[ip_var[j].num].definition;
				if (info->ssa.ops[line].op1_use >= 0 &&
				    !info->ssa.vars[info->ssa.ops[line].op1_use].no_val &&
				    ip_var[xlat[info->num].vars + info->ssa.ops[line].op1_use].kind == IP_NONE) {
					ip_var[j].scc_entry = 1;
				} else if (info->ssa.ops[line].op2_use >= 0 &&
				    !info->ssa.vars[info->ssa.ops[line].op2_use].no_val &&
				    ip_var[xlat[info->num].vars + info->ssa.ops[line].op2_use].kind == IP_NONE) {
					ip_var[j].scc_entry = 1;
				} else if (info->ssa.ops[line].result_use >= 0 &&
				    !info->ssa.vars[info->ssa.ops[line].result_use].no_val &&
				    ip_var[xlat[info->num].vars + info->ssa.ops[line].result_use].kind == IP_NONE) {
					ip_var[j].scc_entry = 1;
				} else if (op_array->opcodes[line].opcode == ZEND_OP_DATA) {
					line--;
					if (info->ssa.ops[line].op1_use >= 0 &&
					    !info->ssa.vars[info->ssa.ops[line].op1_use].no_val &&
					    ip_var[xlat[info->num].vars + info->ssa.ops[line].op1_use].kind == IP_NONE) {
						ip_var[j].scc_entry = 1;
					} else if (info->ssa.ops[line].op2_use >= 0 &&
				    	!info->ssa.vars[info->ssa.ops[line].op2_use].no_val &&
					    ip_var[xlat[info->num].vars + info->ssa.ops[line].op2_use].kind == IP_NONE) {
						ip_var[j].scc_entry = 1;
					} else if (info->ssa.ops[line].result_use >= 0 &&
					    !info->ssa.vars[info->ssa.ops[line].result_use].no_val &&
					    ip_var[xlat[info->num].vars + info->ssa.ops[line].result_use].kind == IP_NONE) {
						ip_var[j].scc_entry = 1;
					}
				} else if (line + 1 < op_array->last &&
				           op_array->opcodes[line + 1].opcode == ZEND_OP_DATA) {
					line++;
					if (info->ssa.ops[line].op1_use >= 0 &&
					    !info->ssa.vars[info->ssa.ops[line].op1_use].no_val &&
					    ip_var[xlat[info->num].vars + info->ssa.ops[line].op1_use].kind == IP_NONE) {
						ip_var[j].scc_entry = 1;
					} else if (info->ssa.ops[line].op2_use >= 0 &&
				    	!info->ssa.vars[info->ssa.ops[line].op2_use].no_val &&
					    ip_var[xlat[info->num].vars + info->ssa.ops[line].op2_use].kind == IP_NONE) {
						ip_var[j].scc_entry = 1;
					} else if (info->ssa.ops[line].result_use >= 0 &&
					    !info->ssa.vars[info->ssa.ops[line].result_use].no_val &&
					    ip_var[xlat[info->num].vars + info->ssa.ops[line].result_use].kind == IP_NONE) {
						ip_var[j].scc_entry = 1;
					}
				}
			}
		}
		if (ip_var[j].scc >= 0) {
			ip_var[j].next_scc_var = scc[ip_var[j].scc];
			scc[ip_var[j].scc] = j;
			op_array = ctx->call_graph.op_arrays[ip_var[j].op_array_num];
			info = ZEND_FUNC_INFO(op_array);
			if (ip_var[j].kind == IP_VAR) {
				info->ssa.var_info[ip_var[j].num].has_range = 0;				
			} else if (ip_var[j].kind == IP_RET) {
				info->return_info.has_range = 0;				
			} else if (ip_var[j].kind == IP_ARG) {
				info->arg_info[ip_var[j].num].info.has_range = 0;				
			}
		}
	}

	worklist_len = zend_bitset_len(ip_vars);
	worklist = alloca(sizeof(zend_ulong) * worklist_len);

	/* Walk over SCCs sorted in topological order */
	for (i = 0; i < sccs; i++) {
		j = scc[i];

		if (ip_var[j].next_scc_var < 0) {
			/* SCC with a single variable */
			op_array = ctx->call_graph.op_arrays[ip_var[j].op_array_num];
			info = ZEND_FUNC_INFO(op_array);
			
			if (ip_var[j].kind == IP_VAR) {
				zend_ssa_range tmp;

				if (zend_inference_calc_range(op_array, &info->ssa, ip_var[j].num, 0, 1, &tmp)) {
					zend_inference_init_range(op_array, &info->ssa, ip_var[j].num, tmp.underflow, tmp.min, tmp.max, tmp.overflow);
				} else {
					zend_inference_init_range(op_array, &info->ssa, ip_var[j].num, 1, LONG_MIN, LONG_MAX, 1);
				}
			} else if (ip_var[j].kind == IP_RET) {
				zend_ssa_var_info tmp;

				zend_func_return_info(op_array, ctx->main_script, 1, 0, &tmp);
				if (tmp.has_range) {
					info->return_info.has_range = 1;
					info->return_info.range = tmp.range;
				} else {
					info->return_info.has_range = 1;
					info->return_info.range.underflow = 1;
					info->return_info.range.min = LONG_MIN;
					info->return_info.range.max = LONG_MAX;
					info->return_info.range.overflow = 1;
				}
			} else if (ip_var[j].kind == IP_ARG) {
				zend_ssa_var_info tmp;

				zend_jit_func_arg_info(ctx, op_array, ip_var[j].num, 1, 0, &tmp);
				if (tmp.has_range) {
					info->arg_info[ip_var[j].num].info.has_range = 1;
					info->arg_info[ip_var[j].num].info.range = tmp.range;
				} else {
					info->arg_info[ip_var[j].num].info.has_range = 1;
					info->arg_info[ip_var[j].num].info.range.underflow = 1;
					info->arg_info[ip_var[j].num].info.range.min = LONG_MIN;
					info->arg_info[ip_var[j].num].info.range.max = LONG_MAX;
					info->arg_info[ip_var[j].num].info.range.overflow = 1;
				}
			}
			continue;
		}

		/* Start from SCC entry points */
		memset(worklist, 0, sizeof(zend_ulong) * worklist_len);
		do {
			if (ip_var[j].scc_entry) {
				zend_bitset_incl(worklist, j);
			}
			j = ip_var[j].next_scc_var;
		} while (j >= 0);

#if RANGE_WARMAP_PASSES > 0
		zend_jit_ip_infer_ranges_warmup(ctx, ip_var, ip_vars, scc, i);
		for (j = scc[i]; j >= 0; j = ip_var[j].next_scc_var) {
			zend_bitset_incl(worklist, j);
		}
#endif

		/* widening */
		while (!zend_bitset_empty(worklist, worklist_len)) {
			j = zend_bitset_first(worklist, worklist_len);
			zend_bitset_excl(worklist, j);
			op_array = ctx->call_graph.op_arrays[ip_var[j].op_array_num];
			info = ZEND_FUNC_INFO(op_array);

			if (ip_var[j].kind == IP_VAR) {
				zend_ssa_range tmp;

				if (zend_inference_calc_range(op_array, &info->ssa, ip_var[j].num, 1, 0, &tmp)) {
					if (zend_inference_widening_meet(&info->ssa.var_info[ip_var[j].num], &tmp)) {
						int i;
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								if (ip_var[ip_var[j].dep[i]].kind == IP_VAR &&
								    info->ssa.vars[ip_var[ip_var[j].dep[i]].num].definition_phi &&
								    info->ssa.vars[ip_var[ip_var[j].dep[i]].num].definition_phi->pi >= 0 &&
								    info->ssa.vars[ip_var[ip_var[j].dep[i]].num].definition_phi->sources[0] != ip_var[j].num) {
									/* Don't process symbolic dependencies during widening */
								    continue;									
								}
								zend_bitset_incl(worklist, ip_var[j].dep[i]);
							}
						}
					}
				}
			} else if (ip_var[j].kind == IP_RET) {
				zend_ssa_var_info tmp;

				zend_func_return_info(op_array, ctx->main_script, 1, 1, &tmp);
				if (tmp.has_range) {
					if (zend_inference_widening_meet(&info->return_info, &tmp.range)) {
						int i;
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								zend_bitset_incl(worklist, ip_var[j].dep[i]);
							}
						}
					}
				}
			} else if (ip_var[j].kind == IP_ARG) {
				zend_ssa_var_info tmp;

				zend_jit_func_arg_info(ctx, op_array, ip_var[j].num, 1, 1, &tmp);
				if (tmp.has_range) {
					if (zend_inference_widening_meet(&info->arg_info[ip_var[j].num].info, &tmp.range)) {
						int i;
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								zend_bitset_incl(worklist, ip_var[j].dep[i]);
							}
						}
					}
				}
			}
		}

		/* Add all SCC entry variables into worklist for narrowing */
		for (j = scc[i]; j >= 0; j = ip_var[j].next_scc_var) {
			op_array = ctx->call_graph.op_arrays[ip_var[j].op_array_num];
			info = ZEND_FUNC_INFO(op_array);

			if (ip_var[j].kind == IP_VAR) {
				if (!info->ssa.var_info[ip_var[j].num].has_range) {
					zend_inference_init_range(op_array, &info->ssa, ip_var[j].num, 1, LONG_MIN, LONG_MAX, 1);
				}
			} else if (ip_var[j].kind == IP_RET) {
				if (!info->return_info.has_range) {
					info->return_info.has_range = 1;
					info->return_info.range.underflow = 1;
					info->return_info.range.min = LONG_MIN;
					info->return_info.range.max = LONG_MAX;
					info->return_info.range.overflow = 1;
				}
			} else if (ip_var[j].kind == IP_ARG) {
				if (!info->arg_info[ip_var[j].num].info.has_range) {
					info->arg_info[ip_var[j].num].info.has_range = 1;
					info->arg_info[ip_var[j].num].info.range.underflow = 1;
					info->arg_info[ip_var[j].num].info.range.min = LONG_MIN;
					info->arg_info[ip_var[j].num].info.range.max = LONG_MAX;
					info->arg_info[ip_var[j].num].info.range.overflow = 1;
				}
			}
			zend_bitset_incl(worklist, j);
		}

		/* narrowing */
		while (!zend_bitset_empty(worklist, worklist_len)) {
			j = zend_bitset_first(worklist, worklist_len);
			zend_bitset_excl(worklist, j);
			op_array = ctx->call_graph.op_arrays[ip_var[j].op_array_num];
			info = ZEND_FUNC_INFO(op_array);

			if (ip_var[j].kind == IP_VAR) {
				zend_ssa_range tmp;

				if (zend_inference_calc_range(op_array, &info->ssa, ip_var[j].num, 0, 1, &tmp)) {
					if (zend_inference_narrowing_meet(&info->ssa.var_info[ip_var[j].num], &tmp)) {
						int i;
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								zend_bitset_incl(worklist, ip_var[j].dep[i]);
							}
						}
					}
				}
			} else if (ip_var[j].kind == IP_RET) {
				zend_ssa_var_info tmp;

				zend_func_return_info(op_array, ctx->main_script, 1, 0, &tmp);
				if (tmp.has_range) {
					if (zend_inference_narrowing_meet(&info->return_info, &tmp.range)) {
						int i;
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								zend_bitset_incl(worklist, ip_var[j].dep[i]);
							}
						}
					}
				}
			} else if (ip_var[j].kind == IP_ARG) {
				zend_ssa_var_info tmp;

				zend_jit_func_arg_info(ctx, op_array, ip_var[j].num, 1, 0, &tmp);
				if (tmp.has_range) {
					if (zend_inference_narrowing_meet(&info->arg_info[ip_var[j].num].info, &tmp.range)) {
						int i;
						for (i = 0; i < ip_var[j].deps; i++) {
							if (ip_var[ip_var[j].dep[i]].scc == ip_var[j].scc) {
								zend_bitset_incl(worklist, ip_var[j].dep[i]);
							}
						}
					}
				}
			}
		}
	}

	if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_TYPES) {
		fprintf(stderr, "Interprocedure Variable\n");
		for (i = 0; i < ip_vars; i++) {
			if (ip_var[i].kind == IP_NONE) {
				continue;
			}
			fprintf(stderr, "  %4d: %2d %s ", i, ip_var[i].op_array_num, ctx->call_graph.op_arrays[ip_var[i].op_array_num]->function_name ? ctx->call_graph.op_arrays[ip_var[i].op_array_num]->function_name->val : "$main");
			if (ip_var[i].kind == IP_VAR) {
				fprintf(stderr, "#%d.", ip_var[i].num);
				zend_dump_var(ctx->call_graph.op_arrays[ip_var[i].op_array_num], IS_CV, ZEND_FUNC_INFO(ctx->call_graph.op_arrays[ip_var[i].op_array_num])->ssa.vars[ip_var[i].num].var);
			} else if (ip_var[i].kind == IP_RET) {
				fprintf(stderr, "RET");
			} else if (ip_var[i].kind == IP_ARG) {
				fprintf(stderr, "ARG %d", ip_var[i].num);
			}
			if (ip_var[i].scc >= 0) {
				if (ip_var[i].scc_entry) {
					fprintf(stderr, " *");
				} else {
					fprintf(stderr, "  ");
				}
				fprintf(stderr, "SCC=%d;", ip_var[i].scc);
			}
			if (ip_var[i].deps) {
				fprintf(stderr, " (");
				for (j = 0; j < ip_var[i].deps; j++) {
					if (j != 0) {
						fprintf(stderr, ",");
					}
					fprintf(stderr, "%d", ip_var[i].dep[j]);
				}
				fprintf(stderr, ")");
			}
			fprintf(stderr, "\n");
    	}
		fprintf(stderr, "\n");
	}
#if 0
	j = 0;
	int deps = 0;
	for (i = 0; i < ip_vars; i++) {
		if (ip_var[i].kind != IP_NONE) {
			j++;
		}
   		deps += ip_var[i].deps;
	}
	fprintf(stderr, "IP_VARs=%d/%d, IP_DEPS=%d IP_SCCs=%d\n\n", ip_vars, j, deps, sccs);
#endif
}

static void zend_jit_infer_arg_and_return_types(zend_jit_context *ctx, int recursive)
{
	zend_func_info *info;
	int i, j;
	int worklist_len;
	zend_bitset worklist, visited;
	zend_ssa_var_info tmp;
	zend_bitset *varlist;

	worklist_len = zend_bitset_len(ctx->call_graph.op_arrays_count);
	worklist = (zend_bitset)alloca(sizeof(zend_ulong) * worklist_len);
	memset(worklist, 0, sizeof(zend_ulong) * worklist_len);
	visited = (zend_bitset)alloca(sizeof(zend_ulong) * worklist_len);
	memset(visited, 0, sizeof(zend_ulong) * worklist_len);

	varlist = (zend_bitset*)alloca(sizeof(zend_bitset) * ctx->call_graph.op_arrays_count);
	memset(varlist, 0, sizeof(zend_bitset) * ctx->call_graph.op_arrays_count);

	for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
		info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);
		varlist[i] = (zend_bitset)alloca(sizeof(zend_ulong) * zend_bitset_len(info->ssa.vars_count));
		memset(varlist[i], 0, sizeof(zend_ulong) * zend_bitset_len(info->ssa.vars_count));
		if (info && info->caller_info &&
		    (!recursive || (info->flags & ZEND_FUNC_RECURSIVE))) {
			zend_bitset_incl(worklist, info->num);
		}
	}
	
	/* infer return types */
	while (!zend_bitset_empty(worklist, worklist_len)) {
		i = zend_bitset_first(worklist, worklist_len);
		zend_bitset_excl(worklist, i);
		info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);

		/* calculate argument types */
		if (info && info->ssa.vars && info->num_args > 0 && info->caller_info) {
			int num_args = MIN(info->num_args, ctx->call_graph.op_arrays[i]->num_args);

			for (j = 0; j < num_args; j++) {
				zend_ssa_var_info tmp;

				zend_jit_func_arg_info(ctx, ctx->call_graph.op_arrays[i], j, recursive, 0, &tmp);
				if (tmp.type != 0) {
					if (tmp.type != info->arg_info[j].info.type ||
					    tmp.ce != info->arg_info[j].info.ce ||
					    tmp.is_instanceof != info->arg_info[j].info.is_instanceof) {
						info->arg_info[j].info.type = tmp.type;
						info->arg_info[j].info.ce = tmp.ce;
						info->arg_info[j].info.is_instanceof = tmp.is_instanceof;
						zend_bitset_incl(varlist[i], info->arg_info[j].ssa_var);
					}
				}
			}
		}
		
		if (zend_bitset_empty(varlist[i], zend_bitset_len(info->ssa.vars_count))) {
			zend_func_return_info(ctx->call_graph.op_arrays[i], ctx->main_script, recursive, 0, &tmp);
		} else {		
			/* perform incremental type inference */
			zend_infer_types_ex(ctx->call_graph.op_arrays[i], ctx->main_script, &info->ssa, varlist[i]);
			zend_func_return_info(ctx->call_graph.op_arrays[i], ctx->main_script, recursive, 0, &tmp);

			/* check if this function calls others */
			if (info->callee_info) {
				zend_call_info *call_info = info->callee_info;

				while (call_info) {
					if (call_info->recursive <= recursive) {
						if (call_info->callee_func->type == ZEND_USER_FUNCTION) {
							zend_bitset_incl(worklist, ZEND_FUNC_INFO(&call_info->callee_func->op_array)->num);
						}
					}
					call_info = call_info->next_callee;
				}
			}
		}

		if (info->return_info.type != tmp.type ||
		    info->return_info.ce != tmp.ce ||
		    info->return_info.is_instanceof != tmp.is_instanceof ||
		    !zend_bitset_in(visited, i)) {
			zend_call_info *call_info = info->caller_info;

			zend_bitset_incl(visited, i);
			info->return_info.type = tmp.type;
			info->return_info.ce = tmp.ce;
			info->return_info.is_instanceof = tmp.is_instanceof;
			while (call_info) {
				zend_op_array *op_array = call_info->caller_op_array;
				zend_func_info *info = ZEND_FUNC_INFO(op_array);

				if (info && info->ssa.ops && info->ssa.ops[call_info->caller_call_opline - op_array->opcodes].result_def >= 0) {
					zend_bitset_incl(varlist[info->num], info->ssa.ops[call_info->caller_call_opline - op_array->opcodes].result_def);
					zend_bitset_incl(worklist, info->num);
				}
				call_info = call_info->next_caller;
			}
		}
	}
}

int zend_jit_optimize_calls(zend_jit_context *ctx)
{
	zend_op_array *op_array;
	zend_func_info *info;
	zend_func_info *clone;
	int i, return_value_used;

	if ((ZCG(accel_directives).jit_opt & JIT_OPT_SSA) >= JIT_OPT_SSA_O2) {	
		zend_jit_ip_infer_ranges(ctx, 0);
		zend_jit_infer_return_types(ctx);
	}

	if ((ZCG(accel_directives).jit_opt & JIT_OPT_SSA) >= JIT_OPT_SSA_O3) {	
		/* Analyze recursive dependencies */
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			op_array = ctx->call_graph.op_arrays[i];
			zend_inference_check_recursive_dependencies(op_array);
		}

		/* Create clones for called functions */
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			op_array = ctx->call_graph.op_arrays[i];
			info = ZEND_FUNC_INFO(op_array);
			return_value_used = zend_jit_is_return_value_used(op_array);
			if (info && info->ssa.vars && info->caller_info &&
			    (op_array->num_args || return_value_used > 0)) {
				/* Create clone */
				clone = zend_jit_create_clone(ctx, info);
				if (!clone) {
					return FAILURE;
				}
				clone->return_value_used = return_value_used;
				clone->clone = info;
				ZEND_SET_FUNC_INFO(op_array, clone);
				/* Collect argument info */
				zend_jit_collect_recv_arg_info(ctx, op_array);
			}
		}

		/* Find functions that always called with the same number of arguments */
		// TODO: multiple clones
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			zend_func_info *info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);
			if (info && info->clone && info->caller_info) {
				zend_call_info *call_info = info->caller_info;
				int num_args = call_info->num_args;

				do {
					if (call_info->num_args != num_args) {
						num_args = -1;
					}
					call_info = call_info->next_caller;
				} while (call_info);
				if (num_args >= 0) {
					info->num_args = num_args;
				}
			}
		}			
	
		zend_jit_ip_infer_ranges(ctx, 1);
		zend_jit_infer_arg_and_return_types(ctx, 0);
		zend_jit_infer_arg_and_return_types(ctx, 1);

		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			zend_func_info *info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);
			if (info->return_value_used == 1 &&
	    		!(info->return_info.type & (MAY_BE_REF|MAY_BE_OBJECT|MAY_BE_ARRAY))) {
				// TODO: REGRET may be in some cases it makes sense to return heap allocated zval_ptr
				if ((info->return_info.type & MAY_BE_ANY) == (MAY_BE_FALSE|MAY_BE_TRUE) ||
				    (info->return_info.type & MAY_BE_ANY) == MAY_BE_LONG) {
					info->return_info.type |= MAY_BE_IN_REG;
#if defined(__x86_64__)
				} else if ((info->return_info.type & MAY_BE_ANY) == MAY_BE_DOUBLE) {
					info->return_info.type |= MAY_BE_IN_REG;
#endif
//???				} else {
//???					info->return_info.type |= MAY_BE_TMP_ZVAL;
				}
			}
		}

		/* Revert main function info */
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			op_array = ctx->call_graph.op_arrays[i];
			info = ZEND_FUNC_INFO(op_array);
			if (info->clone) {
				clone = info;				
				info = info->clone;
				clone->clone = info->clone;
				info->clone = clone;
				ZEND_SET_FUNC_INFO(op_array, info);
			}
		}

		/* Associate clones with calls */
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			op_array = ctx->call_graph.op_arrays[i];
			info = ZEND_FUNC_INFO(op_array);
			if (info->clone) {
				clone = info->clone;
				do {
					zend_call_info *call_info = info->caller_info;
					while (call_info) {
						// TODO: multiple clones;
						call_info->clone = clone;
						call_info = call_info->next_caller;
					}
					clone = clone->clone;
				} while (clone);
			}
		}
	}

	return SUCCESS;
}

static int zend_jit_is_similar_clone(zend_func_info *info, zend_func_info *clone)
{
	int i;

	if (info->flags != clone->flags) {
		return 0;
	}
	if (info->return_info.type != clone->return_info.type) {
		return 0;
	}
	for (i = 0; i < info->ssa.vars_count; i++) {
		if (info->ssa.var_info[i].type != clone->ssa.var_info[i].type) {
			return 0;
		}
	}
	return 1;
}

static zend_func_info* zend_jit_find_similar_clone(zend_func_info *info, zend_func_info *clone)
{
	while (1) {
		if (zend_jit_is_similar_clone(info, clone)) {
			return info;
		}
		info = info->clone;
		if (info == clone) {
			return NULL;
		}
	}
}

void zend_jit_remove_useless_clones(zend_op_array *op_array)
{
	zend_func_info *info = ZEND_FUNC_INFO(op_array);
	zend_func_info **clone = &info->clone;

	while (*clone) {
		zend_func_info *similar = zend_jit_find_similar_clone(info, *clone);
		if (similar) {
			zend_call_info *call_info = info->caller_info;

			while (call_info) {
				if (call_info->clone == *clone) {
					call_info->clone = similar;
				}
				call_info = call_info->next_caller;
			}
			*clone = (*clone)->clone;						
		} else {
			clone = &(*clone)->clone;
		}
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
