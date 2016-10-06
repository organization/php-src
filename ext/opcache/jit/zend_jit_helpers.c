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
   |          Xinchen Hui <laruence@php.net>                              |
   +----------------------------------------------------------------------+
*/

/* $Id:$ */

#include <ZendAccelerator.h>

#include <zend.h>
#include <zend_API.h>
#include <zend_compile.h>
#include <zend_vm.h>
#include <zend_execute.h>
#include <zend_constants.h>
#include <zend_exceptions.h>

#include "jit/zend_jit_config.h"
#include "jit/zend_jit_helpers.h"

ZEND_FASTCALL zend_string* zend_jit_helper_string_alloc(size_t len, int persistent) {
	return zend_string_alloc(len, persistent);
}

ZEND_FASTCALL zend_string* zend_jit_helper_string_realloc(zend_string *str, size_t len, int persistent) {
	return zend_string_realloc(str, len, persistent);
}

ZEND_FASTCALL void zend_jit_helper_string_release(zend_string *str) {
	return zend_string_release(str);
}

ZEND_FASTCALL int zend_jit_helper_handle_numeric_str(zend_string *str, zend_ulong *idx) {
	register const char *tmp = str->val;

	if (*tmp > '9') {
		return 0;
	} else if (*tmp < '0') {
		if (*tmp != '-') {
			return 0;
		}
		tmp++;
		if (*tmp > '9' || *tmp < '0') {
			return 0;
		}
	}
	return _zend_handle_numeric_str_ex(str->val, str->len, idx);
}

zend_ulong zend_jit_helper_dval_to_lval(double dval) {
	return zend_dval_to_lval(dval);
}

ZEND_FASTCALL zend_ulong zend_jit_helper_slow_str_index(zval *dim, uint32_t type) {
	switch (Z_TYPE_P(dim)) {
		case IS_STRING:
			if (IS_LONG == is_numeric_string(Z_STRVAL_P(dim), Z_STRLEN_P(dim), NULL, NULL, -1)) {
				break;
			}
			if (type != BP_VAR_IS) {
				zend_error(E_WARNING, "Illegal string offset '%s'", Z_STRVAL_P(dim));
			}
			break;
		case IS_DOUBLE:
		case IS_NULL:
		case IS_TRUE:
		case IS_FALSE:
			if (type != BP_VAR_IS) {
				zend_error(E_NOTICE, "String offset cast occurred");
			}
			break;
		default:
			zend_error(E_WARNING, "Illegal offset type");
			break;
	}

	return zval_get_long(dim);
}

ZEND_FASTCALL int zend_jit_helper_slow_fetch_address_obj(zval *container, zval *retval, zval *result) {
	if (UNEXPECTED(retval == &EG(uninitialized_zval))) {
		zend_class_entry *ce = Z_OBJCE_P(container);

		zend_error(E_NOTICE, "Indirect modification of overloaded element of %s has no effect", ce->name->val);
		return 0;
	} else if (EXPECTED(retval && Z_TYPE_P(retval) != IS_UNDEF)) {
		if (!Z_ISREF_P(retval)) {
			if (Z_REFCOUNTED_P(retval) &&
					Z_REFCOUNT_P(retval) > 1) {
				if (Z_TYPE_P(retval) != IS_OBJECT) {
					Z_DELREF_P(retval);
					ZVAL_DUP(result, retval);
					return 1;
				} else {
					ZVAL_COPY(result, retval);
					return 1;
				}
			}
			if (Z_TYPE_P(retval) != IS_OBJECT) {
				zend_class_entry *ce = Z_OBJCE_P(container);
				zend_error(E_NOTICE, "Indirect modification of overloaded element of %s has no effect", ce->name->val);
			}
		}

		if (retval != result) {
			return 2;
		}

		return 1;
	} else {
		return -1;
	}
}

ZEND_FASTCALL void zend_jit_helper_new_ref(zval *ref, zval* val) {
	zend_reference *_ref = emalloc(sizeof(zend_reference));
	GC_REFCOUNT(_ref) = 1;
	GC_TYPE_INFO(_ref) = IS_REFERENCE;
	ZVAL_COPY_VALUE(&_ref->val, val);
	Z_REF_P(ref) = _ref;
	Z_TYPE_INFO_P(ref) = IS_REFERENCE_EX;
}

