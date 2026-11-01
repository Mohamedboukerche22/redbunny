#include "redb.h"

#define MAX_C_SCOPES 128
#define MAX_C_SYMBOLS 512
#define MAX_FUNCS 256
#define MAX_LOOPS 64

typedef struct {
    char name[128];
    RedbType type;
    LLVMValueRef alloca;
} CSym;

typedef struct {
    CSym syms[MAX_C_SYMBOLS];
    int count;
} CScope;

typedef struct {
    char name[128];
    ASTNode *node;
    LLVMValueRef fn;
} FuncInfo;

typedef struct {
    LLVMBasicBlockRef break_bb;
    LLVMBasicBlockRef continue_bb;
} LoopInfo;

static LLVMContextRef ctx;
static LLVMModuleRef mod;
static LLVMBuilderRef builder;
static LLVMBasicBlockRef entry_bb;

static CScope scopes[MAX_C_SCOPES];
static int scope_top;

static FuncInfo funcs[MAX_FUNCS];
static int func_count;
static ASTNode *main_node;

static LoopInfo loops[MAX_LOOPS];
static int loop_top;

static RedbType current_ret;
static int current_is_main;

static void gen_stmt(ASTNode *node);
static LLVMValueRef gen_expr(ASTNode *node, RedbType *out_type);

static void cg_fail(int line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "redb: error:%d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void push_scope(void)
{
    if (scope_top >= MAX_C_SCOPES)
        cg_fail(0, "scope nesting too deep");
    memset(&scopes[scope_top], 0, sizeof(scopes[scope_top]));
    scope_top++;
}

static void pop_scope(void)
{
    if (scope_top > 0)
        scope_top--;
}

static CSym *c_lookup(const char *name)
{
    int i, j;

    for (i = scope_top - 1; i >= 0; i--) {
        for (j = scopes[i].count - 1; j >= 0; j--) {
            if (!strcmp(scopes[i].syms[j].name, name))
                return &scopes[i].syms[j];
        }
    }
    return NULL;
}

static CSym *c_declare(const char *name, RedbType type, LLVMValueRef alloca, int line)
{
    CScope *scope = &scopes[scope_top - 1];
    CSym *sym;

    if (scope->count >= MAX_C_SYMBOLS)
        cg_fail(line, "too many symbols in scope");
    sym = &scope->syms[scope->count++];
    memset(sym, 0, sizeof(*sym));
    snprintf(sym->name, sizeof(sym->name), "%s", name);
    sym->type = type;
    sym->alloca = alloca;
    return sym;
}

static LLVMTypeRef llvm_type(RedbType type)
{
    switch (type) {
    case TY_VOID:   return LLVMVoidTypeInContext(ctx);
    case TY_INT:    return LLVMInt64TypeInContext(ctx);
    case TY_FLOAT:  return LLVMFloatTypeInContext(ctx);
    case TY_DOUBLE: return LLVMDoubleTypeInContext(ctx);
    case TY_STR:    return LLVMPointerType(LLVMInt8TypeInContext(ctx), 0);
    case TY_BOOL:   return LLVMInt1TypeInContext(ctx);
    default:        return LLVMPointerType(LLVMInt8TypeInContext(ctx), 0);
    }
}

/*
 * Allocas are hoisted into the entry block so that a declaration inside a
 * loop does not grow the stack on every iteration.
 */
static LLVMValueRef entry_alloca(LLVMTypeRef type, const char *name)
{
    LLVMBasicBlockRef saved = LLVMGetInsertBlock(builder);
    LLVMValueRef first = LLVMGetFirstInstruction(entry_bb);
    LLVMValueRef result;

    if (first)
        LLVMPositionBuilderBefore(builder, first);
    else
        LLVMPositionBuilderAtEnd(builder, entry_bb);

    result = LLVMBuildAlloca(builder, type, name);

    if (saved)
        LLVMPositionBuilderAtEnd(builder, saved);
    return result;
}

static int block_terminated(void)
{
    LLVMBasicBlockRef bb = LLVMGetInsertBlock(builder);

    return bb && LLVMGetBasicBlockTerminator(bb) != NULL;
}

static LLVMValueRef get_runtime(const char *name, LLVMTypeRef ret,
                                LLVMTypeRef *params, unsigned count)
{
    LLVMValueRef fn = LLVMGetNamedFunction(mod, name);
    LLVMTypeRef fn_type;

    if (fn)
        return fn;
    fn_type = LLVMFunctionType(ret, params, count, 0);
    return LLVMAddFunction(mod, name, fn_type);
}

static LLVMValueRef rt_write_str(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_STR) };
    return get_runtime("redb_write", llvm_type(TY_VOID), params, 1);
}

