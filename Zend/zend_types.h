/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2018 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   |          Dmitry Stogov <dmitry@zend.com>                             |
   |          Xinchen Hui <xinchen.h@zend.com>                            |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef ZEND_TYPES_H
#define ZEND_TYPES_H

#include "zend_portability.h"
#include "zend_long.h"

#ifdef WORDS_BIGENDIAN
# define ZEND_ENDIAN_LOHI(lo, hi)          hi; lo;
# define ZEND_ENDIAN_LOHI_3(lo, mi, hi)    hi; mi; lo;
# define ZEND_ENDIAN_LOHI_4(a, b, c, d)    d; c; b; a;
# define ZEND_ENDIAN_LOHI_C(lo, hi)        hi, lo
# define ZEND_ENDIAN_LOHI_C_3(lo, mi, hi)  hi, mi, lo,
# define ZEND_ENDIAN_LOHI_C_4(a, b, c, d)  d, c, b, a
#else
# define ZEND_ENDIAN_LOHI(lo, hi)          lo; hi;
# define ZEND_ENDIAN_LOHI_3(lo, mi, hi)    lo; mi; hi;
# define ZEND_ENDIAN_LOHI_4(a, b, c, d)    a; b; c; d;
# define ZEND_ENDIAN_LOHI_C(lo, hi)        lo, hi
# define ZEND_ENDIAN_LOHI_C_3(lo, mi, hi)  lo, mi, hi,
# define ZEND_ENDIAN_LOHI_C_4(a, b, c, d)  a, b, c, d
#endif

typedef unsigned char zend_bool;
typedef unsigned char zend_uchar;

typedef enum {
  SUCCESS =  0,
  FAILURE = -1,		/* this MUST stay a negative number, or it may affect functions! */
} ZEND_RESULT_CODE;

#ifdef ZEND_ENABLE_ZVAL_LONG64
# ifdef ZEND_WIN32
#  define ZEND_SIZE_MAX  _UI64_MAX
# else
#  define ZEND_SIZE_MAX  SIZE_MAX
# endif
#else
# if defined(ZEND_WIN32)
#  define ZEND_SIZE_MAX  _UI32_MAX
# else
#  define ZEND_SIZE_MAX SIZE_MAX
# endif
#endif

typedef intptr_t zend_intptr_t;
typedef uintptr_t zend_uintptr_t;

#ifdef ZTS
#define ZEND_TLS static TSRM_TLS
#define ZEND_EXT_TLS TSRM_TLS
#else
#define ZEND_TLS static
#define ZEND_EXT_TLS
#endif

typedef struct _zend_object_handlers zend_object_handlers;
typedef struct _zend_class_entry     zend_class_entry;
typedef union  _zend_function        zend_function;
typedef struct _zend_execute_data    zend_execute_data;

#if !ZEND_NAN_TAG
typedef struct _zval_struct     zval;
#else
typedef union  _zval_struct     zval;
#endif

typedef struct _zend_refcounted zend_refcounted;
typedef struct _zend_string     zend_string;
typedef struct _zend_array      zend_array;
typedef struct _zend_object     zend_object;
typedef struct _zend_resource   zend_resource;
typedef struct _zend_reference  zend_reference;
typedef struct _zend_ast_ref    zend_ast_ref;
typedef struct _zend_ast        zend_ast;

typedef int  (*compare_func_t)(const void *, const void *);
typedef void (*swap_func_t)(void *, void *);
typedef void (*sort_func_t)(void *, size_t, size_t, compare_func_t, swap_func_t);
typedef void (*dtor_func_t)(zval *pDest);
typedef void (*copy_ctor_func_t)(zval *pElement);

/*
 * zend_type - is an abstraction layer to represent information about type hint.
 * It shouldn't be used directly. Only through ZEND_TYPE_* macros.
 *
 * ZEND_TYPE_IS_SET()     - checks if type-hint exists
 * ZEND_TYPE_IS_CODE()    - checks if type-hint refer to standard type
 * ZEND_TYPE_IS_CLASS()   - checks if type-hint refer to some class
 *
 * ZEND_TYPE_NAME()       - returns referenced class name
 * ZEND_TYPE_CE()         - returns referenced class entry
 * ZEND_TYPE_CODE()       - returns standard type code (e.g. IS_LONG, _IS_BOOL)
 *
 * ZEND_TYPE_ALLOW_NULL() - checks if NULL is allowed
 *
 * ZEND_TYPE_ENCODE() and ZEND_TYPE_ENCODE_CLASS() should be used for
 * construction.
 */

typedef uintptr_t zend_type;

#define ZEND_TYPE_IS_SET(t) \
	((t) > Z_L(1))

#define ZEND_TYPE_IS_CODE(t) \
	(((t) > Z_L(1)) && ((t) <= Z_L(0x1ff)))

#define ZEND_TYPE_IS_CLASS(t) \
	((t) > Z_L(0x1ff))

#define ZEND_TYPE_NAME(t) \
	((zend_string*)((t) & ~Z_L(0x3)))

#define ZEND_TYPE_CE(t) \
	((zend_class_entry*)((t) & ~Z_L(0x3)))

#define ZEND_TYPE_CODE(t) \
	((t) >> Z_L(1))

#define ZEND_TYPE_ALLOW_NULL(t) \
	(((t) & Z_L(0x1)) != 0)

#define ZEND_TYPE_ENCODE(code, allow_null) \
	(((code) << Z_L(1)) | ((allow_null) ? Z_L(0x1) : Z_L(0x0)))

#define ZEND_TYPE_ENCODE_CLASS(class_name, allow_null) \
	(((uintptr_t)(class_name)) | ((allow_null) ? Z_L(0x1) : Z_L(0)))

#define ZEND_TYPE_ENCODE_CLASS_CONST_0(class_name) \
	((zend_type) class_name)
#define ZEND_TYPE_ENCODE_CLASS_CONST_1(class_name) \
	((zend_type) "?" class_name)
#define ZEND_TYPE_ENCODE_CLASS_CONST_Q2(macro, class_name) \
	macro(class_name)
