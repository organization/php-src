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

#ifndef _ZEND_JIT_CONTEXT_H_
#define _ZEND_JIT_CONTEXT_H_

#include <zend.h>
#include <zend_API.h>
#include <zend_compile.h>
#include <zend_vm.h>
#include <zend_execute.h>
#include <zend_constants.h>
#include <zend_exceptions.h>
#include "zend_arena.h"
#include "zend_bitset.h"
#include "Optimizer/zend_cfg.h"
#include "Optimizer/zend_dfg.h"
#include "Optimizer/zend_ssa.h"

typedef struct _zend_ssa_var_info {
	uint32_t               type; /* inferred type */
	zend_ssa_range         range;
	zend_class_entry      *ce;
	unsigned int           has_range : 1;
	unsigned int           is_instanceof : 1; /* 0 - class == "ce", 1 - may be child of "ce" */
	unsigned int           recursive : 1;
	unsigned int           use_as_double : 1;
} zend_ssa_var_info;

#define ZEND_JIT_FUNC_HAS_PREALLOCATED_CVS     (1<<5) 
#define ZEND_JIT_FUNC_MAY_COMPILE              (1<<6)
#define ZEND_JIT_FUNC_RECURSIVE                (1<<7)
#define ZEND_JIT_FUNC_RECURSIVE_DIRECTLY       (1<<8)
#define ZEND_JIT_FUNC_RECURSIVE_INDIRECTLY     (1<<9)
#define ZEND_JIT_FUNC_NO_IN_MEM_CVS            (1<<10)
#define ZEND_JIT_FUNC_NO_USED_ARGS             (1<<11)
#define ZEND_JIT_FUNC_NO_SYMTAB                (1<<12)
#define ZEND_JIT_FUNC_NO_FRAME                 (1<<13)
#define ZEND_JIT_FUNC_INLINE                   (1<<14)
#define ZEND_JIT_FUNC_HAS_REG_ARGS             (1<<15)

typedef struct _zend_jit_func_info zend_jit_func_info;
typedef struct _zend_jit_call_info zend_jit_call_info;

typedef struct _zend_jit_arg_info {
	zend_op                *opline;
} zend_jit_arg_info;

typedef struct _zend_jit_recv_arg_info {
	int                     ssa_var;
	zend_ssa_var_info       info;
} zend_jit_recv_arg_info;

struct _zend_jit_call_info {
	zend_op_array          *caller_op_array;
	zend_op                *caller_init_opline;
	zend_op                *caller_call_opline;
	zend_function          *callee_func;
	zend_jit_call_info     *next_caller;
	zend_jit_call_info     *next_callee;
	zend_jit_func_info     *clone;
	int                     recursive;
	int                     num_args;
	zend_jit_arg_info       arg_info[1];
};

struct _zend_jit_func_info {
	int                     num;
	uint32_t                flags;
	zend_cfg                cfg;          /* Control Flow Graph            */
	zend_ssa                ssa;          /* Static Single Assignmnt Form  */
	zend_ssa_var_info      *ssa_var_info; /* type/range of SSA variales    */
	int                     sccs;         /* number of SCCs                */
	zend_jit_call_info     *caller_info;  /* where this function is called from */
	zend_jit_call_info     *callee_info;  /* which functions are called from this one */
	int                     num_args;     /* (-1 - unknown) */
	zend_jit_recv_arg_info *arg_info;
	zend_ssa_var_info       return_info;
	zend_jit_func_info     *clone;
	int                     clone_num;
	int                     return_value_used; /* -1 unknown, 0 no, 1 yes */
	void                   *codegen_data;
};

typedef struct _zend_jit_context {
	zend_arena             *arena;
	zend_script            *main_script;
	int                     op_arrays_count;
	zend_op_array         **op_arrays;
	void                   *codegen_ctx;
} zend_jit_context;

extern int zend_jit_rid;

#define JIT_DATA(op_array)  ((zend_jit_func_info*)((op_array)->reserved[zend_jit_rid]))

#define JIT_DATA_SET(op_array, info) do { \
		zend_jit_func_info** pinfo = (zend_jit_func_info**)&(op_array)->reserved[zend_jit_rid]; \
		*pinfo = info; \
	} while (0)