static LLVMValueRef rt_writeln_str(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_STR) };
    return get_runtime("redb_writeln", llvm_type(TY_VOID), params, 1);
}

static LLVMValueRef rt_write_int(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_INT) };
    return get_runtime("redb_write_int", llvm_type(TY_VOID), params, 1);
}

static LLVMValueRef rt_write_double(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_DOUBLE) };
    return get_runtime("redb_write_double", llvm_type(TY_VOID), params, 1);
}

static LLVMValueRef rt_write_bool(void)
{
    LLVMTypeRef params[] = { LLVMInt32TypeInContext(ctx) };
    return get_runtime("redb_write_bool", llvm_type(TY_VOID), params, 1);
}

static LLVMValueRef rt_newline(void)
{
    return get_runtime("redb_newline", llvm_type(TY_VOID), NULL, 0);
}

static LLVMValueRef rt_str_concat(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_STR), llvm_type(TY_STR) };
    return get_runtime("redb_str_concat", llvm_type(TY_STR), params, 2);
}

static LLVMValueRef rt_str_eq(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_STR), llvm_type(TY_STR) };
    return get_runtime("redb_str_eq", LLVMInt32TypeInContext(ctx), params, 2);
}

/* ---- array support ---- */
static LLVMValueRef rt_array_new(void)
{
    LLVMTypeRef params[] = { LLVMInt64TypeInContext(ctx), LLVMInt64TypeInContext(ctx),
                             LLVMInt64TypeInContext(ctx) };
    return get_runtime("redb_array_new", llvm_type(TY_STR), params, 3);
}

static LLVMValueRef rt_array_at(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_STR), LLVMInt64TypeInContext(ctx) };
    return get_runtime("redb_array_at", llvm_type(TY_STR), params, 2);
}

static LLVMValueRef rt_array_size(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_STR) };
    return get_runtime("redb_array_size", LLVMInt64TypeInContext(ctx), params, 1);
}

static LLVMValueRef rt_array_resize(void)
{
    LLVMTypeRef params[] = { llvm_type(TY_STR), LLVMInt64TypeInContext(ctx) };
    return get_runtime("redb_array_resize", llvm_type(TY_VOID), params, 2);
}

/* Byte size of one scalar element; must match the runtime's expectations. */
static int leaf_bytes(RedbType type)
{
    switch (type) {
    case TY_BOOL:   return 1;
    case TY_FLOAT:  return 4;
    default:        return 8;   /* int, double, str (pointer) */
    }
}

/* For array<array<int>>: depth = 1 (inner arrays), leaf = int. */
static int array_depth(RedbType arr, RedbType *leaf)
{
    RedbType t = redb_elem_type(arr);
    int depth = 0;

    while (redb_is_array(t)) {
        depth++;
        t = redb_elem_type(t);
    }
    *leaf = t;
    return depth;
}

static LLVMValueRef call_fn(LLVMValueRef fn, LLVMValueRef *args, unsigned count)
{
    return LLVMBuildCall2(builder, LLVMGlobalGetValueType(fn), fn, args, count, "");
}

static LLVMValueRef string_const(const char *text)
{
    return LLVMBuildGlobalStringPtr(builder, text, "str");
}

static LLVMValueRef to_bool(LLVMValueRef value, RedbType type)
{
    if (type == TY_BOOL)
        return value;
    if (type == TY_INT)
        return LLVMBuildICmp(builder, LLVMIntNE, value,
                             LLVMConstInt(LLVMInt64TypeInContext(ctx), 0, 0), "tobool");
    if (type == TY_FLOAT)
        return LLVMBuildFCmp(builder, LLVMRealONE, value,
                             LLVMConstReal(LLVMFloatTypeInContext(ctx), 0.0), "tobool");
    if (type == TY_DOUBLE)
        return LLVMBuildFCmp(builder, LLVMRealONE, value,
                             LLVMConstReal(LLVMDoubleTypeInContext(ctx), 0.0), "tobool");
    cg_fail(0, "cannot convert %s to bool", redb_type_name(type));
    return NULL;
}