#define ZEND_TYPE_ENCODE_CLASS_CONST_Q1(allow_null, class_name) \
	ZEND_TYPE_ENCODE_CLASS_CONST_Q2(ZEND_TYPE_ENCODE_CLASS_CONST_ ##allow_null, class_name)
#define ZEND_TYPE_ENCODE_CLASS_CONST(class_name, allow_null) \
	ZEND_TYPE_ENCODE_CLASS_CONST_Q1(allow_null, class_name)

typedef union _zend_value {
#if !ZEND_NAN_TAG || SIZEOF_ZEND_LONG == 4
	zend_long         lval;				/* long value */
#endif
#if !ZEND_NAN_TAG
	double            dval;				/* double value */
#endif
#if !ZEND_NAN_TAG_64
	zend_refcounted  *counted;
	zend_string      *str;
	zend_array       *arr;
	zend_object      *obj;
	zend_resource    *res;
	zend_reference   *ref;
	zend_ast_ref     *ast;
	zval             *zv;
	void             *ptr;
	zend_class_entry *ce;
	zend_function    *func;
#endif
#if !ZEND_NAN_TAG
	struct {
		uint32_t w1;
		uint32_t w2;
	} ww;
#else
	uint32_t          fe_pos;           /* Used by FE_RESET/FE_FETCH */
	uint32_t          fe_iter_idx;      /* Used by FW_RESET/FE_FETCH */
	uint32_t          opline_num;       /* Used by FAST_CALL  */
	uint32_t          property_guard;   /* single property guard */
#endif
} zend_value;

#if !ZEND_NAN_TAG
struct _zval_struct {
	zend_value        value;			/* value */
	union {
		struct {
			ZEND_ENDIAN_LOHI_3(
				zend_uchar    type,			/* active type */
				zend_uchar    type_flags,
				union {
					uint16_t  call_info;    /* call info for EX(This) */
					uint16_t  extra;        /* not further specified */
				} u)
		} v;
		uint32_t type_info;
	} u1;
	union {
		uint32_t     next;                 /* hash collision chain */
		uint32_t     opline_num;           /* opline number (for FAST_CALL) */
		uint32_t     num_args;             /* arguments number for EX(This) */
		uint32_t     fe_pos;               /* foreach position */
		uint32_t     fe_iter_idx;          /* foreach iterator index */
		uint32_t     access_flags;         /* class constant access flags */
		uint32_t     property_guard;       /* single property guard */
	} u2;
};
#else
union _zval_struct {
	uint64_t          u64;
	int64_t           i64;
	double            dval;				/* double value */
	struct {
		ZEND_ENDIAN_LOHI(
			zend_value        value,			/* value */
			union {
#if 0
				struct {
					ZEND_ENDIAN_LOHI_3(
						zend_uchar    type,			/* active type */
						zend_uchar    _unused_byte,
						uint16_t      _unused_word
					)
				} v;
#endif
				uint32_t type_info;
			} u1;
		)
	};
};
#endif

typedef struct _zend_refcounted_h {
	uint32_t         refcount;			/* reference counter 32-bit */
	union {
		struct {
			ZEND_ENDIAN_LOHI_3(
				zend_uchar    type,
				zend_uchar    flags,    /* used for strings & objects */
				uint16_t      gc_info)  /* keeps GC root number (or 0) and color */
		} v;
		uint32_t type_info;
	} u;
} zend_refcounted_h;

struct _zend_refcounted {
	zend_refcounted_h gc;
};

struct _zend_string {
	zend_refcounted_h gc;
	zend_ulong        h;                /* hash value */
	size_t            len;
	char              val[1];
};

typedef struct _Bucket {
	zval              val;
	zend_string      *key;              /* string key or NULL for numerics */
	zend_ulong        h;                /* hash value (or numeric index)   */
#if ZEND_NAN_TAG
	uint32_t          next;
#endif
} Bucket;

typedef struct _zend_array HashTable;

struct _zend_array {
	zend_refcounted_h gc;
	union {
		struct {
			ZEND_ENDIAN_LOHI_4(
				zend_uchar    flags,
				zend_uchar    _unused,
				zend_uchar    nIteratorsCount,
				zend_uchar    _unused2)
		} v;
		uint32_t flags;
	} u;
	uint32_t          nTableMask;
	Bucket           *arData;
	uint32_t          nNumUsed;
	uint32_t          nNumOfElements;
	uint32_t          nTableSize;
	uint32_t          nInternalPointer;
	zend_long         nNextFreeElement;
	dtor_func_t       pDestructor;
};

/*
 * HashTable Data Layout
 * =====================
 *
 *                 +=============================+
 *                 | HT_HASH(ht, ht->nTableMask) |
 *                 | ...                         |
 *                 | HT_HASH(ht, -1)             |
 *                 +-----------------------------+
 * ht->arData ---> | Bucket[0]                   |
 *                 | ...                         |
 *                 | Bucket[ht->nTableSize-1]    |
 *                 +=============================+
 */

#define HT_INVALID_IDX ((uint32_t) -1)

#define HT_MIN_MASK ((uint32_t) -2)
#define HT_MIN_SIZE 8

#if SIZEOF_SIZE_T == 4
# define HT_MAX_SIZE 0x04000000 /* small enough to avoid overflow checks */
# define HT_HASH_TO_BUCKET_EX(data, idx) \
	((Bucket*)((char*)(data) + (idx)))
# define HT_IDX_TO_HASH(idx) \
	((idx) * sizeof(Bucket))
# define HT_HASH_TO_IDX(idx) \
	((idx) / sizeof(Bucket))
#elif SIZEOF_SIZE_T == 8
# define HT_MAX_SIZE 0x80000000
# define HT_HASH_TO_BUCKET_EX(data, idx) \
	((data) + (idx))
# define HT_IDX_TO_HASH(idx) \
	(idx)
# define HT_HASH_TO_IDX(idx) \
	(idx)
#else
# error "Unknown SIZEOF_SIZE_T"
#endif

#define HT_HASH_EX(data, idx) \
	((uint32_t*)(data))[(int32_t)(idx)]
#define HT_HASH(ht, idx) \
	HT_HASH_EX((ht)->arData, idx)

#define HT_HASH_SIZE(nTableMask) \
	(((size_t)(uint32_t)-(int32_t)(nTableMask)) * sizeof(uint32_t))
#define HT_DATA_SIZE(nTableSize) \
	((size_t)(nTableSize) * sizeof(Bucket))
#define HT_SIZE_EX(nTableSize, nTableMask) \
	(HT_DATA_SIZE((nTableSize)) + HT_HASH_SIZE((nTableMask)))
#define HT_SIZE(ht) \
	HT_SIZE_EX((ht)->nTableSize, (ht)->nTableMask)
#define HT_USED_SIZE(ht) \
	(HT_HASH_SIZE((ht)->nTableMask) + ((size_t)(ht)->nNumUsed * sizeof(Bucket)))
#define HT_HASH_RESET(ht) \
	memset(&HT_HASH(ht, (ht)->nTableMask), HT_INVALID_IDX, HT_HASH_SIZE((ht)->nTableMask))
#define HT_HASH_RESET_PACKED(ht) do { \
		HT_HASH(ht, -2) = HT_INVALID_IDX; \
		HT_HASH(ht, -1) = HT_INVALID_IDX; \
	} while (0)
#define HT_HASH_TO_BUCKET(ht, idx) \
	HT_HASH_TO_BUCKET_EX((ht)->arData, idx)

#define HT_SET_DATA_ADDR(ht, ptr) do { \
		(ht)->arData = (Bucket*)(((char*)(ptr)) + HT_HASH_SIZE((ht)->nTableMask)); \
	} while (0)
#define HT_GET_DATA_ADDR(ht) \
	((char*)((ht)->arData) - HT_HASH_SIZE((ht)->nTableMask))

typedef uint32_t HashPosition;

typedef struct _HashTableIterator {
	HashTable    *ht;
	HashPosition  pos;
} HashTableIterator;

struct _zend_object {
	zend_refcounted_h gc;
	uint32_t          handle; // TODO: may be removed ???
	zend_class_entry *ce;
	const zend_object_handlers *handlers;
	HashTable        *properties;
	zval              properties_table[1];
};

struct _zend_resource {
	zend_refcounted_h gc;
	int               handle; // TODO: may be removed ???
	int               type;
	void             *ptr;
};

struct _zend_reference {
	zend_refcounted_h gc;
	zval              val;
};

struct _zend_ast_ref {
	zend_refcounted_h gc;
	/*zend_ast        ast; zend_ast follows the zend_ast_ref structure */
};

/* regular data types */
#define IS_UNDEF					0
#define IS_NULL						1
#define IS_FALSE					2
#define IS_TRUE						3
#define IS_LONG						4
#define IS_DOUBLE					5
#define IS_STRING					6
#define IS_ARRAY					7
#define IS_OBJECT					8
#define IS_RESOURCE					9
#define IS_REFERENCE				10

/* constant expressions */
#define IS_CONSTANT_AST				11

/* internal types */
#define IS_INDIRECT             	12
#define IS_PTR						13
#define _IS_ERROR					13

#define IS_RESERVE_1				14
#define IS_RESERVE_2				15

/* fake types used only for type hinting (Z_TYPE(zv) can not use them) */
#define _IS_BOOL					16
#define IS_CALLABLE					17
#define IS_ITERABLE					18
#define IS_VOID						19
#define _IS_NUMBER					20

#if ZEND_NAN_TAG
# if ZEND_NAN_TAG_32
#  define IS_TYPE_REFCOUNTED		(1<<4)

#  define Z_TYPE_MASK				0x0f
#  define Z_TYPE_FLAGS_SHIFT		0

#  define Z_RAW_TYPE_INFO(zv)       (zv).u1.type_info
#  define Z_RAW_TYPE(zv)            (Z_RAW_TYPE_INFO(zv) & (uint32_t)~IS_TYPE_REFCOUNTED)

#  define Z_TYPE_TO_RAW(t)			((uint32_t)(t)-32)
#  define Z_TYPE_TO_RAW_WORD(t)		((uint32_t)(t)-32)
#  define Z_RAW_TO_TYPE(t)			((uint32_t)(t)+32)

#  define ZVAL_UNDEF_INITIALIZER	{.u64=0xffffffef00000000LL}

#  define zend_raw_type				uint32_t
# elif ZEND_NAN_TAG_64
#  define IS_TYPE_REFCOUNTED		(1<<4)

#  define Z_TYPE_MASK				0x0f
#  define Z_TYPE_FLAGS_SHIFT		0

#  define Z_RAW_TYPE_INFO(zv)       ((uint64_t)((zv).i64 >> 47))
#  define Z_RAW_TYPE(zv)            (Z_RAW_TYPE_INFO(zv) | (uint64_t)IS_TYPE_REFCOUNTED)

#  define Z_TYPE_TO_RAW(t)			((uint64_t)~(t))
#  define Z_TYPE_TO_RAW_WORD(t)		(((uint32_t)~(t)) << (47 - 32))
#  define Z_RAW_TO_TYPE(t)			((uint64_t)~(t))

#  define ZVAL_UNDEF_INITIALIZER	{.u64=0xfff0000000000000LL}

#  define zend_raw_type				uint64_t
# else
#  error "Unknown NaN tagging"
# endif
#else
# define IS_TYPE_REFCOUNTED			(1<<0)

# define Z_TYPE_MASK				0xff
# define Z_TYPE_FLAGS_SHIFT			8

# define Z_RAW_TYPE_INFO(zv)       (zv).u1.type_info
# define Z_RAW_TYPE(zv)            (zv).u1.v.type

# define Z_TYPE_TO_RAW(t)			(t)
# define Z_TYPE_TO_RAW_WORD(t)		(t)
# define Z_RAW_TO_TYPE(t)			(t)

# define ZVAL_UNDEF_INITIALIZER		{{0}, {{0}}, {0}}

# define zend_raw_type				uint32_t
#endif

#define Z_RAW_TYPE_INFO_P(zval_p)	Z_RAW_TYPE_INFO(*(zval_p))
#define Z_RAW_TYPE_P(zval_p)		Z_RAW_TYPE(*(zval_p))

static zend_always_inline void zval_set_type_info(zval* pz, uint32_t type_info) {
#if ZEND_NAN_TAG_32
	if (type_info != IS_DOUBLE) {
		pz->u1.type_info = Z_TYPE_TO_RAW(type_info);
	}
#elif ZEND_NAN_TAG_64
	if (type_info != IS_DOUBLE) {
		ZEND_ASSERT(type_info <= IS_LONG || type_info == _IS_ERROR || type_info == IS_RESERVE_1 || type_info == IS_RESERVE_2);
		pz->u1.type_info = Z_TYPE_TO_RAW_WORD(type_info);
	}
#else
	pz->u1.type_info = type_info;
#endif
}