ZEND_FASTCALL void zend_jit_helper_init_array(zval *zv, uint32_t size) {
	zend_array *_arr = emalloc(sizeof(zend_array));
	Z_ARR_P(zv) = _arr;	
	Z_TYPE_INFO_P(zv) = IS_ARRAY_EX;
	zend_hash_init(Z_ARRVAL_P(zv), size, NULL, ZVAL_PTR_DTOR, 0);
}

ZEND_FASTCALL int zend_jit_helper_slow_strlen(zend_execute_data *execute_data, zval *value, zval *ret) {
	zend_bool strict = EX_USES_STRICT_TYPES();

	if (EXPECTED(!strict)) {
		zend_string *str;
		zval tmp;

		ZVAL_COPY(&tmp, value);
		if (zend_parse_arg_str_weak(&tmp, &str)) {
			ZVAL_LONG(ret, ZSTR_LEN(str));
			zval_ptr_dtor(&tmp);
			return 1;
		}
		zval_ptr_dtor(&tmp);
	}
	zend_internal_type_error(strict, "strlen() expects parameter 1 to be string, %s given", zend_get_type_by_const(Z_TYPE_P(value)));
	ZVAL_NULL(ret);
	return 0;
}

ZEND_FASTCALL void zend_jit_helper_assign_to_string_offset(zval *str, zend_long offset, zval *value, zval *result){
	zend_string *old_str;
	zend_uchar c;
	size_t string_len;

	if (offset < 0) {
		zend_error(E_WARNING, "Illegal string offset:  " ZEND_LONG_FMT, offset);
		zend_string_release(Z_STR_P(str));
		if (result) {
			ZVAL_NULL(result);
		}
		return;
	}

	if (Z_TYPE_P(value) != IS_STRING) {
		/* Convert to string, just the time to pick the 1st byte */
		zend_string *tmp = zval_get_string(value);

		string_len = ZSTR_LEN(tmp);
		c = (zend_uchar)ZSTR_VAL(tmp)[0];
		zend_string_release(tmp);
	} else {
		string_len = Z_STRLEN_P(value);
		c = (zend_uchar)Z_STRVAL_P(value)[0];
	}

	if (string_len == 0) {
		/* Error on empty input string */
		zend_error(E_WARNING, "Cannot assign an empty string to a string offset");
		zend_string_release(Z_STR_P(str));
		if (result) {
			ZVAL_NULL(result);
		}
		return;
	}

	old_str = Z_STR_P(str);
	if ((size_t)offset >= Z_STRLEN_P(str)) {
		zend_long old_len = Z_STRLEN_P(str);
		Z_STR_P(str) = zend_string_realloc(Z_STR_P(str), offset + 1, 0);
		Z_TYPE_INFO_P(str) = IS_STRING_EX;
		memset(Z_STRVAL_P(str) + old_len, ' ', offset - old_len);
		Z_STRVAL_P(str)[offset+1] = 0;
	} else if (!Z_REFCOUNTED_P(str)) {
		Z_STR_P(str) = zend_string_init(Z_STRVAL_P(str), Z_STRLEN_P(str), 0);
		Z_TYPE_INFO_P(str) = IS_STRING_EX;
	}

	Z_STRVAL_P(str)[offset] = c;

	zend_string_release(old_str);
	if (result) {
		zend_uchar c = (zend_uchar)Z_STRVAL_P(str)[offset];

		if (CG(one_char_string)[c]) {
			ZVAL_INTERNED_STR(result, CG(one_char_string)[c]);
		} else {
			ZVAL_NEW_STR(result, zend_string_init(Z_STRVAL_P(str) + offset, 1, 0));
		}
	}
}