#define JIT_DEBUG_DUMP_TYPED_SSA    0x001
#define JIT_DEBUG_DUMP_SSA          0x002
#define JIT_DEBUG_DUMP_TYPES        0x004
#define JIT_DEBUG_DUMP_VARS         0x008

#define JIT_DEBUG_DUMP_CFG          0x010
#define JIT_DEBUG_DUMP_DOMINATORS   0x020
#define JIT_DEBUG_DUMP_LIVENESS     0x040
#define JIT_DEBUG_DUMP_PHI          0x080

#define JIT_DEBUG_DUMP_ASM          0x100
#define JIT_DEBUG_DUMP_ASM_WITH_SSA 0x200
#define JIT_DEBUG_DUMP_SRC_LLVM_IR  0x400
#define JIT_DEBUG_DUMP_OPT_LLVM_IR  0x800
#define JIT_DEBUG_SYMSUPPORT		0x1000

#define JIT_DEBUG_STAT              0x08000000 /* dump JIT statistics */

#define JIT_DEBUG_GDB				0x10000000 /* don't delete debug sumbols */
#define JIT_DEBUG_OPROFILE			0x20000000
#define JIT_DEBUG_VALGRIND			0x40000000

#define JIT_OPT_BC                  0x3000
#define JIT_OPT_BC_O0               0x0000
#define JIT_OPT_BC_O1               0x1000
#define JIT_OPT_BC_O2               0x2000
#define JIT_OPT_BC_O3               0x3000

#define JIT_OPT_SSA                 0x0300
#define JIT_OPT_SSA_O0              0x0000
#define JIT_OPT_SSA_O1              0x0100
#define JIT_OPT_SSA_O2              0x0200
#define JIT_OPT_SSA_O3              0x0300

#define JIT_OPT_LLVM                0x0030
#define JIT_OPT_LLVM_O0             0x0000
#define JIT_OPT_LLVM_O1             0x0010
#define JIT_OPT_LLVM_O2             0x0020
#define JIT_OPT_LLVM_O3             0x0030

#define JIT_OPT_CODEGEN             0x0003
#define JIT_OPT_CODEGEN_O0          0x0000
#define JIT_OPT_CODEGEN_O1          0x0001
#define JIT_OPT_CODEGEN_O2          0x0002
#define JIT_OPT_CODEGEN_O3          0x0003

#define RETURN_VALUE_USED(opline) (!((opline)->result_type & EXT_TYPE_UNUSED))

/* Bitmask for type inference */
#define MAY_BE_NULL		(1<<0) //???IS_NULL)
#define MAY_BE_FALSE	(1<<1) //???IS_FALSE)
#define MAY_BE_TRUE		(1<<2) //???IS_TRUE)
#define MAY_BE_LONG		(1<<3) //???IS_LONG)
#define MAY_BE_DOUBLE	(1<<4) //???IS_DOUBLE)
#define MAY_BE_STRING	(1<<5) //???IS_STRING)
#define MAY_BE_ARRAY	(1<<6) //???IS_ARRAY)
#define MAY_BE_OBJECT	(1<<7) //???IS_OBJECT)
#define MAY_BE_RESOURCE	(1<<8) //???IS_RESOURCE)
#define MAY_BE_ANY      0x1ff

#define MAY_BE_UNDEF    (1<<9)
#define MAY_BE_DEF      (1<<10)
#define MAY_BE_REF      (1<<11) /* may be reference */
#define MAY_BE_RC1      (1<<12) /* may be non-reference with refcount == 1 */
#define MAY_BE_RCN      (1<<13) /* may be non-reference with refcount > 1  */

#define MAY_BE_IN_MEM   (1<<14) /* at least one of usage requires completely
                                   initialized zval structure in memory */
#define MAY_BE_IN_REG   (1<<15) /* value allocated in CPU register */