static zend_always_inline void zval_set_ptr(zval* pz, uint32_t type_info, void *ptr) {
#if ZEND_NAN_TAG_64
	pz->u64 = ((uint64_t)Z_TYPE_TO_RAW(type_info) << 47) | (uint64_t)(uintptr_t)ptr;
#else
	pz->value.ptr = ptr;
	pz->u1.type_info = Z_TYPE_TO_RAW(type_info);
#endif
}

static zend_always_inline void zval_set_ptr2(zval* pz, uint32_t type_info, void *ptr) {
#if ZEND_NAN_TAG_64
	pz->u64 = ((uint64_t)Z_TYPE_TO_RAW(type_info) << 47) | (uint64_t)(intptr_t)ptr;
#else
	pz->value.ptr = ptr;
#endif
}

static zend_always_inline void* zval_get_ptr(const zval* pz) {
#if ZEND_NAN_TAG_64
	return (void*)(pz->u64 & 0x00007fffffffffffLL);
#else
	return (void*)pz->value.ptr;
#endif
}

#if ZEND_NAN_TAG_32
static zend_always_inline zend_bool zend_type_is_double(uint32_t type_info) {
	return type_info < 0xffffffe0;
}
#elif ZEND_NAN_TAG_64
static zend_always_inline zend_bool zend_type_is_double(uint64_t type_info) {
	return type_info <= 0xffffffffffffffe0LL || type_info == 0xfffffffffffffff0;
}
#endif

#if ZEND_NAN_TAG_32
static zend_always_inline zend_bool zend_type_is_refcounted(uint32_t type_info) {
	return type_info >= (uint32_t)~Z_TYPE_MASK;
}
#elif ZEND_NAN_TAG_64
static zend_always_inline zend_bool zend_type_is_refcounted(uint64_t type_info) {
	return type_info > 0xffffffffffffffe0LL && type_info < 0xffffffffffffffefLL;
}
#else
static zend_always_inline zend_bool zend_type_is_refcounted(uint32_t type_info) {
	return (type_info & (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) != 0;
}
#endif

static zend_always_inline zend_uchar zval_get_type(const zval* pz) {
#if ZEND_NAN_TAG_64
	return zend_type_is_double(Z_RAW_TYPE_INFO_P(pz)) ? IS_DOUBLE : (~Z_RAW_TYPE_INFO_P(pz) & Z_TYPE_MASK);
#elif ZEND_NAN_TAG_32
	return zend_type_is_double(Z_RAW_TYPE_INFO_P(pz)) ? IS_DOUBLE : (Z_RAW_TYPE_INFO_P(pz) & Z_TYPE_MASK);
#else
	return pz->u1.v.type;
#endif
}

static zend_always_inline zend_uchar zval_get_type_flags(const zval* pz) {
#if ZEND_NAN_TAG_64
	return zend_type_is_double(Z_RAW_TYPE_INFO_P(pz)) ? 0 : (Z_RAW_TYPE_INFO_P(pz) ^ IS_TYPE_REFCOUNTED);
#elif ZEND_NAN_TAG_32
	return zend_type_is_double(Z_RAW_TYPE_INFO_P(pz)) ? 0 : (Z_RAW_TYPE_INFO_P(pz) & IS_TYPE_REFCOUNTED);
#else
	return pz->u1.v.type_flags;
#endif
}

static zend_always_inline uint32_t zval_get_type_info(const zval* pz) {
#if ZEND_NAN_TAG_64
	return zend_type_is_double(Z_RAW_TYPE_INFO_P(pz)) ? IS_DOUBLE : ~Z_RAW_TO_TYPE(pz->u1.type_info);
#elif ZEND_NAN_TAG_32
	return zend_type_is_double(Z_RAW_TYPE_INFO_P(pz)) ? IS_DOUBLE : Z_RAW_TO_TYPE(pz->u1.type_info);
#else
	return pz->u1.type_info;
#endif
}

#define ZEND_SAME_FAKE_TYPE(faketype, realtype) ( \
	(faketype) == (realtype) \
	|| ((faketype) == _IS_BOOL && ((realtype) == IS_TRUE || (realtype) == IS_FALSE)) \
)

/* we should never set just Z_TYPE, we should set Z_TYPE_INFO */
#define Z_TYPE(zval)				zval_get_type(&(zval))
#define Z_TYPE_P(zval_p)			Z_TYPE(*(zval_p))

#define Z_TYPE_FLAGS(zval)			zval_get_type_flags(&(zval))
#define Z_TYPE_FLAGS_P(zval_p)		Z_TYPE_FLAGS(*(zval_p))

#define Z_TYPE_INFO(zval)			zval_get_type_info(&(zval))
#define Z_TYPE_INFO_P(zval_p)		Z_TYPE_INFO(*(zval_p))

#define Z_SET_TYPE_INFO(zv, t)		zval_set_type_info(&(zv), (t))
#define Z_SET_TYPE_INFO_P(pzv, t)	Z_SET_TYPE_INFO(*(pzv), (t))

#define Z_SET_PTR(zv, t, p)			zval_set_ptr(&(zv), (t), (p))
#define Z_SET_PTR_P(pzv, t, p)		Z_SET_PTR(*(pzv), (t), (p))
#define Z_SET_PTR2(zv, t, p)		zval_set_ptr2(&(zv), (t), (p))
#define Z_SET_PTR2_P(pzv, t, p)		Z_SET_PTR2(*(pzv), (t), (p))

#if ZEND_NAN_TAG
# define Z_NEXT(zval)				(((Bucket*)(&(zval)))->next)
#else
# define Z_NEXT(zval)				(zval).u2.next
#endif
#define Z_NEXT_P(zval_p)			Z_NEXT(*(zval_p))

#if ZEND_NAN_TAG
# define Z_FE_POS(zval)				((&zval)+1)->value.fe_pos
#else
# define Z_FE_POS(zval)				(zval).u2.fe_pos
#endif
#define Z_FE_POS_P(zval_p)			Z_FE_POS(*(zval_p))

#if ZEND_NAN_TAG
# define Z_FE_ITER(zval)			((&zval)+1)->value.fe_iter_idx
#else
# define Z_FE_ITER(zval)			(zval).u2.fe_iter_idx
#endif
#define Z_FE_ITER_P(zval_p)			Z_FE_ITER(*(zval_p))

#if ZEND_NAN_TAG
# define Z_OPLINE_NUM(zval)			((&zval)+1)->value.opline_num
#else
# define Z_OPLINE_NUM(zval)			(zval).u2.opline_num
#endif
#define Z_OPLINE_NUM_P(zval_p)		Z_OPLINE_NUM(*(zval_p))

#if ZEND_NAN_TAG
# define Z_ACCESS_FLAGS(zval)		(((zend_class_constant*)(&(zval)))->access_flags)
#else
# define Z_ACCESS_FLAGS(zval)		(zval).u2.access_flags
#endif
#define Z_ACCESS_FLAGS_P(zval_p)	Z_ACCESS_FLAGS(*(zval_p))

#if ZEND_NAN_TAG
# define Z_PROPERTY_GUARD(zval)		((&zval)+1)->value.property_guard
#else
# define Z_PROPERTY_GUARD(zval)		(zval).u2.property_guard
#endif
#define Z_PROPERTY_GUARD_P(zval_p)	Z_PROPERTY_GUARD(*(zval_p))

#define Z_PTR(zval)					zval_get_ptr(&(zval))
#define Z_PTR_P(zval_p)				Z_PTR(*(zval_p))

#define Z_COUNTED(zval)				((zend_refcounted*)Z_PTR(zval))
#define Z_COUNTED_P(zval_p)			Z_COUNTED(*(zval_p))

#define GC_REFCOUNT(p)				zend_gc_refcount(&(p)->gc)
#define GC_SET_REFCOUNT(p, rc)		zend_gc_set_refcount(&(p)->gc, rc)
#define GC_ADDREF(p)				zend_gc_addref(&(p)->gc)
#define GC_DELREF(p)				zend_gc_delref(&(p)->gc)
#define GC_ADDREF_EX(p, rc)			zend_gc_addref_ex(&(p)->gc, rc)
#define GC_DELREF_EX(p, rc)			zend_gc_delref_ex(&(p)->gc, rc)

#define GC_TYPE(p)					(p)->gc.u.v.type
#define GC_FLAGS(p)					(p)->gc.u.v.flags
#define GC_INFO(p)					(p)->gc.u.v.gc_info
#define GC_TYPE_INFO(p)				(p)->gc.u.type_info

#define Z_GC_TYPE(zval)				GC_TYPE(Z_COUNTED(zval))
#define Z_GC_TYPE_P(zval_p)			Z_GC_TYPE(*(zval_p))

#define Z_GC_FLAGS(zval)			GC_FLAGS(Z_COUNTED(zval))
#define Z_GC_FLAGS_P(zval_p)		Z_GC_FLAGS(*(zval_p))