static LLVMValueRef convert(LLVMValueRef value, RedbType from, RedbType to)
{
    if (from == to)
        return value;

    if (from == TY_BOOL && to == TY_INT)
        return LLVMBuildZExt(builder, value, llvm_type(TY_INT), "bool2int");
    if (from == TY_INT && to == TY_BOOL)
        return to_bool(value, TY_INT);

    if (from == TY_INT && (to == TY_FLOAT || to == TY_DOUBLE))
        return LLVMBuildSIToFP(builder, value, llvm_type(to), "int2fp");
    if ((from == TY_FLOAT || from == TY_DOUBLE) && to == TY_INT)
        return LLVMBuildFPToSI(builder, value, llvm_type(TY_INT), "fp2int");
    if (from == TY_FLOAT && to == TY_DOUBLE)
        return LLVMBuildFPExt(builder, value, llvm_type(TY_DOUBLE), "f2d");
    if (from == TY_DOUBLE && to == TY_FLOAT)
        return LLVMBuildFPTrunc(builder, value, llvm_type(TY_FLOAT), "d2f");

    return value;
}

static void emit_newline(void)
{
    call_fn(rt_newline(), NULL, 0);
}

static void print_value(LLVMValueRef value, RedbType type, int newline)
{
    LLVMValueRef args[1];

    switch (type) {
    case TY_STR:
        args[0] = value;
        call_fn(newline ? rt_writeln_str() : rt_write_str(), args, 1);
        return;

    case TY_INT:
        args[0] = value;
        call_fn(rt_write_int(), args, 1);
        break;

    case TY_FLOAT:
        args[0] = LLVMBuildFPExt(builder, value, llvm_type(TY_DOUBLE), "printd");
        call_fn(rt_write_double(), args, 1);
        break;

    case TY_DOUBLE:
        args[0] = value;
        call_fn(rt_write_double(), args, 1);
        break;

    case TY_BOOL:
        args[0] = LLVMBuildZExt(builder, value, LLVMInt32TypeInContext(ctx), "printb");
        call_fn(rt_write_bool(), args, 1);
        break;

    default:
        cg_fail(0, "cannot print value of type %s", redb_type_name(type));
        return;
    }

    if (newline)
        emit_newline();
}

static FuncInfo *find_func(const char *name)
{
    int i;

    for (i = 0; i < func_count; i++) {
        if (!strcmp(funcs[i].name, name))
            return &funcs[i];
    }
    return NULL;
}