#define MAY_BE_ARRAY_OF_NULL		(1<<(16+0)) //???IS_NULL))
#define MAY_BE_ARRAY_OF_FALSE		(1<<(16+1)) //???IS_BOOL))
#define MAY_BE_ARRAY_OF_TRUE		(1<<(16+2)) //???IS_BOOL))
#define MAY_BE_ARRAY_OF_LONG		(1<<(16+3)) //???IS_LONG))
#define MAY_BE_ARRAY_OF_DOUBLE		(1<<(16+4)) //???IS_DOUBLE))
#define MAY_BE_ARRAY_OF_STRING		(1<<(16+5)) //???IS_STRING))
#define MAY_BE_ARRAY_OF_ARRAY		(1<<(16+6)) //???IS_ARRAY))
#define MAY_BE_ARRAY_OF_OBJECT		(1<<(16+7)) //???IS_OBJECT))
#define MAY_BE_ARRAY_OF_RESOURCE	(1<<(16+8)) //???IS_RESOURCE))
#define MAY_BE_ARRAY_OF_ANY			0x1ff0000

#define MAY_BE_ARRAY_KEY_LONG       (1<<25)
#define MAY_BE_ARRAY_KEY_STRING     (1<<26)
#define MAY_BE_ARRAY_KEY_ANY        (MAY_BE_ARRAY_KEY_LONG | MAY_BE_ARRAY_KEY_STRING)

#define MAY_BE_ARRAY_OF_REF			(1<<27)

#define MAY_BE_ERROR                (1<<28)
#define MAY_BE_CLASS                (1<<29)

#define MAY_BE_REG_ZVAL             (1<<30)
#define MAY_BE_REG_ZVAL_PTR         (1<<31)

/* The following flags are valid only for return values of internal functions
 * returned by zend_jit_get_func_info()
 */

#define FUNC_MAY_WARN               (1<<30)
#define FUNC_MAY_INLINE             (1<<31)

static inline int next_use(zend_ssa_op *ssa_op, int var, int use)
{
	ssa_op += use;
	if (ssa_op->result_use == var) {
		return ssa_op->res_use_chain;
	}
	return (ssa_op->op1_use == var) ? ssa_op->op1_use_chain : ssa_op->op2_use_chain;
}

static inline zend_ssa_phi* next_use_phi(zend_jit_func_info *info, int var, zend_ssa_phi *p)
{
	if (p->pi >= 0) {
		return p->use_chains[0];
	} else {
		int j;
		for (j = 0; j < info->cfg.blocks[p->block].predecessors_count; j++) {
			if (p->sources[j] == var) {
				return p->use_chains[j];
			}
		}
	}
	return NULL;
}

static inline uint32_t get_ssa_var_info(zend_jit_func_info *info, int ssa_var_num)
{
	if (info->ssa_var_info && ssa_var_num >= 0) {
		return info->ssa_var_info[ssa_var_num].type;
	} else {
		return MAY_BE_IN_MEM | MAY_BE_DEF | MAY_BE_UNDEF | MAY_BE_RC1 | MAY_BE_RCN | MAY_BE_REF | MAY_BE_ANY | MAY_BE_ARRAY_KEY_ANY | MAY_BE_ARRAY_OF_ANY | MAY_BE_ARRAY_OF_REF | MAY_BE_ERROR;
	}
}

static inline uint32_t ssa_result_info(zend_op_array *op_array, zend_op *opline)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	return get_ssa_var_info(info, info->ssa.ops ? info->ssa.ops[opline - op_array->opcodes].result_def : -1);
}

static inline uint32_t ssa_op1_def_info(zend_op_array *op_array, zend_op *opline)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	return get_ssa_var_info(info, info->ssa.ops ? info->ssa.ops[opline - op_array->opcodes].op1_def : -1);
}

static inline uint32_t ssa_op2_def_info(zend_op_array *op_array, zend_op *opline)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	return get_ssa_var_info(info, info->ssa.ops ? info->ssa.ops[opline - op_array->opcodes].op2_def : -1);
}

static inline int ssa_result_var(zend_op_array *op_array, zend_op *opline)
{
	zend_jit_func_info *info = JIT_DATA(op_array);
	return info->ssa.ops ? info->ssa.ops[opline - op_array->opcodes].result_def : -1;
}