#define Z_GC_INFO(zval)				GC_INFO(Z_COUNTED(zval))
#define Z_GC_INFO_P(zval_p)			Z_GC_INFO(*(zval_p))
#define Z_GC_TYPE_INFO(zval)		GC_TYPE_INFO(Z_COUNTED(zval))
#define Z_GC_TYPE_INFO_P(zval_p)	Z_GC_TYPE_INFO(*(zval_p))

#define GC_FLAGS_SHIFT				8
#define GC_INFO_SHIFT				16
#define GC_INFO_MASK				0xffff0000

/* zval.value->gc.u.v.flags (common flags) */
#define GC_COLLECTABLE				(1<<0)
#define GC_PROTECTED                (1<<1) /* used for recursion detection */
#define GC_IMMUTABLE                (1<<2) /* can't be canged in place */
#define GC_PERSISTENT               (1<<3) /* allocated using malloc */
#define GC_PERSISTENT_LOCAL         (1<<4) /* persistent, but thread-local */

#define GC_ARRAY					(IS_ARRAY          | (GC_COLLECTABLE << GC_FLAGS_SHIFT))
#define GC_OBJECT					(IS_OBJECT         | (GC_COLLECTABLE << GC_FLAGS_SHIFT))

/* extended types */
#define IS_INTERNED_STRING_EX		IS_STRING

#define IS_STRING_EX				(IS_STRING         | (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT))
#define IS_ARRAY_EX					(IS_ARRAY          | (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT))
#define IS_OBJECT_EX				(IS_OBJECT         | (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT))
#define IS_RESOURCE_EX				(IS_RESOURCE       | (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT))
#define IS_REFERENCE_EX				(IS_REFERENCE      | (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT))

#define IS_CONSTANT_AST_EX			(IS_CONSTANT_AST   | (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT))

/* string flags (zval.value->gc.u.flags) */
#define IS_STR_INTERNED				GC_IMMUTABLE  /* interned string */
#define IS_STR_PERSISTENT			GC_PERSISTENT /* allocated using malloc */
#define IS_STR_PERMANENT        	(1<<5)        /* relives request boundary */

/* array flags */
#define IS_ARRAY_IMMUTABLE			GC_IMMUTABLE
#define IS_ARRAY_PERSISTENT			GC_PERSISTENT

/* object flags (zval.value->gc.u.flags) */
#define IS_OBJ_DESTRUCTOR_CALLED	(1<<4)
#define IS_OBJ_FREE_CALLED			(1<<5)
#define IS_OBJ_USE_GUARDS           (1<<6)
#define IS_OBJ_HAS_GUARDS           (1<<7)

#define OBJ_FLAGS(obj)              GC_FLAGS(obj)

/* Recursion protection macros must be used only for arrays and objects */
#define GC_IS_RECURSIVE(p) \
	(GC_FLAGS(p) & GC_PROTECTED)

#define GC_PROTECT_RECURSION(p) do { \
		GC_FLAGS(p) |= GC_PROTECTED; \
	} while (0)

#define GC_UNPROTECT_RECURSION(p) do { \
		GC_FLAGS(p) &= ~GC_PROTECTED; \
	} while (0)

#define Z_IS_RECURSIVE(zval)        GC_IS_RECURSIVE(Z_COUNTED(zval))
#define Z_PROTECT_RECURSION(zval)   GC_PROTECT_RECURSION(Z_COUNTED(zval))
#define Z_UNPROTECT_RECURSION(zval) GC_UNPROTECT_RECURSION(Z_COUNTED(zval))
#define Z_IS_RECURSIVE_P(zv)        Z_IS_RECURSIVE(*(zv))
#define Z_PROTECT_RECURSION_P(zv)   Z_PROTECT_RECURSION(*(zv))
#define Z_UNPROTECT_RECURSION_P(zv) Z_UNPROTECT_RECURSION(*(zv))

/* Type checks */
#if ZEND_NAN_TAG
# define T_IS_UNDEF(t)				((t) == Z_TYPE_TO_RAW(IS_UNDEF))
# define T_IS_NULL(t)				((t) == Z_TYPE_TO_RAW(IS_NULL))
# define T_IS_FALSE(t)				((t) == Z_TYPE_TO_RAW(IS_FALSE))
# define T_IS_TRUE(t)				((t) == Z_TYPE_TO_RAW(IS_TRUE))
# define T_IS_LONG(t)				((t) == Z_TYPE_TO_RAW(IS_LONG))
# define T_IS_DOUBLE(t)				zend_type_is_double(t)
#if ZEND_NAN_TAG_32
# define T_IS_STRING(t)				(((t) & (uint32_t)~(IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) == Z_TYPE_TO_RAW(IS_STRING))
# define T_IS_ARRAY(t)				(((t) & (uint32_t)~(IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) == Z_TYPE_TO_RAW(IS_ARRAY))
#elif ZEND_NAN_TAG_64
# define T_IS_STRING(t)				(((t) | IS_TYPE_REFCOUNTED) == Z_TYPE_TO_RAW(IS_STRING))
# define T_IS_ARRAY(t)				(((t) | IS_TYPE_REFCOUNTED) == Z_TYPE_TO_RAW(IS_ARRAY))
#endif
# define T_IS_OBJECT(t)				((t) == Z_TYPE_TO_RAW(IS_OBJECT_EX))
# define T_IS_RESOURCE(t)			((t) == Z_TYPE_TO_RAW(IS_RESOURCE_EX))
# define T_IS_REFERENCE(t)			((t) == Z_TYPE_TO_RAW(IS_REFERENCE_EX))
#if ZEND_NAN_TAG_32
#  define T_IS_CONSTANT(t)			(((t) & (uint32_t)~(IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) == Z_TYPE_TO_RAW(IS_CONSTANT_AST))
#elif ZEND_NAN_TAG_64
#  define T_IS_CONSTANT(t)			(((t) | IS_TYPE_REFCOUNTED) == Z_TYPE_TO_RAW(IS_CONSTANT_AST))
#endif
# define T_IS_INDIRECT(t)			((t) == Z_TYPE_TO_RAW(IS_INDIRECT))
# define T_IS_PTR(t)				((t) == Z_TYPE_TO_RAW(IS_PTR))
# define T_IS_ERROR(t)				((t) == Z_TYPE_TO_RAW(_IS_ERROR))
#else
# define T_IS_UNDEF(t)				(((t) & Z_TYPE_MASK) == IS_UNDEF)
# define T_IS_NULL(t)				(((t) & Z_TYPE_MASK) == IS_NULL)
# define T_IS_FALSE(t)				(((t) & Z_TYPE_MASK) == IS_FALSE)
# define T_IS_TRUE(t)				(((t) & Z_TYPE_MASK) == IS_TRUE)
# define T_IS_LONG(t)				(((t) & Z_TYPE_MASK) == IS_LONG)
# define T_IS_DOUBLE(t)				(((t) & Z_TYPE_MASK) == IS_DOUBLE)
# define T_IS_STRING(t)				(((t) & Z_TYPE_MASK) == IS_STRING)
# define T_IS_ARRAY(t)				(((t) & Z_TYPE_MASK) == IS_ARRAY)
# define T_IS_OBJECT(t)				(((t) & Z_TYPE_MASK) == IS_OBJECT)
# define T_IS_RESOURCE(t)			(((t) & Z_TYPE_MASK) == IS_RESOURCE)
# define T_IS_REFERENCE(t)			(((t) & Z_TYPE_MASK) == IS_REFERENCE)
# define T_IS_CONSTANT(t)			(((t) & Z_TYPE_MASK) == IS_CONSTANT_AST)
# define T_IS_INDIRECT(t)			(((t) & Z_TYPE_MASK) == IS_INDIRECT)
# define T_IS_PTR(t)				(((t) & Z_TYPE_MASK) == IS_PTR)
# define T_IS_ERROR(t)				(((t) & Z_TYPE_MASK) == _IS_ERROR)
#endif

#if ZEND_NAN_TAG
  // IS_UNDEF, IS_NULL, IS_FALSE or IS_TRUE
# if ZEND_NAN_TAG_32
#  define T_IS_PRIMITIVE(t)			((t) <= Z_TYPE_TO_RAW(IS_TRUE) && (t) >= Z_TYPE_TO_RAW(IS_UNDEF))
# elif ZEND_NAN_TAG_64
#  define T_IS_PRIMITIVE(t)			((t) >= Z_TYPE_TO_RAW(IS_TRUE))
# endif
  // IS_UNDEF, IS_NULL or IS_FALSE
# if ZEND_NAN_TAG_32
#  define T_IS_LESS_THAN_TRUE(t)	((t) < Z_TYPE_TO_RAW(IS_TRUE) && (t) >= Z_TYPE_TO_RAW(IS_UNDEF))
# elif ZEND_NAN_TAG_64
#  define T_IS_LESS_THAN_TRUE(t)	((t) > Z_TYPE_TO_RAW(IS_TRUE))
# endif
  // !IS_UNDEF and !IS_NULL
# if ZEND_NAN_TAG_32
#  define T_IS_SET(t)				((t) > Z_TYPE_TO_RAW(IS_NULL) || (t) < Z_TYPE_TO_RAW(IS_UNDEF))
# elif ZEND_NAN_TAG_64
#  define T_IS_SET(t)				((t) < Z_TYPE_TO_RAW(IS_NULL))
# endif
  // IS_FALSE or IS_TRUE