static LLVMValueRef gen_binary(ASTNode *node, RedbType *out_type)
{
    const char *op = node->string_val;
    RedbType lt, rt, result;
    LLVMValueRef left = gen_expr(node->children[0], &lt);
    LLVMValueRef right = gen_expr(node->children[1], &rt);
    LLVMValueRef args[2];

    if (!strcmp(op, "+") && lt == TY_STR && rt == TY_STR) {
        args[0] = left;
        args[1] = right;
        *out_type = TY_STR;
        return call_fn(rt_str_concat(), args, 2);
    }

    if ((!strcmp(op, "==") || !strcmp(op, "!=")) && lt == TY_STR && rt == TY_STR) {
        LLVMValueRef eq;

        args[0] = left;
        args[1] = right;
        eq = call_fn(rt_str_eq(), args, 2);
        eq = LLVMBuildICmp(builder, LLVMIntNE, eq,
                           LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0), "streq");
        if (!strcmp(op, "!="))
            eq = LLVMBuildXor(builder, eq,
                              LLVMConstInt(LLVMInt1TypeInContext(ctx), 1, 0), "strne");
        *out_type = TY_BOOL;
        return eq;
    }

    if (!strcmp(op, "&&") || !strcmp(op, "||")) {
        LLVMValueRef l = to_bool(left, lt);
        LLVMValueRef r = to_bool(right, rt);

        *out_type = TY_BOOL;
        return !strcmp(op, "&&") ? LLVMBuildAnd(builder, l, r, "and")
                                 : LLVMBuildOr(builder, l, r, "or");
    }

    result = (lt == TY_DOUBLE || rt == TY_DOUBLE) ? TY_DOUBLE
           : (lt == TY_FLOAT || rt == TY_FLOAT) ? TY_FLOAT
           : TY_INT;
    left = convert(left, lt, result);
    right = convert(right, rt, result);

    if (!strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=") ||
        !strcmp(op, "==") || !strcmp(op, "!=")) {
        LLVMValueRef cmp;

        if (result == TY_INT) {
            LLVMIntPredicate pred = LLVMIntEQ;

            if (!strcmp(op, "<"))  pred = LLVMIntSLT;
            if (!strcmp(op, "<=")) pred = LLVMIntSLE;
            if (!strcmp(op, ">"))  pred = LLVMIntSGT;
            if (!strcmp(op, ">=")) pred = LLVMIntSGE;
            if (!strcmp(op, "!=")) pred = LLVMIntNE;
            cmp = LLVMBuildICmp(builder, pred, left, right, "cmp");
        } else {
            LLVMRealPredicate pred = LLVMRealOEQ;

            if (!strcmp(op, "<"))  pred = LLVMRealOLT;
            if (!strcmp(op, "<=")) pred = LLVMRealOLE;
            if (!strcmp(op, ">"))  pred = LLVMRealOGT;
            if (!strcmp(op, ">=")) pred = LLVMRealOGE;
            if (!strcmp(op, "!=")) pred = LLVMRealONE;
            cmp = LLVMBuildFCmp(builder, pred, left, right, "cmp");
        }
        *out_type = TY_BOOL;
        return cmp;
    }

    *out_type = result;
    if (result == TY_INT) {
        if (!strcmp(op, "+")) return LLVMBuildAdd(builder, left, right, "add");
        if (!strcmp(op, "-")) return LLVMBuildSub(builder, left, right, "sub");
        if (!strcmp(op, "*")) return LLVMBuildMul(builder, left, right, "mul");
        if (!strcmp(op, "/")) return LLVMBuildSDiv(builder, left, right, "div");
        if (!strcmp(op, "%")) return LLVMBuildSRem(builder, left, right, "rem");
    } else {
        if (!strcmp(op, "+")) return LLVMBuildFAdd(builder, left, right, "fadd");
        if (!strcmp(op, "-")) return LLVMBuildFSub(builder, left, right, "fsub");
        if (!strcmp(op, "*")) return LLVMBuildFMul(builder, left, right, "fmul");
        if (!strcmp(op, "/")) return LLVMBuildFDiv(builder, left, right, "fdiv");
        if (!strcmp(op, "%")) return LLVMBuildFRem(builder, left, right, "frem");
    }

    cg_fail(node->line, "unsupported operator '%s'", op);
    return NULL;
}

static LLVMValueRef gen_array_new(RedbType arr_type, LLVMValueRef size)
{
    RedbType leaf;
    int depth = array_depth(arr_type, &leaf);
    LLVMValueRef args[3];

    args[0] = size;
    args[1] = LLVMConstInt(LLVMInt64TypeInContext(ctx),
                           (unsigned long long)depth, 0);
    args[2] = LLVMConstInt(LLVMInt64TypeInContext(ctx),
                           (unsigned long long)leaf_bytes(leaf), 0);
    return call_fn(rt_array_new(), args, 3);
}

static void gen_array_fill(LLVMValueRef arr, ASTNode *literal, RedbType arr_type)
{
    RedbType elem = redb_elem_type(arr_type);
    size_t i;

    for (i = 0; i < literal->child_count; i++) {
        RedbType value_type;
        LLVMValueRef value = gen_expr(literal->children[i], &value_type);
        LLVMValueRef args[2];
        LLVMValueRef slot;

        value = convert(value, value_type, elem);
        args[0] = arr;
        args[1] = LLVMConstInt(LLVMInt64TypeInContext(ctx),
                               (unsigned long long)i, 0);
        slot = call_fn(rt_array_at(), args, 2);
        LLVMBuildStore(builder, value, slot);
    }
}

static LLVMValueRef gen_array_literal(ASTNode *node, RedbType *out_type)
{
    RedbType arr_type = node->inferred;
    LLVMValueRef size = LLVMConstInt(LLVMInt64TypeInContext(ctx),
                                     (unsigned long long)node->child_count, 0);
    LLVMValueRef arr = gen_array_new(arr_type, size);

    gen_array_fill(arr, node, arr_type);
    *out_type = arr_type;
    return arr;
}