static inline uint32_t _const_op_type(zval *zv) {
	if (Z_TYPE_P(zv) == IS_CONSTANT) {
		return MAY_BE_ANY | MAY_BE_ARRAY_KEY_ANY | MAY_BE_ARRAY_OF_ANY;
	} else if (Z_TYPE_P(zv) == IS_CONSTANT_AST) {
		return MAY_BE_ANY | MAY_BE_ARRAY_KEY_ANY | MAY_BE_ARRAY_OF_ANY;
	} else if (Z_TYPE_P(zv) == IS_ARRAY) {
		HashTable *ht = Z_ARRVAL_P(zv);
		uint32_t tmp = MAY_BE_ARRAY | MAY_BE_DEF | MAY_BE_RC1;

		zend_string *str;
		zval *val;
		ZEND_HASH_FOREACH_STR_KEY_VAL(ht, str, val) {
			if (str) {
				tmp |= MAY_BE_ARRAY_KEY_STRING;
			} else {
				tmp |= MAY_BE_ARRAY_KEY_LONG;
			}
			tmp |= 1 << (Z_TYPE_P(val) - 1 + 16);
		} ZEND_HASH_FOREACH_END();
		return tmp;
	} else {
		return (1 << (Z_TYPE_P(zv) - 1)) | MAY_BE_DEF | MAY_BE_RC1;
	}
}

#define DEFINE_SSA_OP_INFO(opN) \
	static inline uint32_t ssa_##opN##_info(zend_op_array *op_array, zend_op *opline) \
	{																		\
		zend_jit_func_info *info = JIT_DATA(op_array); \
		if (opline->opN##_type == IS_CONST) {							\
			return _const_op_type(RT_CONSTANT(op_array, opline->opN)); \
		} else { \
			return get_ssa_var_info(info, info->ssa.ops ? info->ssa.ops[opline - op_array->opcodes].opN##_use : -1); \
		} \
	}

DEFINE_SSA_OP_INFO(op1)
DEFINE_SSA_OP_INFO(op2)

#define OP1_SSA_VAR()           (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].op1_use : -1)
#define OP2_SSA_VAR()           (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].op2_use : -1)
#define OP1_DATA_SSA_VAR()      (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline + 1 - op_array->opcodes].op1_use : -1)
#define OP2_DATA_SSA_VAR()      (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline + 1 - op_array->opcodes].op2_use : -1)
#define OP1_DEF_SSA_VAR()       (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].op1_def : -1)
#define OP2_DEF_SSA_VAR()       (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].op2_def : -1)
#define OP1_DATA_DEF_SSA_VAR()  (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline + 1 - op_array->opcodes].op1_def : -1)
#define OP2_DATA_DEF_SSA_VAR()  (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline + 1 - op_array->opcodes].op2_def : -1)
#define RES_SSA_VAR()           (JIT_DATA(op_array)->ssa.ops ? JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].result_def : -1)

#define OP1_INFO()              (ssa_op1_info(op_array, opline))
#define OP2_INFO()              (ssa_op2_info(op_array, opline))
#define OP1_DATA_INFO()         (ssa_op1_info(op_array, (opline+1)))
#define OP2_DATA_INFO()         (ssa_op2_info(op_array, (opline+1)))
#define OP1_DEF_INFO()          (ssa_op1_def_info(op_array, opline))
#define OP2_DEF_INFO()          (ssa_op2_def_info(op_array, opline))
#define OP1_DATA_DEF_INFO()     (ssa_op1_def_info(op_array, (opline+1)))
#define OP2_DATA_DEF_INFO()     (ssa_op2_def_info(op_array, (opline+1)))
#define RES_INFO()              (ssa_result_info(op_array, opline))

#define OP1_MAY_BE(t)      (OP1_INFO() & (t))
#define OP2_MAY_BE(t)      (OP2_INFO() & (t))
#define OP1_DEF_MAY_BE(t)  (OP1_DEF_INFO() & (t))
#define OP2_DEF_MAY_BE(t)  (OP2_DEF_INFO() & (t))
#define RES_MAY_BE(t)      (RES_INFO() & (t))

#define OP1_OP()           (&(opline->op1))
#define OP2_OP()           (&(opline->op2))
#define OP1_DATA_OP()      (&((opline + 1)->op1))
#define RES_OP()           (&(opline->result))
#define OP1_OP_TYPE()      (opline->op1_type)
#define OP2_OP_TYPE()      (opline->op2_type)
#define OP1_DATA_OP_TYPE() ((opline + 1)->op1_type)

#define DEFINE_SSA_OP_HAS_RANGE(opN) \
	static inline long ssa_##opN##_has_range(zend_op_array *op_array, zend_op *opline) \
	{ \
		return ((opline->opN##_type == IS_CONST && \
			    (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_LONG || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_TRUE || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_FALSE || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_NULL)) || \
		       (opline->opN##_type != IS_UNUSED && \
		        JIT_DATA(op_array)->ssa.ops && \
		        JIT_DATA(op_array)->ssa_var_info && \
		        JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use >= 0 && \
			    JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].has_range)); \
	}