# define T_IS_BOOL(t)				(T_IS_FALSE(t) || T_IS_TRUE(t))
  // IS_LONG or IS_DOUBLE
# define T_IS_NUMBER(t)				(T_IS_LONG(t) || T_IS_DOUBLE(t))
	  // IS_UNDEF, IS_NULL, IS_FALSE, IS_TRUE, IS_LONG or IS_DOUBLE
# if ZEND_NAN_TAG_32
#  define T_IS_SCALAR(t)			((t) <= Z_TYPE_TO_RAW(IS_LONG))
# elif ZEND_NAN_TAG_64
#  define T_IS_SCALAR(t)			((t) >= Z_TYPE_TO_RAW(IS_LONG) || T_IS_DOUBLE(t))
# endif
  // IS_UNDEF, IS_NULL, IS_FALSE, IS_TRUE, IS_LONG, IS_DOUBLE or IS_STRING
# if ZEND_NAN_TAG_32
#  define T_IS_SCALAR_OR_STRING(t)	(((t) & (uint32_t)~(IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) <= Z_TYPE_TO_RAW(IS_STRING))
# elif ZEND_NAN_TAG_64
#  define T_IS_SCALAR_OR_STRING(t)	(((t) | IS_TYPE_REFCOUNTED) >= Z_TYPE_TO_RAW(IS_STRING) || T_IS_DOUBLE(t))
# endif
  // IS_UNDEF, IS_NULL, IS_FALSE, IS_TRUE, IS_LONG, S_DOUBLE, IS_STRING or IS_ARRAY
# if ZEND_NAN_TAG_32
#  define T_IS_PERSISTABLE(t)		(((t) & (uint32_t)~(IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) <= Z_TYPE_TO_RAW(IS_ARRAY))
# elif ZEND_NAN_TAG_64
#  define T_IS_PERSISTABLE(t)		(((t) | IS_TYPE_REFCOUNTED) >= Z_TYPE_TO_RAW(IS_ARRAY) || T_IS_DOUBLE(t))
# endif
#else
  // IS_UNDEF, IS_NULL, IS_FALSE or IS_TRUE
# define T_IS_PRIMITIVE(t)			(((t) & Z_TYPE_MASK) <= IS_TRUE)
  // IS_UNDEF, IS_NULL or IS_FALSE
# define T_IS_LESS_THAN_TRUE(t)	    (((t) & Z_TYPE_MASK) < IS_TRUE)
  // !IS_UNDEF and !IS_NULL
# define T_IS_SET(t)				(((t) & Z_TYPE_MASK) > IS_NULL)
  // IS_FALSE or IS_TRUE
# define T_IS_BOOL(t)				(T_IS_FALSE(t) || T_IS_TRUE(t))
  // IS_LONG or IS_DOUBLE
# define T_IS_NUMBER(t)				(T_IS_LONG(t) || T_IS_DOUBLE(t))
  // IS_UNDEF, IS_NULL, IS_FALSE, IS_TRUE, IS_LONG or IS_DOUBLE
# define T_IS_SCALAR(t)				(((t) & Z_TYPE_MASK) <= IS_DOUBLE)
  // IS_UNDEF, IS_NULL, IS_FALSE, IS_TRUE, IS_LONG, IS_DOUBLE or IS_STRING
# define T_IS_SCALAR_OR_STRING(t)	(((t) & Z_TYPE_MASK) <= IS_STRING)
  // IS_UNDEF, IS_NULL, IS_FALSE, IS_TRUE, IS_LONG, S_DOUBLE, IS_STRING or IS_ARRAY
# define T_IS_PERSISTABLE(t)		(((t) & Z_TYPE_MASK) <= IS_ARRAY)
#endif

#define T_IS_REFCOUNTED(t)			zend_type_is_refcounted(t)

#define Z_IS_UNDEF(zval)			T_IS_UNDEF(Z_RAW_TYPE_INFO(zval))
#define Z_IS_NULL(zval)				T_IS_NULL(Z_RAW_TYPE_INFO(zval))
#define Z_IS_FALSE(zval)			T_IS_FALSE(Z_RAW_TYPE_INFO(zval))
#define Z_IS_TRUE(zval)				T_IS_TRUE(Z_RAW_TYPE_INFO(zval))
#define Z_IS_LONG(zval)				T_IS_LONG(Z_RAW_TYPE_INFO(zval))
#define Z_IS_DOUBLE(zval)			T_IS_DOUBLE(Z_RAW_TYPE_INFO(zval))
#define Z_IS_STRING(zval)			T_IS_STRING(Z_RAW_TYPE_INFO(zval))
#define Z_IS_ARRAY(zval)			T_IS_ARRAY(Z_RAW_TYPE_INFO(zval))
#define Z_IS_OBJECT(zval)			T_IS_OBJECT(Z_RAW_TYPE_INFO(zval))
#define Z_IS_RESOURCE(zval)			T_IS_RESOURCE(Z_RAW_TYPE_INFO(zval))
#define Z_IS_REFERENCE(zval)		T_IS_REFERENCE(Z_RAW_TYPE_INFO(zval))
#define Z_IS_INDIRECT(zval)			T_IS_INDIRECT(Z_RAW_TYPE_INFO(zval))
#define Z_IS_PTR(zval)				T_IS_PTR(Z_RAW_TYPE_INFO(zval))
#define Z_IS_SET(zval)				T_IS_SET(Z_RAW_TYPE_INFO(zval))
#define Z_IS_PRIMITIVE(zval)		T_IS_PRIMITIVE(Z_RAW_TYPE_INFO(zval))
#define Z_IS_BOOL(zval)				T_IS_BOOL(Z_RAW_TYPE_INFO(zval))
#define Z_IS_NUMBER(zval)			T_IS_NUMBER(Z_RAW_TYPE_INFO(zval))
#define Z_IS_SCALAR(zval)			T_IS_SCALAR(Z_RAW_TYPE_INFO(zval))
#define Z_IS_SCALAR_OR_STRING(zval)	T_IS_SCALAR_OR_STRING(Z_RAW_TYPE_INFO(zval))
#define Z_IS_PERSISTABLE(zval)		T_IS_PERSISTABLE(Z_RAW_TYPE_INFO(zval))
#define Z_IS_LESS_THAN_TRUE(zval)	T_IS_LESS_THAN_TRUE(Z_RAW_TYPE_INFO(zval))

#define Z_IS_UNDEF_P(zval)			Z_IS_UNDEF(*(zval))
#define Z_IS_NULL_P(zval)			Z_IS_NULL(*(zval))
#define Z_IS_FALSE_P(zval)			Z_IS_FALSE(*(zval))
#define Z_IS_TRUE_P(zval)			Z_IS_TRUE(*(zval))
#define Z_IS_LONG_P(zval)			Z_IS_LONG(*(zval))
#define Z_IS_DOUBLE_P(zval)			Z_IS_DOUBLE(*(zval))
#define Z_IS_STRING_P(zval)			Z_IS_STRING(*(zval))
#define Z_IS_ARRAY_P(zval)			Z_IS_ARRAY(*(zval))
#define Z_IS_OBJECT_P(zval)			Z_IS_OBJECT(*(zval))
#define Z_IS_RESOURCE_P(zval)		Z_IS_RESOURCE(*(zval))
#define Z_IS_REFERENCE_P(zval)		Z_IS_REFERENCE(*(zval))
#define Z_IS_INDIRECT_P(zval)		Z_IS_INDIRECT(*(zval))
#define Z_IS_PTR_P(zval)			Z_IS_PTR(*(zval))
#define Z_IS_SET_P(zval)			Z_IS_SET(*(zval))
#define Z_IS_PRIMITIVE_P(zval)		Z_IS_PRIMITIVE(*(zval))
#define Z_IS_BOOL_P(zval)			Z_IS_BOOL(*(zval))
#define Z_IS_NUMBER_P(zval)			Z_IS_NUMBER(*(zval))
#define Z_IS_SCALAR_P(zval)			Z_IS_SCALAR(*(zval))
#define Z_IS_SCALAR_OR_STRING_P(zval)	Z_IS_SCALAR_OR_STRING(*(zval))
#define Z_IS_PERSISTABLE_P(zval)	Z_IS_PERSISTABLE(*(zval))
#define Z_IS_LESS_THAN_TRUE_P(zval)	Z_IS_LESS_THAN_TRUE(*(zval))

#define Z_CONSTANT(zval)			T_IS_CONSTANT(Z_RAW_TYPE_INFO(zval))
#define Z_CONSTANT_P(zval_p)		Z_CONSTANT(*(zval_p))

#define Z_REFCOUNTED(zval)			T_IS_REFCOUNTED(Z_RAW_TYPE_INFO(zval))
#define Z_REFCOUNTED_P(zval_p)		Z_REFCOUNTED(*(zval_p))

/* deprecated: (COPYABLE is the same as IS_ARRAY) */
#define Z_COPYABLE(zval)			Z_IS_ARRAY(zval)
#define Z_COPYABLE_P(zval_p)		Z_COPYABLE(*(zval_p))