static LLVMValueRef gen_expr(ASTNode *node, RedbType *out_type)
{
    if (!node) {
        *out_type = TY_VOID;
        return NULL;
    }

    switch (node->type) {
    case AST_INT_LIT:
        *out_type = TY_INT;
        return LLVMConstInt(LLVMInt64TypeInContext(ctx), (unsigned long long)node->int_val, 1);

    case AST_FLOAT_LIT:
        *out_type = TY_DOUBLE;
        return LLVMConstReal(LLVMDoubleTypeInContext(ctx), node->float_val);

    case AST_STRING_LIT:
        *out_type = TY_STR;
        return string_const(node->string_val);

    case AST_BOOL_LIT:
        *out_type = TY_BOOL;
        return LLVMConstInt(LLVMInt1TypeInContext(ctx), (unsigned)node->bool_val, 0);

    case AST_IDENT: {
        CSym *sym = c_lookup(node->name);

        if (!sym)
            cg_fail(node->line, "unknown identifier '%s'", node->name);
        *out_type = sym->type;
        return LLVMBuildLoad2(builder, llvm_type(sym->type), sym->alloca, node->name);
    }

    case AST_UNARY: {
        RedbType operand_type;
        LLVMValueRef operand = gen_expr(node->children[0], &operand_type);

        if (!strcmp(node->string_val, "-")) {
            *out_type = operand_type;
            if (operand_type == TY_INT)
                return LLVMBuildNeg(builder, operand, "neg");
            if (operand_type == TY_FLOAT || operand_type == TY_DOUBLE)
                return LLVMBuildFNeg(builder, operand, "fneg");
            cg_fail(node->line, "cannot negate %s", redb_type_name(operand_type));
        }

        *out_type = TY_BOOL;
        return LLVMBuildXor(builder, to_bool(operand, operand_type),
                            LLVMConstInt(LLVMInt1TypeInContext(ctx), 1, 0), "not");
    }

    case AST_BINARY:
        return gen_binary(node, out_type);

    case AST_CALL: {
        FuncInfo *fi = find_func(node->name);
        LLVMValueRef args[REDB_MAX_PARAMS];
        size_t i;
        RedbType arg_type;

        if (!fi)
            cg_fail(node->line, "unknown function '%s'", node->name);
        if (node->child_count > REDB_MAX_PARAMS)
            cg_fail(node->line, "too many arguments");

        for (i = 0; i < node->child_count; i++) {
            args[i] = gen_expr(node->children[i], &arg_type);
            args[i] = convert(args[i], arg_type, fi->node->param_types[i]);
        }

        *out_type = fi->node->ret_type;
        return call_fn(fi->fn, args, (unsigned)node->child_count);
    }

    case AST_CALL_WRITE:
    case AST_CALL_WRITELN: {
        RedbType value_type;
        LLVMValueRef value = gen_expr(node->children[0], &value_type);

        print_value(value, value_type, node->type == AST_CALL_WRITELN);
        *out_type = TY_VOID;
        return NULL;
    }

    case AST_CALL_FORMAT: {
        size_t i;

        for (i = 0; i < node->child_count; i++) {
            ASTNode *part = node->children[i];

            if (part->type == AST_STRING_LIT) {
                LLVMValueRef args[1];

                args[0] = string_const(part->string_val);
                call_fn(rt_write_str(), args, 1);
            } else {
                RedbType value_type;
                LLVMValueRef value = gen_expr(part, &value_type);

                print_value(value, value_type, 0);
            }
        }
        *out_type = TY_VOID;
        return NULL;
    }

    case AST_ARRAY_LITERAL:
        return gen_array_literal(node, out_type);

    case AST_INDEX: {
        RedbType base_type, index_type;
        LLVMValueRef base = gen_expr(node->children[0], &base_type);
        LLVMValueRef index = gen_expr(node->children[1], &index_type);
        LLVMValueRef args[2];
        LLVMValueRef slot;
        RedbType elem = redb_elem_type(base_type);

        args[0] = base;
        args[1] = convert(index, index_type, TY_INT);
        slot = call_fn(rt_array_at(), args, 2);
        *out_type = elem;
        return LLVMBuildLoad2(builder, llvm_type(elem), slot, "elem");
    }

    case AST_METHOD: {
        RedbType base_type, arg_type;
        LLVMValueRef base = gen_expr(node->children[0], &base_type);
        LLVMValueRef args[2];

        (void)base_type;
        args[0] = base;
        if (!strcmp(node->name, "size")) {
            *out_type = TY_INT;
            return call_fn(rt_array_size(), args, 1);
        }
        args[1] = gen_expr(node->children[1], &arg_type);
        args[1] = convert(args[1], arg_type, TY_INT);
        call_fn(rt_array_resize(), args, 2);
        *out_type = TY_VOID;
        return NULL;
    }

    default:
        cg_fail(node->line, "invalid expression node %d", (int)node->type);
        return NULL;
    }
}

