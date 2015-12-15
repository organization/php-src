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
#include "Optimizer/zend_inference.h"
#include "Optimizer/zend_func_info.h"
#include "Optimizer/zend_call_graph.h"

#define ZEND_JIT_FUNC_MAY_COMPILE              (1<<8)
#define ZEND_JIT_FUNC_NO_IN_MEM_CVS            (1<<10)
#define ZEND_JIT_FUNC_NO_USED_ARGS             (1<<11)
#define ZEND_JIT_FUNC_NO_SYMTAB                (1<<12)
#define ZEND_JIT_FUNC_NO_FRAME                 (1<<13)
#define ZEND_JIT_FUNC_INLINE                   (1<<14)
#define ZEND_JIT_FUNC_HAS_REG_ARGS             (1<<15)

typedef struct _zend_jit_context {
	zend_arena             *arena;
	zend_script            *main_script;
	zend_call_graph         call_graph;
	void                   *codegen_ctx;
} zend_jit_context;

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

#define OP1_SSA_VAR()           (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline - op_array->opcodes].op1_use : -1)
#define OP2_SSA_VAR()           (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline - op_array->opcodes].op2_use : -1)
#define OP1_DATA_SSA_VAR()      (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline + 1 - op_array->opcodes].op1_use : -1)
#define OP2_DATA_SSA_VAR()      (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline + 1 - op_array->opcodes].op2_use : -1)
#define OP1_DEF_SSA_VAR()       (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline - op_array->opcodes].op1_def : -1)
#define OP2_DEF_SSA_VAR()       (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline - op_array->opcodes].op2_def : -1)
#define OP1_DATA_DEF_SSA_VAR()  (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline + 1 - op_array->opcodes].op1_def : -1)
#define OP2_DATA_DEF_SSA_VAR()  (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline + 1 - op_array->opcodes].op2_def : -1)
#define RES_SSA_VAR()           (ZEND_FUNC_INFO(op_array)->ssa.ops ? ZEND_FUNC_INFO(op_array)->ssa.ops[opline - op_array->opcodes].result_def : -1)

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

#ifdef __cplusplus
extern "C" {
#endif

int zend_jit_optimize_ssa(zend_jit_context *ctx, zend_op_array *op_array) ZEND_HIDDEN;
int zend_jit_optimize_vars(zend_jit_context *ctx, zend_op_array *op_array) ZEND_HIDDEN;
int zend_jit_optimize_calls(zend_jit_context *ctx) ZEND_HIDDEN;
void zend_jit_remove_useless_clones(zend_op_array *op_array) ZEND_HIDDEN;

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