/* deprecated: (IMMUTABLE is the same as IS_ARRAY && !REFCOUED) */
#define Z_IMMUTABLE(zval)			Z_IS_ARRAY(zval)
#define Z_IMMUTABLE_P(zval_p)		Z_IMMUTABLE(*(zval_p))
#define Z_OPT_IMMUTABLE(zval)		Z_IMMUTABLE(zval_p)
#define Z_OPT_IMMUTABLE_P(zval_p)	Z_IMMUTABLE(*(zval_p))

/* the following Z_OPT_* macros make better code when Z_TYPE_INFO accessed before */
#define Z_OPT_TYPE(zval)			Z_TYPE(zval)
#define Z_OPT_TYPE_P(zval_p)		Z_OPT_TYPE(*(zval_p))

#define Z_OPT_CONSTANT(zval)		Z_CONSTANT(zval)
#define Z_OPT_CONSTANT_P(zval_p)	Z_OPT_CONSTANT(*(zval_p))

#define Z_OPT_REFCOUNTED(zval)		Z_REFCOUNTED(zval)
#define Z_OPT_REFCOUNTED_P(zval_p)	Z_OPT_REFCOUNTED(*(zval_p))

/* deprecated: (COPYABLE is the same as IS_ARRAY) */
#define Z_OPT_COPYABLE(zval)		Z_IS_ARRAY(zval)
#define Z_OPT_COPYABLE_P(zval_p)	Z_OPT_COPYABLE(*(zval_p))

#define Z_OPT_ISREF(zval)			Z_IS_REFERENCE(zval)
#define Z_OPT_ISREF_P(zval_p)		Z_OPT_ISREF(*(zval_p))

#define Z_ISREF(zval)				Z_IS_REFERENCE(zval)
#define Z_ISREF_P(zval_p)			Z_ISREF(*(zval_p))

#define Z_ISUNDEF(zval)				Z_IS_UNDEF(zval)
#define Z_ISUNDEF_P(zval_p)			Z_ISUNDEF(*(zval_p))

#define Z_ISNULL(zval)				Z_IS_NULL(zval)
#define Z_ISNULL_P(zval_p)			Z_ISNULL(*(zval_p))

#define Z_ISERROR(zval)				T_IS_ERROR(Z_RAW_TYPE_INFO(zval))
#define Z_ISERROR_P(zval_p)			Z_ISERROR(*(zval_p))

// TODO: ???
#define Z_LVAL(zval)				(zval).value.lval
#define Z_LVAL_P(zval_p)			Z_LVAL(*(zval_p))

#if ZEND_NAN_TAG
# define Z_DVAL(zval)				(zval).dval
#else
# define Z_DVAL(zval)				(zval).value.dval
#endif
#define Z_DVAL_P(zval_p)			Z_DVAL(*(zval_p))

#define Z_STR(zval)					((zend_string*)Z_PTR(zval))
#define Z_STR_P(zval_p)				Z_STR(*(zval_p))

#define Z_STRVAL(zval)				ZSTR_VAL(Z_STR(zval))
#define Z_STRVAL_P(zval_p)			Z_STRVAL(*(zval_p))

#define Z_STRLEN(zval)				ZSTR_LEN(Z_STR(zval))
#define Z_STRLEN_P(zval_p)			Z_STRLEN(*(zval_p))

#define Z_STRHASH(zval)				ZSTR_HASH(Z_STR(zval))
#define Z_STRHASH_P(zval_p)			Z_STRHASH(*(zval_p))

#define Z_ARR(zval)					((zend_array*)Z_PTR(zval))
#define Z_ARR_P(zval_p)				Z_ARR(*(zval_p))

#define Z_ARRVAL(zval)				Z_ARR(zval)
#define Z_ARRVAL_P(zval_p)			Z_ARRVAL(*(zval_p))

#define Z_OBJ(zval)					((zend_object*)Z_PTR(zval))
#define Z_OBJ_P(zval_p)				Z_OBJ(*(zval_p))

#define Z_OBJ_HT(zval)				Z_OBJ(zval)->handlers
#define Z_OBJ_HT_P(zval_p)			Z_OBJ_HT(*(zval_p))

#define Z_OBJ_HANDLER(zval, hf)		Z_OBJ_HT((zval))->hf
#define Z_OBJ_HANDLER_P(zv_p, hf)	Z_OBJ_HANDLER(*(zv_p), hf)

#define Z_OBJ_HANDLE(zval)          (Z_OBJ((zval)))->handle
#define Z_OBJ_HANDLE_P(zval_p)      Z_OBJ_HANDLE(*(zval_p))

#define Z_OBJCE(zval)				(Z_OBJ(zval)->ce)
#define Z_OBJCE_P(zval_p)			Z_OBJCE(*(zval_p))

#define Z_OBJPROP(zval)				Z_OBJ_HT((zval))->get_properties(&(zval))
#define Z_OBJPROP_P(zval_p)			Z_OBJPROP(*(zval_p))

#define Z_OBJDEBUG(zval,tmp)		(Z_OBJ_HANDLER((zval),get_debug_info)?Z_OBJ_HANDLER((zval),get_debug_info)(&(zval),&tmp):(tmp=0,Z_OBJ_HANDLER((zval),get_properties)?Z_OBJPROP(zval):NULL))
#define Z_OBJDEBUG_P(zval_p,tmp)	Z_OBJDEBUG(*(zval_p), tmp)

#define Z_RES(zval)					((zend_resource*)Z_PTR(zval))
#define Z_RES_P(zval_p)				Z_RES(*zval_p)

#define Z_RES_HANDLE(zval)			Z_RES(zval)->handle
#define Z_RES_HANDLE_P(zval_p)		Z_RES_HANDLE(*zval_p)

#define Z_RES_TYPE(zval)			Z_RES(zval)->type
#define Z_RES_TYPE_P(zval_p)		Z_RES_TYPE(*zval_p)

#define Z_RES_VAL(zval)				Z_RES(zval)->ptr
#define Z_RES_VAL_P(zval_p)			Z_RES_VAL(*zval_p)

#define Z_REF(zval)					((zend_reference*)Z_PTR(zval))
#define Z_REF_P(zval_p)				Z_REF(*(zval_p))

#define Z_REFVAL(zval)				&Z_REF(zval)->val
#define Z_REFVAL_P(zval_p)			Z_REFVAL(*(zval_p))

#define Z_AST(zval)					((zend_ast_ref*)Z_PTR(zval))
#define Z_AST_P(zval_p)				Z_AST(*(zval_p))

#define GC_AST(p)					((zend_ast*)(((char*)p) + sizeof(zend_ast_ref)))

#define Z_ASTVAL(zval)				GC_AST(Z_AST(zval))
#define Z_ASTVAL_P(zval_p)			Z_ASTVAL(*(zval_p))

#define Z_INDIRECT(zv)				((zval*)Z_PTR(zv))
#define Z_INDIRECT_P(zval_p)		Z_INDIRECT(*(zval_p))

#define Z_CE(zval)					((zend_class_entry*)Z_PTR(zval))
#define Z_CE_P(zval_p)				Z_CE(*(zval_p))

#define Z_FUNC(zval)				((zend_function*)Z_PTR(zval))
#define Z_FUNC_P(zval_p)			Z_FUNC(*(zval_p))

#define ZVAL_UNDEF(z) do {				\
		Z_SET_TYPE_INFO_P(z, IS_UNDEF);	\
	} while (0)

#define ZVAL_NULL(z) do {				\
		Z_SET_TYPE_INFO_P(z, IS_NULL);	\
	} while (0)

#define ZVAL_FALSE(z) do {				\
		Z_SET_TYPE_INFO_P(z, IS_FALSE);	\
	} while (0)

#define ZVAL_TRUE(z) do {				\
		Z_SET_TYPE_INFO_P(z, IS_TRUE);	\
	} while (0)

#define ZVAL_BOOL(z, b) do {			\
		Z_SET_TYPE_INFO_P(z,			\
			(b) ? IS_TRUE : IS_FALSE);	\
	} while (0)

#define ZVAL_LONG(z, l) {				\
		zval *__z = (z);				\
		Z_LVAL_P(__z) = l;				\
		Z_SET_TYPE_INFO_P(__z, IS_LONG);\
	}

#define ZVAL_DOUBLE(z, d) {				\
		zval *__z = (z);				\
		Z_DVAL_P(__z) = d;				\
		Z_SET_TYPE_INFO_P(__z, IS_DOUBLE);	\
	}

#define ZVAL_STR(z, s) do {						\
		zval *__z = (z);						\
		zend_string *__s = (s);					\
		Z_SET_PTR_P(__z,                        \
			/* interned strings support */		\
			ZSTR_IS_INTERNED(__s) ?				\
				IS_INTERNED_STRING_EX :			\
				IS_STRING_EX, __s);				\
	} while (0)

#define ZVAL_INTERNED_STR(z, s) \
	Z_SET_PTR_P(z, IS_INTERNED_STRING_EX, s)

#define ZVAL_NEW_STR(z, s) \
	Z_SET_PTR_P(z, IS_STRING_EX, s)

#define ZVAL_STR_COPY(z, s) do {						\
		zval *__z = (z);								\
		zend_string *__s = (s);							\
		/* interned strings support */					\
		if (ZSTR_IS_INTERNED(__s)) {					\
			Z_SET_PTR_P(__z, IS_INTERNED_STRING_EX, __s);\
		} else {										\
			GC_ADDREF(__s);								\
			Z_SET_PTR_P(__z, IS_STRING_EX, __s);		\
		}												\
	} while (0)

