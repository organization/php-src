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

#include "jit/zend_jit.h"
#include "jit/zend_jit_context.h"
#include "jit/zend_jit_codegen.h"
#include "zend_bitset.h"
#include "Optimizer/zend_dump.h"

#if HAVE_VALGRIND
# include "valgrind/callgrind.h"
#endif

static zend_jit_context *zend_jit_context_create(zend_script* main_script, size_t arena_size)
{
	zend_jit_context *ctx;
	zend_arena *arena = zend_arena_create(arena_size);

	if (!arena) {
		return NULL;
	}

	ctx = zend_arena_alloc(&arena, sizeof(zend_jit_context));
	if (!ctx) {
		zend_arena_destroy(arena);
		return NULL;
	}
	memset(ctx, 0, sizeof(sizeof(zend_jit_context)));

	ctx->arena = arena;
	ctx->main_script = main_script;

	return ctx;
}

static zend_jit_context *zend_jit_start_script(zend_script* main_script)
{
	return zend_jit_context_create(main_script,
			 sizeof(void*) * 4 * 1024 * 1024);
}

static int zend_jit_end_script(zend_jit_context *ctx)
{
	zend_arena_destroy(ctx->arena);
	return SUCCESS;
}

static int zend_jit_op_array_analyze_cfg(zend_jit_context *ctx, zend_op_array *op_array)
{
    zend_func_info *info = ZEND_FUNC_INFO(op_array);

	if (!info) {
		return FAILURE;
	}
	info->ssa.rt_constants = 1;

	/* Don't run JIT on very big functions */
	if (op_array->last > JIT_MAX_OPCODES) {
		return FAILURE;
	}

	if (zend_build_cfg(&ctx->arena, op_array, ZEND_CFG_NO_ENTRY_PREDECESSORS | ZEND_RT_CONSTANTS, &info->ssa.cfg, &info->flags) != SUCCESS) {
		return FAILURE;
	}

	if (zend_cfg_build_predecessors(&ctx->arena, &info->ssa.cfg) != SUCCESS) {
		return FAILURE;
	}

	if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_CFG) {
		zend_dump_op_array(op_array, ZEND_DUMP_CFG | ZEND_DUMP_RT_CONSTANTS, NULL, &info->ssa.cfg);
	}

//???
	if (op_array->fn_flags & ZEND_ACC_GENERATOR) {
		// TODO: LLVM Support for generators
		return SUCCESS;
	}

	if (op_array->fn_flags & ZEND_ACC_HAS_FINALLY_BLOCK) {
		// TODO: LLVM Support for finally
		return SUCCESS;
	}

	return SUCCESS;
}

static int zend_jit_op_array_analyze_ssa(zend_jit_context *ctx, zend_op_array *op_array)
{
    zend_func_info *info = ZEND_FUNC_INFO(op_array);
    uint32_t build_flags;
	
	if (info && info->ssa.cfg.blocks &&
	    op_array->last_try_catch == 0 &&
	    !(op_array->fn_flags & ZEND_ACC_GENERATOR)) {
		if (!(info->flags & ZEND_FUNC_INDIRECT_VAR_ACCESS)) {
			if (zend_cfg_compute_dominators_tree(op_array, &info->ssa.cfg) != SUCCESS) {
				return FAILURE;
			}
			if (zend_cfg_identify_loops(op_array, &info->ssa.cfg, &info->flags) != SUCCESS) {
				return FAILURE;
			}
			if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_DOMINATORS) {
				zend_dump_dominators(op_array, &info->ssa.cfg);
			}
			build_flags = ZEND_RT_CONSTANTS | ZEND_SSA_RC_INFERENCE;
			if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_LIVENESS) {
				build_flags |= ZEND_SSA_DEBUG_LIVENESS;
			}
			if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_PHI) {
				build_flags |= ZEND_SSA_DEBUG_PHI_PLACEMENT;
			}
			if (zend_build_ssa(&ctx->arena, op_array, build_flags, &info->ssa, &info->flags) != SUCCESS) {
				return FAILURE;
			}
			if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_SSA) {
				zend_dump_op_array(op_array, ZEND_DUMP_SSA | ZEND_DUMP_RT_CONSTANTS, NULL, &info->ssa);
			}
			if (zend_jit_optimize_ssa(ctx, op_array) != SUCCESS) {
				return FAILURE;
			}
		}
	}
	return SUCCESS;
}

#if HAVE_VALGRIND
static int zend_jit_int(zend_script *script)
#else
int zend_jit(zend_script *script)
#endif
{
	int i;
	zend_func_info *info;
	zend_jit_context *ctx;
	
	ctx = zend_jit_start_script(script);
	if (!ctx) {
		return FAILURE;
	}

	if (zend_build_call_graph(&ctx->arena, script, ZEND_RT_CONSTANTS, &ctx->call_graph) != SUCCESS) {
		return FAILURE;
	}

	/* Disable profiling for JIT-ed scripts */
//???	if (ZCG(accel_directives).jit_profile) {
//???		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
//???			zend_jit_profile_reset(ctx->call_graph.op_arrays[i]);
//???		}
//???	}

	/* Analyses */
	for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
		zend_jit_op_array_analyze_cfg(ctx, ctx->call_graph.op_arrays[i]);
	}
	for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
		zend_jit_op_array_analyze_ssa(ctx, ctx->call_graph.op_arrays[i]);
	}