ZEND_FASTCALL zval* zend_jit_obj_proxy_add(zval *var_ptr, zval *value) {
	/* proxy object */
	zval rv;
	zval *objval = Z_OBJ_HANDLER_P(var_ptr, get)(var_ptr, &rv);
	Z_ADDREF_P(objval);
	add_function(objval, objval, value);
	Z_OBJ_HANDLER_P(var_ptr, set)(var_ptr, objval);
	zval_ptr_dtor(objval);
	return var_ptr;
}

ZEND_FASTCALL zval* zend_jit_obj_proxy_sub(zval *var_ptr, zval *value) {
	/* proxy object */
	zval rv;
	zval *objval = Z_OBJ_HANDLER_P(var_ptr, get)(var_ptr, &rv);
	Z_ADDREF_P(objval);
	sub_function(objval, objval, value);
	Z_OBJ_HANDLER_P(var_ptr, set)(var_ptr, objval);
	zval_ptr_dtor(objval);
	return var_ptr;
}

ZEND_FASTCALL zval* zend_jit_obj_proxy_mul(zval *var_ptr, zval *value) {
	/* proxy object */
	zval rv;
	zval *objval = Z_OBJ_HANDLER_P(var_ptr, get)(var_ptr, &rv);
	Z_ADDREF_P(objval);
	mul_function(objval, objval, value);
	Z_OBJ_HANDLER_P(var_ptr, set)(var_ptr, objval);
	zval_ptr_dtor(objval);
	return var_ptr;
}

ZEND_FASTCALL zval* zend_jit_obj_proxy_div(zval *var_ptr, zval *value) {
	/* proxy object */
	zval rv;
	zval *objval = Z_OBJ_HANDLER_P(var_ptr, get)(var_ptr, &rv);
	Z_ADDREF_P(objval);
	div_function(objval, objval, value);
	Z_OBJ_HANDLER_P(var_ptr, set)(var_ptr, objval);
	zval_ptr_dtor(objval);
	return var_ptr;
}

ZEND_FASTCALL zval* zend_jit_obj_proxy_concat(zval *var_ptr, zval *value) {
	/* proxy object */
	zval rv;
	zval *objval = Z_OBJ_HANDLER_P(var_ptr, get)(var_ptr, &rv);
	Z_ADDREF_P(objval);
	concat_function(objval, objval, value);
	Z_OBJ_HANDLER_P(var_ptr, set)(var_ptr, objval);
	zval_ptr_dtor(objval);
	return var_ptr;
}

ZEND_FASTCALL void zend_jit_helper_free_extra_args(zend_execute_data *call)
{
	uint32_t first_extra_arg = call->func->op_array.num_args;
	zval *end = ZEND_CALL_VAR_NUM(call, call->func->op_array.last_var + call->func->op_array.T);
	zval *p = end + (ZEND_CALL_NUM_ARGS(call) - first_extra_arg);

	do {
		p--;
		zval_ptr_dtor_nogc(p);
	} while (p != end);
}

ZEND_FASTCALL void zend_jit_helper_free_call_frame(void)
{
	zend_vm_stack p = EG(vm_stack);
	zend_vm_stack prev = p->prev;

	EG(vm_stack_top) = prev->top;
	EG(vm_stack_end) = prev->end;
	EG(vm_stack) = prev;
	efree(p);
}

ZEND_FASTCALL void* zend_jit_helper_arena_calloc(size_t size)
{
	void *ret = zend_arena_alloc(&CG(arena), size);
	memset(ret, 0, size); 
	return ret;
}