static void gen_block(ASTNode *node)
{
    size_t i;

    push_scope();
    for (i = 0; i < node->child_count; i++) {
        if (block_terminated())
            break;
        gen_stmt(node->children[i]);
    }
    pop_scope();
}

static void gen_stmt(ASTNode *node)
{
    if (!node || block_terminated())
        return;

    switch (node->type) {
    case AST_PROGRAM:
    case AST_BLOCK:
        gen_block(node);
        break;

    case AST_INCLUDE:
        /* The standard runtime is linked automatically. */
        break;

    case AST_FUNC_DECL:
        /* All functions are emitted before main. */
        break;

    case AST_VAR_DECL: {
        CSym *sym = c_lookup(node->name);
        RedbType value_type;
        LLVMValueRef value = gen_expr(node->children[0], &value_type);

        if (!sym) {
            LLVMValueRef slot = entry_alloca(llvm_type(node->inferred), node->name);

            sym = c_declare(node->name, node->inferred, slot, node->line);
        }
        value = convert(value, value_type, sym->type);
        LLVMBuildStore(builder, value, sym->alloca);
        break;
    }

    case AST_FOR_LOOP: {
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
            ctx, LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder)), "for.cond");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
            ctx, LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder)), "for.body");
        LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(
            ctx, LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder)), "for.inc");
        LLVMBasicBlockRef after_bb = LLVMAppendBasicBlockInContext(
            ctx, LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder)), "for.after");
        RedbType start_type, end_type;
        LLVMValueRef start = gen_expr(node->children[0], &start_type);
        LLVMValueRef end = gen_expr(node->children[1], &end_type);
        LLVMValueRef slot;
        LLVMValueRef iter;

        start = convert(start, start_type, TY_INT);
        end = convert(end, end_type, TY_INT);

        push_scope();
        slot = entry_alloca(llvm_type(TY_INT), node->name);
        c_declare(node->name, TY_INT, slot, node->line);
        LLVMBuildStore(builder, start, slot);

        LLVMBuildBr(builder, loop_bb);
        LLVMPositionBuilderAtEnd(builder, loop_bb);
        iter = LLVMBuildLoad2(builder, llvm_type(TY_INT), slot, "i");
        LLVMBuildCondBr(builder,
                        LLVMBuildICmp(builder,
                                      node->descending ? LLVMIntSGE : LLVMIntSLE,
                                      iter, end,
                                      node->descending ? "fordowncond" : "forcond"),
                        body_bb, after_bb);

        LLVMPositionBuilderAtEnd(builder, body_bb);
        loops[loop_top].break_bb = after_bb;
        loops[loop_top].continue_bb = inc_bb;
        loop_top++;
        gen_stmt(node->children[2]);
        loop_top--;
        if (!block_terminated())
            LLVMBuildBr(builder, inc_bb);

        LLVMPositionBuilderAtEnd(builder, inc_bb);
        iter = LLVMBuildLoad2(builder, llvm_type(TY_INT), slot, "i.next");
        iter = LLVMBuildAdd(builder, iter,
                            LLVMConstInt(LLVMInt64TypeInContext(ctx),
                                         node->descending ? (unsigned long long)-1 : 1ULL, 1),
                            node->descending ? "dec" : "inc");
        LLVMBuildStore(builder, iter, slot);
        LLVMBuildBr(builder, loop_bb);

        LLVMPositionBuilderAtEnd(builder, after_bb);
        pop_scope();
        break;
    }

    case AST_WHILE_LOOP: {
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
        LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(ctx, fn, "while.cond");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(ctx, fn, "while.body");
        LLVMBasicBlockRef after_bb = LLVMAppendBasicBlockInContext(ctx, fn, "while.after");
        RedbType cond_type;
        LLVMValueRef cond;

        LLVMBuildBr(builder, cond_bb);
        LLVMPositionBuilderAtEnd(builder, cond_bb);
        cond = gen_expr(node->children[0], &cond_type);
        LLVMBuildCondBr(builder, to_bool(cond, cond_type), body_bb, after_bb);

        LLVMPositionBuilderAtEnd(builder, body_bb);
        loops[loop_top].break_bb = after_bb;
        loops[loop_top].continue_bb = cond_bb;
        loop_top++;
        gen_stmt(node->children[1]);
        loop_top--;
        if (!block_terminated())
            LLVMBuildBr(builder, cond_bb);

        LLVMPositionBuilderAtEnd(builder, after_bb);
        break;
    }

    case AST_IF: {
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
        LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(ctx, fn, "if.then");
        LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(ctx, fn, "if.else");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(ctx, fn, "if.merge");
        RedbType cond_type;
        LLVMValueRef cond = gen_expr(node->children[0], &cond_type);

        LLVMBuildCondBr(builder, to_bool(cond, cond_type), then_bb, else_bb);

        LLVMPositionBuilderAtEnd(builder, then_bb);
        gen_stmt(node->children[1]);
        if (!block_terminated())
            LLVMBuildBr(builder, merge_bb);

        LLVMPositionBuilderAtEnd(builder, else_bb);
        if (node->child_count > 2)
            gen_stmt(node->children[2]);
        if (!block_terminated())
            LLVMBuildBr(builder, merge_bb);

        LLVMPositionBuilderAtEnd(builder, merge_bb);
        break;
    }

    case AST_RETURN: {
        if (node->child_count > 0) {
            RedbType value_type;
            LLVMValueRef value = gen_expr(node->children[0], &value_type);

            value = convert(value, value_type, current_ret);
            if (current_is_main)
                value = LLVMBuildTrunc(builder, value,
                                       LLVMInt32TypeInContext(ctx), "mainret");
            LLVMBuildRet(builder, value);
        } else if (current_ret == TY_VOID) {
            LLVMBuildRetVoid(builder);
        } else if (current_is_main) {
            LLVMBuildRet(builder, LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0));
        } else {
            LLVMBuildRet(builder, LLVMConstNull(llvm_type(current_ret)));
        }
        break;
    }

    case AST_ARRAY_DECL: {
        RedbType arr_type = node->inferred;
        LLVMValueRef arr;
        LLVMValueRef slot;

        if (node->has_init) {
            ASTNode *init_node = node->children[1];

            if (init_node->type == AST_ARRAY_LITERAL) {
                LLVMValueRef size;

                if (node->has_size) {
                    RedbType size_type;
                    LLVMValueRef raw = gen_expr(node->children[0], &size_type);

                    size = convert(raw, size_type, TY_INT);
                } else {
                    size = LLVMConstInt(LLVMInt64TypeInContext(ctx),
                                        (unsigned long long)init_node->child_count, 0);
                }
                arr = gen_array_new(arr_type, size);
                gen_array_fill(arr, init_node, arr_type);
            } else {
                RedbType init_type;

                arr = gen_expr(init_node, &init_type);
            }
        } else {
            RedbType size_type;
            LLVMValueRef size = gen_expr(node->children[0], &size_type);

            arr = gen_array_new(arr_type, convert(size, size_type, TY_INT));
        }
        slot = entry_alloca(llvm_type(arr_type), node->name);
        c_declare(node->name, arr_type, slot, node->line);
        LLVMBuildStore(builder, arr, slot);
        break;
    }

    case AST_INDEX_ASSIGN: {
        ASTNode *target = node->children[0];
        RedbType base_type, index_type, value_type;
        LLVMValueRef base = gen_expr(target->children[0], &base_type);
        LLVMValueRef index = gen_expr(target->children[1], &index_type);
        LLVMValueRef value = gen_expr(node->children[1], &value_type);
        RedbType elem = redb_elem_type(base_type);
        LLVMValueRef args[2];
        LLVMValueRef slot;

        args[0] = base;
        args[1] = convert(index, index_type, TY_INT);
        slot = call_fn(rt_array_at(), args, 2);
        value = convert(value, value_type, elem);
        LLVMBuildStore(builder, value, slot);
        break;
    }

    case AST_BREAK:
        if (loop_top == 0)
            cg_fail(node->line, "'break' outside of a loop");
        LLVMBuildBr(builder, loops[loop_top - 1].break_bb);
        break;

    case AST_CONTINUE:
        if (loop_top == 0)
            cg_fail(node->line, "'continue' outside of a loop");
        LLVMBuildBr(builder, loops[loop_top - 1].continue_bb);
        break;

    default: {
        RedbType ignored;

        gen_expr(node, &ignored);
        break;
    }
    }
}