#define DEFINE_SSA_OP_MIN_RANGE(opN) \
	static inline long ssa_##opN##_min_range(zend_op_array *op_array, zend_op *opline) \
	{ \
		if (opline->opN##_type == IS_CONST) { \
			if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_LONG) { \
				return Z_LVAL_P(RT_CONSTANT(op_array, opline->opN)); \
			} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_TRUE) { \
				return 1; \
			} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_FALSE) { \
				return 0; \
			} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_NULL) { \
				return 0; \
			} \
		} else if (opline->opN##_type != IS_UNUSED && \
		    JIT_DATA(op_array)->ssa.ops && \
		    JIT_DATA(op_array)->ssa_var_info && \
		    JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use >= 0 && \
		    JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].has_range) { \
			return JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].range.min; \
		} \
		return LONG_MIN; \
	}

#define DEFINE_SSA_OP_MAX_RANGE(opN) \
	static inline long ssa_##opN##_max_range(zend_op_array *op_array, zend_op *opline) \
	{ \
		if (opline->opN##_type == IS_CONST) { \
			if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_LONG) { \
				return Z_LVAL_P(RT_CONSTANT(op_array, opline->opN)); \
			} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_TRUE) { \
				return 1; \
			} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_FALSE) { \
				return 0; \
			} else if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_NULL) { \
				return 0; \
			} \
		} else if (opline->opN##_type != IS_UNUSED && \
		    JIT_DATA(op_array)->ssa.ops && \
		    JIT_DATA(op_array)->ssa_var_info && \
		    JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use >= 0 && \
		    JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].has_range) { \
			return JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].range.max; \
		} \
		return LONG_MAX; \
	}

#define DEFINE_SSA_OP_RANGE_UNDERFLOW(opN) \
	static inline char ssa_##opN##_range_underflow(zend_op_array *op_array, zend_op *opline) \
	{ \
		if (opline->opN##_type == IS_CONST) { \
			if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_LONG || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_TRUE || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_FALSE || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_NULL) { \
				return 0; \
			} \
		} else if (opline->opN##_type != IS_UNUSED && \
		    JIT_DATA(op_array)->ssa.ops && \
		    JIT_DATA(op_array)->ssa_var_info && \
		    JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use >= 0 && \
		    JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].has_range) { \
			return JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].range.underflow; \
		} \
		return 1; \
	}

#define DEFINE_SSA_OP_RANGE_OVERFLOW(opN) \
	static inline char ssa_##opN##_range_overflow(zend_op_array *op_array, zend_op *opline) \
	{ \
		if (opline->opN##_type == IS_CONST) { \
			if (Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_LONG || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_TRUE || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_FALSE || Z_TYPE_P(RT_CONSTANT(op_array, opline->opN)) == IS_NULL) { \
				return 0; \
			} \
		} else if (opline->opN##_type != IS_UNUSED && \
		    JIT_DATA(op_array)->ssa.ops && \
		    JIT_DATA(op_array)->ssa_var_info && \
		    JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use >= 0 && \
		    JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].has_range) { \
			return JIT_DATA(op_array)->ssa_var_info[JIT_DATA(op_array)->ssa.ops[opline - op_array->opcodes].opN##_use].range.overflow; \
		} \
		return 1; \
	}