#define ZVAL_ARR(z, a) \
	Z_SET_PTR_P(z, IS_ARRAY_EX, a)

#define ZVAL_NEW_ARR(z) do {									\
		zval *__z = (z);										\
		zend_array *_arr =										\
			(zend_array *) emalloc(sizeof(zend_array));			\
		Z_SET_PTR_P(__z, IS_ARRAY_EX, __arr);					\
	} while (0)

#define ZVAL_NEW_PERSISTENT_ARR(z) do {							\
		zval *__z = (z);										\
		zend_array *_arr =										\
			(zend_array *) malloc(sizeof(zend_array));			\
		Z_SET_PTR_P(__z, IS_ARRAY_EX, _arr);					\
	} while (0)

#define ZVAL_OBJ(z, o) \
	Z_SET_PTR_P(z, IS_OBJECT_EX, o)

#define ZVAL_RES(z, r) \
	Z_SET_PTR_P(z, IS_RESOURCE_EX, r)

#define ZVAL_NEW_RES(z, h, p, t) do {							\
		zend_resource *_res =									\
		(zend_resource *) emalloc(sizeof(zend_resource));		\
		GC_SET_REFCOUNT(_res, 1);								\
		GC_TYPE_INFO(_res) = IS_RESOURCE;						\
		_res->handle = (h);										\
		_res->type = (t);										\
		_res->ptr = (p);										\
		Z_SET_PTR_P(z, IS_RESOURCE_EX, _res);					\
	} while (0)

#define ZVAL_NEW_PERSISTENT_RES(z, h, p, t) do {				\
		zend_resource *_res =									\
		(zend_resource *) malloc(sizeof(zend_resource));		\
		GC_SET_REFCOUNT(_res, 1);								\
		GC_TYPE_INFO(_res) = IS_RESOURCE |						\
			(GC_PERSISTENT << GC_FLAGS_SHIFT);					\
		_res->handle = (h);										\
		_res->type = (t);										\
		_res->ptr = (p);										\
		Z_SET_PTR_P(z, IS_RESOURCE_EX, _res);					\
	} while (0)

#define ZVAL_REF(z, r) \
	Z_SET_PTR_P(z, IS_REFERENCE_EX, r)

#define ZVAL_NEW_EMPTY_REF(z) do {								\
		zend_reference *_ref =									\
		(zend_reference *) emalloc(sizeof(zend_reference));		\
		GC_SET_REFCOUNT(_ref, 1);								\
		GC_TYPE_INFO(_ref) = IS_REFERENCE;						\
		Z_SET_PTR_P(z, IS_REFERENCE_EX, _ref);					\
	} while (0)

#define ZVAL_NEW_REF(z, r) do {									\
		zend_reference *_ref =									\
		(zend_reference *) emalloc(sizeof(zend_reference));		\
		GC_SET_REFCOUNT(_ref, 1);								\
		GC_TYPE_INFO(_ref) = IS_REFERENCE;						\
		ZVAL_COPY_VALUE(&_ref->val, r);							\
		Z_SET_PTR_P(z, IS_REFERENCE_EX, _ref);					\
	} while (0)

#define ZVAL_MAKE_REF_EX(z, refcount) do {						\
		zval *_z = (z);											\
		zend_reference *_ref =									\
			(zend_reference *) emalloc(sizeof(zend_reference));	\
		GC_SET_REFCOUNT(_ref, (refcount));						\
		GC_TYPE_INFO(_ref) = IS_REFERENCE;						\
		ZVAL_COPY_VALUE(&_ref->val, _z);						\
		Z_SET_PTR_P(z, IS_REFERENCE_EX, _ref);					\
	} while (0)

#define ZVAL_NEW_PERSISTENT_REF(z, r) do {						\
		zend_reference *_ref =									\
		(zend_reference *) malloc(sizeof(zend_reference));		\
		GC_SET_REFCOUNT(_ref, 1);								\
		GC_TYPE_INFO(_ref) = IS_REFERENCE |						\
			(GC_PERSISTENT << GC_FLAGS_SHIFT);					\
		ZVAL_COPY_VALUE(&_ref->val, r);							\
		Z_SET_PTR_P(z, IS_REFERENCE_EX, _ref);					\
	} while (0)

#define ZVAL_AST(z, ast) \
	Z_SET_PTR_P(z, IS_CONSTANT_AST_EX, ast)

#define ZVAL_INDIRECT(z, v) \
	Z_SET_PTR_P(z, IS_INDIRECT, v)

#define ZVAL_PTR(z, p) \
	Z_SET_PTR_P(z, IS_PTR, p)

#define ZVAL_FUNC(z, f) \
	ZVAL_PTR(z, f)

#define ZVAL_CE(z, c) \
	ZVAL_PTR(z, c)

#define ZVAL_ERROR(z) \
	Z_SET_TYPE_INFO_P(z, _IS_ERROR)

#define Z_REFCOUNT_P(pz)			zval_refcount_p(pz)
#define Z_SET_REFCOUNT_P(pz, rc)	zval_set_refcount_p(pz, rc)
#define Z_ADDREF_P(pz)				zval_addref_p(pz)
#define Z_DELREF_P(pz)				zval_delref_p(pz)

#define Z_REFCOUNT(z)				Z_REFCOUNT_P(&(z))
#define Z_SET_REFCOUNT(z, rc)		Z_SET_REFCOUNT_P(&(z), rc)
#define Z_ADDREF(z)					Z_ADDREF_P(&(z))
#define Z_DELREF(z)					Z_DELREF_P(&(z))

#define Z_TRY_ADDREF_P(pz) do {		\
	if (Z_REFCOUNTED_P((pz))) {		\
		Z_ADDREF_P((pz));			\
	}								\
} while (0)

#define Z_TRY_DELREF_P(pz) do {		\
	if (Z_REFCOUNTED_P((pz))) {		\
		Z_DELREF_P((pz));			\
	}								\
} while (0)

#define Z_TRY_ADDREF(z)				Z_TRY_ADDREF_P(&(z))
#define Z_TRY_DELREF(z)				Z_TRY_DELREF_P(&(z))

#ifndef ZEND_RC_DEBUG
# define ZEND_RC_DEBUG 0
#endif

#if ZEND_RC_DEBUG
extern ZEND_API zend_bool zend_rc_debug;
# define ZEND_RC_MOD_CHECK(p) do { \
		if (zend_rc_debug) { \
			ZEND_ASSERT(!((p)->u.v.flags & GC_IMMUTABLE)); \
			ZEND_ASSERT(((p)->u.v.flags & (GC_PERSISTENT|GC_PERSISTENT_LOCAL)) != GC_PERSISTENT); \
		} \
	} while (0)
# define GC_MAKE_PERSISTENT_LOCAL(p) do { \
		GC_FLAGS(p) |= GC_PERSISTENT_LOCAL; \
	} while (0)
#else
# define ZEND_RC_MOD_CHECK(p) \
	do { } while (0)
# define GC_MAKE_PERSISTENT_LOCAL(p) \
	do { } while (0)
#endif

static zend_always_inline uint32_t zend_gc_refcount(const zend_refcounted_h *p) {
	return p->refcount;
}

static zend_always_inline uint32_t zend_gc_set_refcount(zend_refcounted_h *p, uint32_t rc) {
	p->refcount = rc;
	return p->refcount;
}

static zend_always_inline uint32_t zend_gc_addref(zend_refcounted_h *p) {
	ZEND_RC_MOD_CHECK(p);
	return ++(p->refcount);
}

static zend_always_inline uint32_t zend_gc_delref(zend_refcounted_h *p) {
	ZEND_RC_MOD_CHECK(p);
	return --(p->refcount);
}

static zend_always_inline uint32_t zend_gc_addref_ex(zend_refcounted_h *p, uint32_t rc) {
	ZEND_RC_MOD_CHECK(p);
	p->refcount += rc;
	return p->refcount;
}

static zend_always_inline uint32_t zend_gc_delref_ex(zend_refcounted_h *p, uint32_t rc) {
	ZEND_RC_MOD_CHECK(p);
	p->refcount -= rc;
	return p->refcount;
}

static zend_always_inline uint32_t zval_refcount_p(const zval* pz) {
#if ZEND_DEBUG
	ZEND_ASSERT(Z_REFCOUNTED_P(pz) || Z_IS_ARRAY_P(pz));
#endif
	return GC_REFCOUNT(Z_COUNTED_P(pz));
}

static zend_always_inline uint32_t zval_set_refcount_p(zval* pz, uint32_t rc) {
	ZEND_ASSERT(Z_REFCOUNTED_P(pz));
	return GC_SET_REFCOUNT(Z_COUNTED_P(pz), rc);
}

static zend_always_inline uint32_t zval_addref_p(zval* pz) {
	ZEND_ASSERT(Z_REFCOUNTED_P(pz));
	return GC_ADDREF(Z_COUNTED_P(pz));
}

static zend_always_inline uint32_t zval_delref_p(zval* pz) {
	ZEND_ASSERT(Z_REFCOUNTED_P(pz));
	return GC_DELREF(Z_COUNTED_P(pz));
}

#if ZEND_NAN_TAG_64
	/* pass */	