static void collect_funcs(ASTNode *node)
{
    size_t i;

    if (!node)
        return;

    if (node->type == AST_FUNC_DECL) {
        if (node->is_main) {
            if (main_node)
                cg_fail(node->line, "multiple main() definitions");
            main_node = node;
        } else {
            if (func_count >= MAX_FUNCS)
                cg_fail(node->line, "too many functions");
            snprintf(funcs[func_count].name, sizeof(funcs[func_count].name),
                     "%s", node->name);
            funcs[func_count].node = node;
            func_count++;
        }
    }

    for (i = 0; i < node->child_count; i++)
        collect_funcs(node->children[i]);
}

static void emit_function(ASTNode *node, LLVMValueRef fn)
{
    unsigned i;

    entry_bb = LLVMAppendBasicBlockInContext(ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(builder, entry_bb);

    scope_top = 0;
    push_scope();

    current_ret = node->ret_type;
    current_is_main = node->is_main;
    loop_top = 0;

    for (i = 0; i < (unsigned)node->param_count; i++) {
        char argname[32];
        LLVMValueRef slot;

        snprintf(argname, sizeof(argname), "arg%d", i + 1);
        slot = entry_alloca(llvm_type(node->param_types[i]), argname);
        LLVMBuildStore(builder, LLVMGetParam(fn, i), slot);
        c_declare(argname, node->param_types[i], slot, node->line);
    }

    gen_stmt(node->children[0]);

    if (!block_terminated()) {
        if (node->ret_type == TY_VOID)
            LLVMBuildRetVoid(builder);
        else
            LLVMBuildRet(builder, LLVMConstNull(llvm_type(node->ret_type)));
    }

    pop_scope();
}

int redb_codegen(ASTNode *ast, const char *ir_path)
{
    char *verify_error = NULL;
    int i;

    ctx = LLVMContextCreate();
    mod = LLVMModuleCreateWithNameInContext("redb_module", ctx);
    builder = LLVMCreateBuilderInContext(ctx);

    func_count = 0;
    main_node = NULL;
    collect_funcs(ast);

    if (!main_node) {
        fprintf(stderr, "redb: error: no main() function found\n");
        return -1;
    }

    for (i = 0; i < func_count; i++) {
        ASTNode *node = funcs[i].node;
        LLVMTypeRef params[REDB_MAX_PARAMS];
        LLVMTypeRef fn_type;
        int j;

        for (j = 0; j < node->param_count; j++)
            params[j] = llvm_type(node->param_types[j]);
        fn_type = LLVMFunctionType(llvm_type(node->ret_type), params,
                                   (unsigned)node->param_count, 0);
        funcs[i].fn = LLVMAddFunction(mod, funcs[i].name, fn_type);
    }

    for (i = 0; i < func_count; i++)
        emit_function(funcs[i].node, funcs[i].fn);

    {
        LLVMTypeRef main_type = LLVMFunctionType(LLVMInt32TypeInContext(ctx), NULL, 0, 0);
        LLVMValueRef main_fn = LLVMAddFunction(mod, "main", main_type);

        entry_bb = LLVMAppendBasicBlockInContext(ctx, main_fn, "entry");
        LLVMPositionBuilderAtEnd(builder, entry_bb);

        scope_top = 0;
        push_scope();
        current_ret = TY_INT;
        current_is_main = 1;
        loop_top = 0;

        gen_stmt(main_node->children[0]);

        if (!block_terminated())
            LLVMBuildRet(builder, LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0));
        pop_scope();
    }

    if (LLVMVerifyModule(mod, LLVMReturnStatusAction, &verify_error) != 0) {
        fprintf(stderr, "redb: internal error: generated invalid LLVM IR\n");
        if (verify_error)
            fprintf(stderr, "%s\n", verify_error);
        LLVMDisposeMessage(verify_error);
        return -1;
    }
    LLVMDisposeMessage(verify_error);

    if (LLVMPrintModuleToFile(mod, ir_path, &verify_error) != 0) {
        fprintf(stderr, "redb: error: could not write IR to '%s'", ir_path);
        if (verify_error)
            fprintf(stderr, ": %s", verify_error);
        fputc('\n', stderr);
        LLVMDisposeMessage(verify_error);
        return -1;
    }

    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(mod);
    LLVMContextDispose(ctx);
    return 0;
}