DEFINE_SSA_OP_HAS_RANGE(op1)
DEFINE_SSA_OP_MIN_RANGE(op1)
DEFINE_SSA_OP_MAX_RANGE(op1)
DEFINE_SSA_OP_RANGE_UNDERFLOW(op1)
DEFINE_SSA_OP_RANGE_OVERFLOW(op1)
DEFINE_SSA_OP_HAS_RANGE(op2)
DEFINE_SSA_OP_MIN_RANGE(op2)
DEFINE_SSA_OP_MAX_RANGE(op2)
DEFINE_SSA_OP_RANGE_UNDERFLOW(op2)
DEFINE_SSA_OP_RANGE_OVERFLOW(op2)

#define OP1_HAS_RANGE()         (ssa_op1_has_range (op_array, opline))
#define OP1_MIN_RANGE()         (ssa_op1_min_range (op_array, opline))
#define OP1_MAX_RANGE()         (ssa_op1_max_range (op_array, opline))
#define OP1_RANGE_UNDERFLOW()   (ssa_op1_range_underflow (op_array, opline))
#define OP1_RANGE_OVERFLOW()    (ssa_op1_range_overflow (op_array, opline))
#define OP2_HAS_RANGE()         (ssa_op2_has_range (op_array, opline))
#define OP2_MIN_RANGE()         (ssa_op2_min_range (op_array, opline))
#define OP2_MAX_RANGE()         (ssa_op2_max_range (op_array, opline))
#define OP2_RANGE_UNDERFLOW()   (ssa_op2_range_underflow (op_array, opline))
#define OP2_RANGE_OVERFLOW()    (ssa_op2_range_overflow (op_array, opline))

#define JIT_DUMP_CFG			(1U<<0)
#define JIT_DUMP_DOMINATORS		(1U<<1)
#define JIT_DUMP_PHI_PLACEMENT	(1U<<2)
#define JIT_DUMP_SSA			(1U<<3)
#define JIT_DUMP_VAR_TYPES		(1U<<4)
#define JIT_DUMP_VAR			(1U<<5)


static inline void*
zend_jit_context_calloc(zend_jit_context *ctx, size_t unit_size, size_t count)
{
	return zend_arena_calloc(&ctx->arena, unit_size, count);
}

#define ZEND_JIT_CONTEXT_CALLOC(ctx, dst, n)							\
	do {																\
		dst = zend_jit_context_calloc(ctx, sizeof(*(dst)), n);			\
		if (!dst)														\
			return FAILURE;												\
	} while (0)

#ifdef __cplusplus
extern "C" {
#endif

int zend_jit_optimize_ssa(zend_jit_context *ctx, zend_op_array *op_array) ZEND_HIDDEN;
int zend_jit_optimize_vars(zend_jit_context *ctx, zend_op_array *op_array) ZEND_HIDDEN;
int zend_jit_optimize_calls(zend_jit_context *ctx) ZEND_HIDDEN;
void zend_jit_remove_useless_clones(zend_op_array *op_array) ZEND_HIDDEN;

uint32_t array_element_type(uint32_t t1, int write, int insert) ZEND_HIDDEN;

void zend_jit_dump(zend_op_array *op_array, uint32_t dump_flags) ZEND_HIDDEN;
void zend_jit_dump_ssa_line(zend_op_array *op_array, const zend_basic_block *b, uint32_t line) ZEND_HIDDEN;
void zend_jit_dump_ssa_bb_header(zend_op_array *op_array, uint32_t line) ZEND_HIDDEN;
void zend_jit_dump_ssa_var(zend_op_array *op_array, int ssa_var_num, int var_num, int pos) ZEND_HIDDEN;
void zend_jit_dump_var(zend_op_array *op_array, int var_num) ZEND_HIDDEN;

void zend_jit_func_info_startup(void);
void zend_jit_func_info_shutdown(void);
uint32_t zend_jit_get_func_info(const zend_jit_call_info *call_info);

#ifdef __cplusplus
}
#endif

#endif /* _ZEND_JIT_CONTEXT_H_ */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