ZEND_FASTCALL void zend_jit_helper_wrong_string_offset(void)
{
	const char *msg = NULL;
	const zend_op *opline = EG(current_execute_data)->opline;
	const zend_op *end;
	uint32_t var;

	switch (opline->opcode) {
		case ZEND_ASSIGN_ADD:
		case ZEND_ASSIGN_SUB:
		case ZEND_ASSIGN_MUL:
		case ZEND_ASSIGN_DIV:
		case ZEND_ASSIGN_MOD:
		case ZEND_ASSIGN_SL:
		case ZEND_ASSIGN_SR:
		case ZEND_ASSIGN_CONCAT:
		case ZEND_ASSIGN_BW_OR:
		case ZEND_ASSIGN_BW_AND:
		case ZEND_ASSIGN_BW_XOR:
		case ZEND_ASSIGN_POW:
			msg = "Cannot use assign-op operators with string offsets";
			break;
		case ZEND_FETCH_DIM_W:
		case ZEND_FETCH_DIM_RW:
		case ZEND_FETCH_DIM_FUNC_ARG:
		case ZEND_FETCH_DIM_UNSET:
			/* TODO: Encode the "reason" into opline->extended_value??? */
			var = opline->result.var;
			opline++;
			end = EG(current_execute_data)->func->op_array.opcodes +
				EG(current_execute_data)->func->op_array.last;
			while (opline < end) {
				if (opline->op1_type == IS_VAR && opline->op1.var == var) {
					switch (opline->opcode) {
						case ZEND_ASSIGN_ADD:
						case ZEND_ASSIGN_SUB:
						case ZEND_ASSIGN_MUL:
						case ZEND_ASSIGN_DIV:
						case ZEND_ASSIGN_MOD:
						case ZEND_ASSIGN_SL:
						case ZEND_ASSIGN_SR:
						case ZEND_ASSIGN_CONCAT:
						case ZEND_ASSIGN_BW_OR:
						case ZEND_ASSIGN_BW_AND:
						case ZEND_ASSIGN_BW_XOR:
						case ZEND_ASSIGN_POW:
							if (opline->extended_value == ZEND_ASSIGN_OBJ) {
								msg = "Cannot use string offset as an object";
							} else if (opline->extended_value == ZEND_ASSIGN_DIM) {
								msg = "Cannot use string offset as an array";
							} else {
								msg = "Cannot use assign-op operators with string offsets";
							}
							break;
						case ZEND_PRE_INC_OBJ:
						case ZEND_PRE_DEC_OBJ:
						case ZEND_POST_INC_OBJ:
						case ZEND_POST_DEC_OBJ:
						case ZEND_PRE_INC:
						case ZEND_PRE_DEC:
						case ZEND_POST_INC:
						case ZEND_POST_DEC:
							msg = "Cannot increment/decrement string offsets";
							break;
						case ZEND_FETCH_DIM_W:
						case ZEND_FETCH_DIM_RW:
						case ZEND_FETCH_DIM_FUNC_ARG:
						case ZEND_FETCH_DIM_UNSET:
						case ZEND_ASSIGN_DIM:
							msg = "Cannot use string offset as an array";
							break;
						case ZEND_FETCH_OBJ_W:
						case ZEND_FETCH_OBJ_RW:
						case ZEND_FETCH_OBJ_FUNC_ARG:
						case ZEND_FETCH_OBJ_UNSET:
						case ZEND_ASSIGN_OBJ:
							msg = "Cannot use string offset as an object";
							break;
						case ZEND_ASSIGN_REF:
						case ZEND_ADD_ARRAY_ELEMENT:
						case ZEND_INIT_ARRAY:
						case ZEND_MAKE_REF:
							msg = "Cannot create references to/from string offsets";
							break;
						case ZEND_RETURN_BY_REF:
						case ZEND_VERIFY_RETURN_TYPE:
							msg = "Cannot return string offsets by reference";
							break;
						case ZEND_UNSET_DIM:
						case ZEND_UNSET_OBJ:
							msg = "Cannot unset string offsets";
							break;
						case ZEND_YIELD:
							msg = "Cannot yield string offsets by reference";
							break;
						case ZEND_SEND_REF:
						case ZEND_SEND_VAR_EX:
							msg = "Only variables can be passed by reference";
							break;
						EMPTY_SWITCH_DEFAULT_CASE();
					}
					break;
				}
				if (opline->op2_type == IS_VAR && opline->op2.var == var) {
					ZEND_ASSERT(opline->opcode == ZEND_ASSIGN_REF);
					msg = "Cannot create references to/from string offsets";
					break;
				}
			}
			break;
		EMPTY_SWITCH_DEFAULT_CASE();
	}
	ZEND_ASSERT(msg != NULL);
	zend_throw_error(NULL, msg);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