//???	zend_jit_optimize_calls(ctx);
	for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
		info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);
		zend_jit_optimize_vars(ctx, ctx->call_graph.op_arrays[i]);
		/* optimize clones */
		if (info->clone) {
			zend_func_info *clone = info->clone;
			do {
				ZEND_SET_FUNC_INFO(ctx->call_graph.op_arrays[i], clone);
//???				zend_jit_optimize_vars(ctx, ctx->call_graph.op_arrays[i]);
				clone = clone->clone;
			} while (clone);
			ZEND_SET_FUNC_INFO(ctx->call_graph.op_arrays[i], info);
		}
	}

//???	for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
//???		info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);
//???		if (info->clone) {
//???			zend_jit_remove_useless_clones(ctx->call_graph.op_arrays[i]);
//???		}
//???	}

	if (ZCG(accel_directives).jit_debug & (JIT_DEBUG_DUMP_VARS|JIT_DEBUG_DUMP_TYPES|JIT_DEBUG_DUMP_TYPED_SSA)) {
		for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
			zend_op_array *op_array = ctx->call_graph.op_arrays[i];
			zend_func_info *info = ZEND_FUNC_INFO(op_array);
			zend_func_info *clone = info;

			while (clone) {
				ZEND_SET_FUNC_INFO(ctx->call_graph.op_arrays[i], clone);
				if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_VARS) {
					zend_dump_variables(op_array);
				}

				if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_TYPES) {
					zend_dump_ssa_variables(op_array, &info->ssa, ZEND_DUMP_RC_INFERENCE);
				}

				if (ZCG(accel_directives).jit_debug & JIT_DEBUG_DUMP_TYPED_SSA) {
					zend_dump_op_array(op_array, ZEND_DUMP_SSA | ZEND_DUMP_RT_CONSTANTS, NULL, &info->ssa);
				}
				clone = clone->clone;
			}
			ZEND_SET_FUNC_INFO(ctx->call_graph.op_arrays[i], info);
		}
	}

	/* Code Generation */
	if (zend_jit_codegen_start_script(ctx) != SUCCESS) {
		zend_jit_end_script(ctx);
		return FAILURE;
	}

	for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
		info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);
		if (info && info->ssa.cfg.blocks && zend_jit_codegen_may_compile(ctx->call_graph.op_arrays[i])) {
			info->flags |= ZEND_JIT_FUNC_MAY_COMPILE;
			if (info->clone) {
				zend_func_info *clone = info->clone;
				do {
					clone->flags |= ZEND_JIT_FUNC_MAY_COMPILE;
					clone = clone->clone;
				} while (clone);
			}
		}
	}
	for (i = 0; i < ctx->call_graph.op_arrays_count; i++) {
		info = ZEND_FUNC_INFO(ctx->call_graph.op_arrays[i]);
		if (info && info->ssa.cfg.blocks && (info->flags & ZEND_JIT_FUNC_MAY_COMPILE)) {
			zend_jit_codegen(ctx, ctx->call_graph.op_arrays[i]);
//			num_compiled_funcs++;
			/* compile clones */
			if (info->clone) {
				zend_func_info *clone = info->clone;
				do {
					if (!(clone->flags & ZEND_JIT_FUNC_INLINE)) {
						ZEND_SET_FUNC_INFO(ctx->call_graph.op_arrays[i], clone);
						zend_jit_codegen(ctx, ctx->call_graph.op_arrays[i]);
					}
					clone = clone->clone;
				} while (clone);
				ZEND_SET_FUNC_INFO(ctx->call_graph.op_arrays[i], info);
			}
		}
	}
	
//	fprintf(stderr, "%s: compiled functions: %d\n", script->full_path, num_compiled_funcs);
	
	if (zend_jit_codegen_end_script(ctx) != SUCCESS) {
		zend_jit_end_script(ctx);
		return FAILURE;
	}

	if (zend_jit_end_script(ctx) != SUCCESS) {
		return FAILURE;
	}

	return SUCCESS;
}

#if HAVE_VALGRIND
int zend_jit(zend_script *script)
{
	int ret;

	if (!(ZCG(accel_directives).jit_debug & JIT_DEBUG_VALGRIND)) {
		CALLGRIND_STOP_INSTRUMENTATION;
	}
	ret = zend_jit_int(script);
	if (!(ZCG(accel_directives).jit_debug & JIT_DEBUG_VALGRIND)) {
		CALLGRIND_START_INSTRUMENTATION;
	}
	return ret;
}
#endif

int zend_jit_startup(size_t size)
{
//???	if (ZCG(accel_directives).jit_profile) {
//???		if (zend_jit_profile_init() != SUCCESS) {
//???			return FAILURE;
//???		}
//???	}

	return zend_jit_codegen_startup(size);
}

void zend_jit_shutdown(void)
{
	zend_jit_codegen_shutdown();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