#elif SIZEOF_SIZE_T == 4 && ZEND_NAN_TAG_32
# define ZVAL_COPY_VALUE_EX(z, v, gc, t)				\
	do {												\
		z->value.ptr = gc;								\
		z->u1.type_info = t;							\
	} while (0)
#elif SIZEOF_SIZE_T == 4
# define ZVAL_COPY_VALUE_EX(z, v, gc, t)				\
	do {												\
		uint32_t _w2 = v->value.ww.w2;					\
		z->value.ptr = gc;								\
		z->value.ww.w2 = _w2;							\
		z->u1.type_info = t;							\
	} while (0)
#elif SIZEOF_SIZE_T == 8
# define ZVAL_COPY_VALUE_EX(z, v, gc, t)				\
	do {												\
		z->value.ptr = gc;								\
		z->u1.type_info = t;							\
	} while (0)
#else
# error "Unknown SIZEOF_SIZE_T"
#endif

#if ZEND_NAN_TAG_64
# define ZVAL_COPY_VALUE(z, v)							\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		_z1->i64 = _z2->i64;							\
	} while (0)	
#else
# define ZVAL_COPY_VALUE(z, v)							\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		zend_refcounted *_gc = Z_COUNTED_P(_z2);		\
		uint32_t _t = _z2->u1.type_info;				\
		ZVAL_COPY_VALUE_EX(_z1, _z2, _gc, _t);			\
	} while (0)
#endif

#if ZEND_NAN_TAG_64
# define ZVAL_COPY(z, v)								\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		_z1->i64 = _z2->i64;							\
		if (Z_REFCOUNTED_P(_z1)) {						\
			Z_ADDREF_P(_z1);							\
		}												\
	} while (0)	
#else
# define ZVAL_COPY(z, v)								\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		zend_refcounted *_gc = Z_COUNTED_P(_z2);		\
		uint32_t _t = _z2->u1.type_info;				\
		ZVAL_COPY_VALUE_EX(_z1, _z2, _gc, _t);			\
		if (T_IS_REFCOUNTED(_t)) {						\
			GC_ADDREF(_gc);								\
		}												\
	} while (0)
#endif

#if ZEND_NAN_TAG_64
# define ZVAL_DUP(z, v)									\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		if (Z_IS_ARRAY_P(_z2)) {						\
			ZVAL_ARR(_z1, zend_array_dup(Z_ARR_P(_z2)));\
		} else {										\
			_z1->i64 = _z2->i64;						\
			if (Z_REFCOUNTED_P(_z1)) {					\
				Z_ADDREF_P(_z1);						\
			}											\
		}												\
	} while (0)	
#else
# define ZVAL_DUP(z, v)									\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		zend_refcounted *_gc = Z_COUNTED_P(_z2);		\
		uint32_t _t = _z2->u1.type_info;				\
		if (T_IS_ARRAY(_t)) {							\
			ZVAL_ARR(_z1, zend_array_dup((zend_array*)_gc));\
		} else {										\
			if (T_IS_REFCOUNTED(_t)) {					\
				GC_ADDREF(_gc);							\
			}											\
			ZVAL_COPY_VALUE_EX(_z1, _z2, _gc, _t);		\
		}												\
	} while (0)
#endif

/* ZVAL_COPY_OR_DUP() should be used instead of ZVAL_COPY() and ZVAL_DUP()
 * in all places where the source may be a persistent zval.
 */
#if ZEND_NAN_TAG_64
# define ZVAL_COPY_OR_DUP(z, v)							\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		_z1->i64 = _z2->i64;							\
		if (Z_REFCOUNTED_P(_z1)) {						\
			zend_refcounted *_gc = Z_COUNTED_P(_z1);	\
			if (EXPECTED(!(GC_FLAGS(_gc) & GC_PERSISTENT))) {\
				GC_ADDREF(_gc);							\
			} else {									\
				zval_copy_ctor_func(_z1);				\
			}											\
		}												\
	} while (0)	
#else
# define ZVAL_COPY_OR_DUP(z, v)											\
	do {																\
		zval *_z1 = (z);												\
		const zval *_z2 = (v);											\
		zend_refcounted *_gc = Z_COUNTED_P(_z2);						\
		uint32_t _t = _z2->u1.type_info;								\
		ZVAL_COPY_VALUE_EX(_z1, _z2, _gc, _t);							\
		if (T_IS_REFCOUNTED(_t)) {										\
			if (EXPECTED(!(GC_FLAGS(_gc) & GC_PERSISTENT))) {			\
				GC_ADDREF(_gc);											\
			} else {													\
				zval_copy_ctor_func(_z1);								\
			}															\
		}																\
	} while (0)
#endif

#define ZVAL_DEREF(z) do {								\
		if (UNEXPECTED(Z_ISREF_P(z))) {					\
			(z) = Z_REFVAL_P(z);						\
		}												\
	} while (0)

#define ZVAL_OPT_DEREF(z) do {							\
		if (UNEXPECTED(Z_OPT_ISREF_P(z))) {				\
			(z) = Z_REFVAL_P(z);						\
		}												\
	} while (0)

#define ZVAL_MAKE_REF(zv) do {							\
		zval *__zv = (zv);								\
		if (!Z_ISREF_P(__zv)) {							\
			ZVAL_NEW_REF(__zv, __zv);					\
		}												\
	} while (0)

#define ZVAL_UNREF(z) do {								\
		zval *_z = (z);									\
		zend_reference *ref;							\
		ZEND_ASSERT(Z_ISREF_P(_z));						\
		ref = Z_REF_P(_z);								\
		ZVAL_COPY_VALUE(_z, &ref->val);					\
		efree_size(ref, sizeof(zend_reference));		\
	} while (0)

#define ZVAL_COPY_UNREF(z, v) do {						\
		zval *_z3 = (v);								\
		if (Z_OPT_REFCOUNTED_P(_z3)) {					\
			if (UNEXPECTED(Z_OPT_ISREF_P(_z3))			\
			 && UNEXPECTED(Z_REFCOUNT_P(_z3) == 1)) {	\
				ZVAL_UNREF(_z3);						\
				if (Z_OPT_REFCOUNTED_P(_z3)) {			\
					Z_ADDREF_P(_z3);					\
				}										\
			} else {									\
				Z_ADDREF_P(_z3);						\
			}											\
		}												\
		ZVAL_COPY_VALUE(z, _z3);						\
	} while (0)


#define SEPARATE_STRING(zv) do {						\
		zval *_zv = (zv);								\
		if (Z_REFCOUNT_P(_zv) > 1) {					\
			zend_string *_str = Z_STR_P(_zv);			\
			ZEND_ASSERT(Z_REFCOUNTED_P(_zv));			\
			ZEND_ASSERT(!ZSTR_IS_INTERNED(_str));		\
			Z_DELREF_P(_zv);							\
			ZVAL_NEW_STR(_zv, zend_string_init(			\
				ZSTR_VAL(_str),	ZSTR_LEN(_str), 0));	\
		}												\
	} while (0)

#define SEPARATE_ARRAY(zv) do {							\
		zval *_zv = (zv);								\
		zend_array *_arr = Z_ARR_P(_zv);				\
		if (UNEXPECTED(GC_REFCOUNT(_arr) > 1)) {		\
			if (Z_REFCOUNTED_P(_zv)) {					\
				GC_DELREF(_arr);						\
			}											\
			ZVAL_ARR(_zv, zend_array_dup(_arr));		\
		}												\
	} while (0)

#define SEPARATE_ZVAL_IF_NOT_REF(zv) do {				\
		zval *__zv = (zv);								\
		if (Z_IS_ARRAY_P(__zv)) {				\
			if (Z_REFCOUNT_P(__zv) > 1) {				\
				if (Z_REFCOUNTED_P(__zv)) {				\
					Z_DELREF_P(__zv);					\
				}										\
				ZVAL_ARR(__zv, zend_array_dup(Z_ARR_P(__zv)));\
			}											\
		}												\
	} while (0)

#define SEPARATE_ZVAL_NOREF(zv) do {					\
		zval *_zv = (zv);								\
		ZEND_ASSERT(!Z_IS_REFERENCE_P(_zv));			\
		SEPARATE_ZVAL_IF_NOT_REF(_zv);					\
	} while (0)

#define SEPARATE_ZVAL(zv) do {							\
		zval *_zv = (zv);								\
		if (Z_ISREF_P(_zv)) {							\
			zend_reference *_r = Z_REF_P(_zv);			\
			ZVAL_COPY_VALUE(_zv, &_r->val);				\
			if (GC_DELREF(_r) == 0) {					\
				efree_size(_r, sizeof(zend_reference));	\
			} else if (Z_IS_ARRAY_P(_zv)) {				\
				ZVAL_ARR(_zv, zend_array_dup(Z_ARR_P(_zv)));\
				break;									\
			} else if (Z_OPT_REFCOUNTED_P(_zv)) {		\
				Z_ADDREF_P(_zv);						\
				break;									\
			}											\
		}												\
		SEPARATE_ZVAL_IF_NOT_REF(_zv);					\
	} while (0)

#define SEPARATE_ARG_IF_REF(varptr) do { 				\
		ZVAL_DEREF(varptr);								\
		if (Z_REFCOUNTED_P(varptr)) { 					\
			Z_ADDREF_P(varptr); 						\
		}												\
	} while (0)

#endif /* ZEND_TYPES_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
