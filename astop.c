#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_compile.h"
#include "zend_ast.h"
#include "zend_vm_opcodes.h"

static zend_ast_process_t      original_ast_process  = NULL;
static zend_op_array *(*original_compile_file)(zend_file_handle *, int) = NULL;
static zend_op_array *(*original_compile_string)(zend_string *, const char *, zend_compile_position) = NULL;
static void (*original_execute_ex)(zend_execute_data *) = NULL;

static int astop_no_exec = 0;


/* ---- AST kind → name ---- */
static const char *ast_kind_name(zend_ast_kind kind)
{
    switch (kind) {
        /* special */
        case ZEND_AST_ZVAL:                      return "ZEND_AST_ZVAL";
        case ZEND_AST_CONSTANT:                  return "ZEND_AST_CONSTANT";
        case ZEND_AST_OP_ARRAY:                  return "ZEND_AST_OP_ARRAY";
        case ZEND_AST_ZNODE:                     return "ZEND_AST_ZNODE";
        /* declaration */
        case ZEND_AST_FUNC_DECL:                 return "ZEND_AST_FUNC_DECL";
        case ZEND_AST_CLOSURE:                   return "ZEND_AST_CLOSURE";
        case ZEND_AST_METHOD:                    return "ZEND_AST_METHOD";
        case ZEND_AST_CLASS:                     return "ZEND_AST_CLASS";
        case ZEND_AST_ARROW_FUNC:                return "ZEND_AST_ARROW_FUNC";
        case ZEND_AST_PROPERTY_HOOK:             return "ZEND_AST_PROPERTY_HOOK";
        /* list */
        case ZEND_AST_ARG_LIST:                  return "ZEND_AST_ARG_LIST";
        case ZEND_AST_ARRAY:                     return "ZEND_AST_ARRAY";
        case ZEND_AST_ENCAPS_LIST:               return "ZEND_AST_ENCAPS_LIST";
        case ZEND_AST_EXPR_LIST:                 return "ZEND_AST_EXPR_LIST";
        case ZEND_AST_STMT_LIST:                 return "ZEND_AST_STMT_LIST";
        case ZEND_AST_IF:                        return "ZEND_AST_IF";
        case ZEND_AST_SWITCH_LIST:               return "ZEND_AST_SWITCH_LIST";
        case ZEND_AST_CATCH_LIST:                return "ZEND_AST_CATCH_LIST";
        case ZEND_AST_PARAM_LIST:                return "ZEND_AST_PARAM_LIST";
        case ZEND_AST_CLOSURE_USES:              return "ZEND_AST_CLOSURE_USES";
        case ZEND_AST_PROP_DECL:                 return "ZEND_AST_PROP_DECL";
        case ZEND_AST_CONST_DECL:                return "ZEND_AST_CONST_DECL";
        case ZEND_AST_CLASS_CONST_DECL:          return "ZEND_AST_CLASS_CONST_DECL";
        case ZEND_AST_NAME_LIST:                 return "ZEND_AST_NAME_LIST";
        case ZEND_AST_TRAIT_ADAPTATIONS:         return "ZEND_AST_TRAIT_ADAPTATIONS";
        case ZEND_AST_USE:                       return "ZEND_AST_USE";
        case ZEND_AST_TYPE_UNION:                return "ZEND_AST_TYPE_UNION";
        case ZEND_AST_TYPE_INTERSECTION:         return "ZEND_AST_TYPE_INTERSECTION";
        case ZEND_AST_ATTRIBUTE_LIST:            return "ZEND_AST_ATTRIBUTE_LIST";
        case ZEND_AST_ATTRIBUTE_GROUP:           return "ZEND_AST_ATTRIBUTE_GROUP";
        case ZEND_AST_MATCH_ARM_LIST:            return "ZEND_AST_MATCH_ARM_LIST";
        case ZEND_AST_MODIFIER_LIST:             return "ZEND_AST_MODIFIER_LIST";
        /* 0 children */
        case ZEND_AST_MAGIC_CONST:               return "ZEND_AST_MAGIC_CONST";
        case ZEND_AST_TYPE:                      return "ZEND_AST_TYPE";
        case ZEND_AST_CONSTANT_CLASS:            return "ZEND_AST_CONSTANT_CLASS";
        case ZEND_AST_CALLABLE_CONVERT:          return "ZEND_AST_CALLABLE_CONVERT";
        /* 1 child */
        case ZEND_AST_VAR:                       return "ZEND_AST_VAR";
        case ZEND_AST_CONST:                     return "ZEND_AST_CONST";
        case ZEND_AST_UNPACK:                    return "ZEND_AST_UNPACK";
        case ZEND_AST_UNARY_PLUS:                return "ZEND_AST_UNARY_PLUS";
        case ZEND_AST_UNARY_MINUS:               return "ZEND_AST_UNARY_MINUS";
        case ZEND_AST_CAST:                      return "ZEND_AST_CAST";
        case ZEND_AST_CAST_VOID:                 return "ZEND_AST_CAST_VOID";
        case ZEND_AST_EMPTY:                     return "ZEND_AST_EMPTY";
        case ZEND_AST_ISSET:                     return "ZEND_AST_ISSET";
        case ZEND_AST_SILENCE:                   return "ZEND_AST_SILENCE";
        case ZEND_AST_SHELL_EXEC:                return "ZEND_AST_SHELL_EXEC";
        case ZEND_AST_PRINT:                     return "ZEND_AST_PRINT";
        case ZEND_AST_INCLUDE_OR_EVAL:           return "ZEND_AST_INCLUDE_OR_EVAL";
        case ZEND_AST_UNARY_OP:                  return "ZEND_AST_UNARY_OP";
        case ZEND_AST_PRE_INC:                   return "ZEND_AST_PRE_INC";
        case ZEND_AST_PRE_DEC:                   return "ZEND_AST_PRE_DEC";
        case ZEND_AST_POST_INC:                  return "ZEND_AST_POST_INC";
        case ZEND_AST_POST_DEC:                  return "ZEND_AST_POST_DEC";
        case ZEND_AST_YIELD_FROM:                return "ZEND_AST_YIELD_FROM";
        case ZEND_AST_CLASS_NAME:                return "ZEND_AST_CLASS_NAME";
        case ZEND_AST_GLOBAL:                    return "ZEND_AST_GLOBAL";
        case ZEND_AST_UNSET:                     return "ZEND_AST_UNSET";
        case ZEND_AST_RETURN:                    return "ZEND_AST_RETURN";
        case ZEND_AST_LABEL:                     return "ZEND_AST_LABEL";
        case ZEND_AST_REF:                       return "ZEND_AST_REF";
        case ZEND_AST_HALT_COMPILER:             return "ZEND_AST_HALT_COMPILER";
        case ZEND_AST_ECHO:                      return "ZEND_AST_ECHO";
        case ZEND_AST_THROW:                     return "ZEND_AST_THROW";
        case ZEND_AST_GOTO:                      return "ZEND_AST_GOTO";
        case ZEND_AST_BREAK:                     return "ZEND_AST_BREAK";
        case ZEND_AST_CONTINUE:                  return "ZEND_AST_CONTINUE";
        case ZEND_AST_PROPERTY_HOOK_SHORT_BODY:  return "ZEND_AST_PROPERTY_HOOK_SHORT_BODY";
        /* 2 children */
        case ZEND_AST_DIM:                       return "ZEND_AST_DIM";
        case ZEND_AST_PROP:                      return "ZEND_AST_PROP";
        case ZEND_AST_NULLSAFE_PROP:             return "ZEND_AST_NULLSAFE_PROP";
        case ZEND_AST_STATIC_PROP:               return "ZEND_AST_STATIC_PROP";
        case ZEND_AST_CALL:                      return "ZEND_AST_CALL";
        case ZEND_AST_CLASS_CONST:               return "ZEND_AST_CLASS_CONST";
        case ZEND_AST_ASSIGN:                    return "ZEND_AST_ASSIGN";
        case ZEND_AST_ASSIGN_REF:                return "ZEND_AST_ASSIGN_REF";
        case ZEND_AST_ASSIGN_OP:                 return "ZEND_AST_ASSIGN_OP";
        case ZEND_AST_BINARY_OP:                 return "ZEND_AST_BINARY_OP";
        case ZEND_AST_GREATER:                   return "ZEND_AST_GREATER";
        case ZEND_AST_GREATER_EQUAL:             return "ZEND_AST_GREATER_EQUAL";
        case ZEND_AST_AND:                       return "ZEND_AST_AND";
        case ZEND_AST_OR:                        return "ZEND_AST_OR";
        case ZEND_AST_ARRAY_ELEM:                return "ZEND_AST_ARRAY_ELEM";
        case ZEND_AST_NEW:                       return "ZEND_AST_NEW";
        case ZEND_AST_INSTANCEOF:                return "ZEND_AST_INSTANCEOF";
        case ZEND_AST_YIELD:                     return "ZEND_AST_YIELD";
        case ZEND_AST_COALESCE:                  return "ZEND_AST_COALESCE";
        case ZEND_AST_ASSIGN_COALESCE:           return "ZEND_AST_ASSIGN_COALESCE";
        case ZEND_AST_STATIC:                    return "ZEND_AST_STATIC";
        case ZEND_AST_WHILE:                     return "ZEND_AST_WHILE";
        case ZEND_AST_DO_WHILE:                  return "ZEND_AST_DO_WHILE";
        case ZEND_AST_IF_ELEM:                   return "ZEND_AST_IF_ELEM";
        case ZEND_AST_SWITCH:                    return "ZEND_AST_SWITCH";
        case ZEND_AST_SWITCH_CASE:               return "ZEND_AST_SWITCH_CASE";
        case ZEND_AST_DECLARE:                   return "ZEND_AST_DECLARE";
        case ZEND_AST_USE_TRAIT:                 return "ZEND_AST_USE_TRAIT";
        case ZEND_AST_TRAIT_PRECEDENCE:          return "ZEND_AST_TRAIT_PRECEDENCE";
        case ZEND_AST_METHOD_REFERENCE:          return "ZEND_AST_METHOD_REFERENCE";
        case ZEND_AST_NAMESPACE:                 return "ZEND_AST_NAMESPACE";
        case ZEND_AST_USE_ELEM:                  return "ZEND_AST_USE_ELEM";
        case ZEND_AST_TRAIT_ALIAS:               return "ZEND_AST_TRAIT_ALIAS";
        case ZEND_AST_GROUP_USE:                 return "ZEND_AST_GROUP_USE";
        case ZEND_AST_ATTRIBUTE:                 return "ZEND_AST_ATTRIBUTE";
        case ZEND_AST_MATCH:                     return "ZEND_AST_MATCH";
        case ZEND_AST_MATCH_ARM:                 return "ZEND_AST_MATCH_ARM";
        case ZEND_AST_NAMED_ARG:                 return "ZEND_AST_NAMED_ARG";
        case ZEND_AST_PARENT_PROPERTY_HOOK_CALL: return "ZEND_AST_PARENT_PROPERTY_HOOK_CALL";
        case ZEND_AST_PIPE:                      return "ZEND_AST_PIPE";
        /* 3 children */
        case ZEND_AST_METHOD_CALL:               return "ZEND_AST_METHOD_CALL";
        case ZEND_AST_NULLSAFE_METHOD_CALL:      return "ZEND_AST_NULLSAFE_METHOD_CALL";
        case ZEND_AST_STATIC_CALL:               return "ZEND_AST_STATIC_CALL";
        case ZEND_AST_CONDITIONAL:               return "ZEND_AST_CONDITIONAL";
        case ZEND_AST_TRY:                       return "ZEND_AST_TRY";
        case ZEND_AST_CATCH:                     return "ZEND_AST_CATCH";
        case ZEND_AST_PROP_GROUP:                return "ZEND_AST_PROP_GROUP";
        case ZEND_AST_CONST_ELEM:                return "ZEND_AST_CONST_ELEM";
        case ZEND_AST_CLASS_CONST_GROUP:         return "ZEND_AST_CLASS_CONST_GROUP";
        case ZEND_AST_CONST_ENUM_INIT:           return "ZEND_AST_CONST_ENUM_INIT";
        /* 4 children */
        case ZEND_AST_FOR:                       return "ZEND_AST_FOR";
        case ZEND_AST_FOREACH:                   return "ZEND_AST_FOREACH";
        case ZEND_AST_ENUM_CASE:                 return "ZEND_AST_ENUM_CASE";
        case ZEND_AST_PROP_ELEM:                 return "ZEND_AST_PROP_ELEM";
        /* 6 children */
        case ZEND_AST_PARAM:                     return "ZEND_AST_PARAM";
        default:                                 return "ZEND_AST_UNKNOWN";
    }
}

/* ---- AST child labels ---- */
static const char *list_item_prefix(zend_ast_kind kind)
{
    switch (kind) {
        case ZEND_AST_STMT_LIST:         return "stmt";
        case ZEND_AST_IF:                return "branch";
        case ZEND_AST_ARG_LIST:          return "arg";
        case ZEND_AST_PARAM_LIST:        return "param";
        case ZEND_AST_ARRAY:             return "elem";
        case ZEND_AST_ENCAPS_LIST:       return "part";
        case ZEND_AST_EXPR_LIST:         return "expr";
        case ZEND_AST_SWITCH_LIST:       return "case";
        case ZEND_AST_CATCH_LIST:        return "catch";
        case ZEND_AST_CLOSURE_USES:      return "use";
        case ZEND_AST_PROP_DECL:         return "prop";
        case ZEND_AST_CONST_DECL:        return "const";
        case ZEND_AST_CLASS_CONST_DECL:  return "const";
        case ZEND_AST_NAME_LIST:         return "name";
        case ZEND_AST_TRAIT_ADAPTATIONS: return "adaptation";
        case ZEND_AST_USE:               return "use";
        case ZEND_AST_TYPE_UNION:        return "type";
        case ZEND_AST_TYPE_INTERSECTION: return "type";
        case ZEND_AST_ATTRIBUTE_LIST:    return "attr";
        case ZEND_AST_ATTRIBUTE_GROUP:   return "group";
        case ZEND_AST_MATCH_ARM_LIST:    return "arm";
        case ZEND_AST_MODIFIER_LIST:     return "modifier";
        default:                         return "item";
    }
}

static const char *child_label(zend_ast_kind parent, uint32_t idx)
{
    switch (parent) {
        case ZEND_AST_IF_ELEM:
        case ZEND_AST_WHILE:
        case ZEND_AST_SWITCH_CASE:
            return idx == 0 ? "condition" : "body";
        case ZEND_AST_DO_WHILE:
            return idx == 0 ? "body" : "condition";
        case ZEND_AST_FOR:
            switch (idx) { case 0: return "init"; case 1: return "condition"; case 2: return "increment"; case 3: return "body"; }
            break;
        case ZEND_AST_FOREACH:
            switch (idx) { case 0: return "expr"; case 1: return "value"; case 2: return "key"; case 3: return "body"; }
            break;
        case ZEND_AST_BINARY_OP: case ZEND_AST_ASSIGN_OP:
        case ZEND_AST_GREATER:   case ZEND_AST_GREATER_EQUAL:
        case ZEND_AST_AND:       case ZEND_AST_OR:
        case ZEND_AST_COALESCE:  case ZEND_AST_ASSIGN_COALESCE:
        case ZEND_AST_PIPE:
            return idx == 0 ? "left" : "right";
        case ZEND_AST_UNARY_OP:    case ZEND_AST_UNARY_PLUS: case ZEND_AST_UNARY_MINUS:
        case ZEND_AST_RETURN:      case ZEND_AST_ECHO:        case ZEND_AST_PRINT:
        case ZEND_AST_THROW:       case ZEND_AST_UNSET:       case ZEND_AST_ISSET:
        case ZEND_AST_EMPTY:       case ZEND_AST_UNPACK:      case ZEND_AST_YIELD_FROM:
        case ZEND_AST_PRE_INC:     case ZEND_AST_PRE_DEC:
        case ZEND_AST_POST_INC:    case ZEND_AST_POST_DEC:
        case ZEND_AST_SILENCE:     case ZEND_AST_SHELL_EXEC:
        case ZEND_AST_INCLUDE_OR_EVAL:
        case ZEND_AST_GLOBAL:      case ZEND_AST_REF:
        case ZEND_AST_CLASS_NAME:  case ZEND_AST_PROPERTY_HOOK_SHORT_BODY:
            return "expr";
        case ZEND_AST_VAR:   case ZEND_AST_CONST: case ZEND_AST_LABEL:
        case ZEND_AST_GOTO:  case ZEND_AST_BREAK: case ZEND_AST_CONTINUE:
            return "name";
        case ZEND_AST_ASSIGN: case ZEND_AST_ASSIGN_REF:
            return idx == 0 ? "var" : "expr";
        case ZEND_AST_CALL:
            return idx == 0 ? "callable" : "args";
        case ZEND_AST_METHOD_CALL: case ZEND_AST_NULLSAFE_METHOD_CALL:
            switch (idx) { case 0: return "object"; case 1: return "method"; case 2: return "args"; }
            break;
        case ZEND_AST_STATIC_CALL:
            switch (idx) { case 0: return "class"; case 1: return "method"; case 2: return "args"; }
            break;
        case ZEND_AST_NEW:
            return idx == 0 ? "class" : "args";
        case ZEND_AST_DIM:
            return idx == 0 ? "expr" : "dim";
        case ZEND_AST_PROP: case ZEND_AST_NULLSAFE_PROP:
            return idx == 0 ? "object" : "prop";
        case ZEND_AST_STATIC_PROP:
            return idx == 0 ? "class" : "prop";
        case ZEND_AST_CLASS_CONST:
            return idx == 0 ? "class" : "const";
        case ZEND_AST_CONDITIONAL:
            switch (idx) { case 0: return "condition"; case 1: return "if_true"; case 2: return "if_false"; }
            break;
        case ZEND_AST_TRY:
            switch (idx) { case 0: return "body"; case 1: return "catches"; case 2: return "finally"; }
            break;
        case ZEND_AST_CATCH:
            switch (idx) { case 0: return "types"; case 1: return "var"; case 2: return "body"; }
            break;
        case ZEND_AST_SWITCH:
            return idx == 0 ? "subject" : "cases";
        case ZEND_AST_MATCH:
            return idx == 0 ? "subject" : "arms";
        case ZEND_AST_MATCH_ARM:
            return idx == 0 ? "conditions" : "body";
        case ZEND_AST_YIELD: case ZEND_AST_ARRAY_ELEM:
            return idx == 0 ? "value" : "key";
        case ZEND_AST_STATIC:
            return idx == 0 ? "var" : "default";
        case ZEND_AST_INSTANCEOF:
            return idx == 0 ? "expr" : "class";
        case ZEND_AST_NAMED_ARG:
            return idx == 0 ? "name" : "value";
        case ZEND_AST_NAMESPACE:
            return idx == 0 ? "name" : "stmts";
        case ZEND_AST_USE_ELEM: case ZEND_AST_TRAIT_ALIAS:
            return idx == 0 ? "name" : "alias";
        case ZEND_AST_GROUP_USE:
            return idx == 0 ? "prefix" : "uses";
        case ZEND_AST_ATTRIBUTE:
            return idx == 0 ? "name" : "args";
        case ZEND_AST_DECLARE:
            return idx == 0 ? "declares" : "stmts";
        case ZEND_AST_USE_TRAIT:
            return idx == 0 ? "traits" : "adaptations";
        case ZEND_AST_TRAIT_PRECEDENCE:
            return idx == 0 ? "method" : "insteadof";
        case ZEND_AST_METHOD_REFERENCE:
            return idx == 0 ? "class" : "method";
        case ZEND_AST_PROP_GROUP:
            switch (idx) { case 0: return "type"; case 1: return "props"; case 2: return "attrs"; }
            break;
        case ZEND_AST_PROP_ELEM:
            switch (idx) { case 0: return "name"; case 1: return "default"; case 2: return "docblock"; case 3: return "hooks"; }
            break;
        case ZEND_AST_CONST_ELEM:
            switch (idx) { case 0: return "name"; case 1: return "value"; case 2: return "docblock"; }
            break;
        case ZEND_AST_CLASS_CONST_GROUP:
            switch (idx) { case 0: return "consts"; case 1: return "attrs"; case 2: return "type"; }
            break;
        case ZEND_AST_CONST_ENUM_INIT:
            switch (idx) { case 0: return "class_name"; case 1: return "case_name"; case 2: return "value"; }
            break;
        case ZEND_AST_ENUM_CASE:
            switch (idx) { case 0: return "name"; case 1: return "value"; case 2: return "attrs"; case 3: return "docblock"; }
            break;
        case ZEND_AST_FUNC_DECL: case ZEND_AST_CLOSURE:
        case ZEND_AST_METHOD:    case ZEND_AST_ARROW_FUNC:
        case ZEND_AST_PROPERTY_HOOK:
            switch (idx) { case 0: return "params"; case 1: return "uses"; case 2: return "body"; case 3: return "return_type"; }
            break;
        case ZEND_AST_CLASS:
            switch (idx) { case 0: return "extends"; case 1: return "implements"; case 2: return "body"; case 3: return "attrs"; }
            break;
        case ZEND_AST_PARAM:
            switch (idx) { case 0: return "type"; case 1: return "default"; case 2: return "attrs"; case 3: return "hooks"; }
            break;
        case ZEND_AST_PARENT_PROPERTY_HOOK_CALL:
            return idx == 0 ? "name" : "args";
    }
    return NULL;
}

/* ---- AST dump ---- */
static void format_zval_str(char *buf, size_t buf_size, zval *val)
{
    switch (Z_TYPE_P(val)) {
        case IS_NULL:   snprintf(buf, buf_size, "null"); break;
        case IS_FALSE:  snprintf(buf, buf_size, "false"); break;
        case IS_TRUE:   snprintf(buf, buf_size, "true"); break;
        case IS_LONG:   snprintf(buf, buf_size, ZEND_LONG_FMT, Z_LVAL_P(val)); break;
        case IS_DOUBLE: snprintf(buf, buf_size, "%g", Z_DVAL_P(val)); break;
        case IS_STRING: {
            size_t len = Z_STRLEN_P(val) > 24 ? 24 : Z_STRLEN_P(val);
            char tmp[28]; size_t j = 0;
            for (size_t k = 0; k < len && j < sizeof(tmp) - 1; k++) {
                char c = Z_STRVAL_P(val)[k];
                if      (c == '\n' && j < sizeof(tmp)-2) { tmp[j++]='\\'; tmp[j++]='n'; }
                else if (c == '\r' && j < sizeof(tmp)-2) { tmp[j++]='\\'; tmp[j++]='r'; }
                else { tmp[j++] = c; }
            }
            tmp[j] = '\0';
            snprintf(buf, buf_size, "\"%s\"", tmp);
            break;
        }
        default: snprintf(buf, buf_size, "zval(t=%d)", Z_TYPE_P(val)); break;
    }
}

static void format_ast_extra(char *buf, size_t buf_size, zend_ast *ast)
{
    buf[0] = '\0';
    switch (ast->kind) {
        case ZEND_AST_ZVAL: {
            format_zval_str(buf, buf_size, zend_ast_get_zval(ast));
            break;
        }
        case ZEND_AST_FUNC_DECL:
        case ZEND_AST_CLOSURE:
        case ZEND_AST_METHOD:
        case ZEND_AST_CLASS:
        case ZEND_AST_ARROW_FUNC:
        case ZEND_AST_PROPERTY_HOOK: {
            zend_ast_decl *decl = (zend_ast_decl *)ast;
            if (decl->name) snprintf(buf, buf_size, "name=%s", ZSTR_VAL(decl->name));
            break;
        }
        case ZEND_AST_BINARY_OP:
        case ZEND_AST_ASSIGN_OP:
        case ZEND_AST_UNARY_OP: {
            const char *op = zend_get_opcode_name(ast->attr);
            snprintf(buf, buf_size, "op=%s", op ? op : "?");
            break;
        }
        case ZEND_AST_CAST: {
            const char *type;
            switch (ast->attr) {
                case IS_NULL:    type = "null";   break;
                case IS_LONG:    type = "int";    break;
                case IS_DOUBLE:  type = "float";  break;
                case IS_STRING:  type = "string"; break;
                case IS_ARRAY:   type = "array";  break;
                case IS_OBJECT:  type = "object"; break;
                case _IS_BOOL:   type = "bool";   break;
                default:         type = "?";      break;
            }
            snprintf(buf, buf_size, "to=%s", type);
            break;
        }
        case ZEND_AST_MAGIC_CONST: {
            /* token values from zend_language_parser.h */
            const char *mc;
            switch (ast->attr) {
                case 346: mc = "__LINE__";      break;
                case 347: mc = "__FILE__";      break;
                case 348: mc = "__DIR__";       break;
                case 349: mc = "__CLASS__";     break;
                case 350: mc = "__TRAIT__";     break;
                case 351: mc = "__METHOD__";    break;
                case 352: mc = "__FUNCTION__";  break;
                case 354: mc = "__NAMESPACE__"; break;
                default:  mc = "?";             break;
            }
            snprintf(buf, buf_size, "%s", mc);
            break;
        }
        case ZEND_AST_INCLUDE_OR_EVAL: {
            const char *inc;
            switch (ast->attr) {
                case ZEND_EVAL:         inc = "eval";         break;
                case ZEND_INCLUDE:      inc = "include";      break;
                case ZEND_INCLUDE_ONCE: inc = "include_once"; break;
                case ZEND_REQUIRE:      inc = "require";      break;
                case ZEND_REQUIRE_ONCE: inc = "require_once"; break;
                default:                inc = "?";            break;
            }
            snprintf(buf, buf_size, "%s", inc);
            break;
        }
    }
}

static void dump_ast(zend_ast *ast, int depth, const char *label)
{
    if (!ast) return;
    if (depth > 200) {
        for (int i = 0; i < depth; i++) fprintf(stderr,"  ");
        fprintf(stderr,"...\n");
        return;
    }

    char extra[64];
    format_ast_extra(extra, sizeof(extra), ast);
    for (int i = 0; i < depth; i++) fprintf(stderr,"  ");
    if (label && extra[0])
        fprintf(stderr,"%s: %s(%s) lineno=%u\n", label, ast_kind_name(ast->kind), extra, zend_ast_get_lineno(ast));
    else if (label)
        fprintf(stderr,"%s: %s lineno=%u\n", label, ast_kind_name(ast->kind), zend_ast_get_lineno(ast));
    else if (extra[0])
        fprintf(stderr,"%s(%s) lineno=%u\n", ast_kind_name(ast->kind), extra, zend_ast_get_lineno(ast));
    else
        fprintf(stderr,"%s lineno=%u\n", ast_kind_name(ast->kind), zend_ast_get_lineno(ast));

    if (zend_ast_is_list(ast)) {
        zend_ast_list *list = zend_ast_get_list(ast);
        const char *prefix = list_item_prefix(ast->kind);
        char lbl[24];
        for (uint32_t i = 0; i < list->children; i++) {
            snprintf(lbl, sizeof(lbl), "%s[%u]", prefix, i);
            dump_ast(list->child[i], depth + 1, lbl);
        }
    } else if (ast->kind == ZEND_AST_FUNC_DECL || ast->kind == ZEND_AST_CLOSURE ||
               ast->kind == ZEND_AST_METHOD    || ast->kind == ZEND_AST_CLASS    ||
               ast->kind == ZEND_AST_ARROW_FUNC || ast->kind == ZEND_AST_PROPERTY_HOOK) {
        /* zend_ast_decl has child[4]; zend_ast_get_num_children() misreads the kind
           for these special-range kinds and returns 1 instead of 4. */
        zend_ast_decl *decl = (zend_ast_decl *)ast;
        for (int i = 0; i < 4; i++)
            dump_ast(decl->child[i], depth + 1, child_label(ast->kind, i));
    } else {
        for (uint32_t i = 0; i < zend_ast_get_num_children(ast); i++)
            dump_ast(ast->child[i], depth + 1, child_label(ast->kind, i));
    }
}

static void astop_ast_process(zend_ast *ast)
{
    fprintf(stderr,"\n=== AST ===\n");
    dump_ast(ast, 0, NULL);
    fprintf(stderr,"\n");
    if (original_ast_process) original_ast_process(ast);
}

/* ---- operand formatting ---- */
static void format_operand(char *buf, size_t buf_size,
    zend_op_array *op_array, zend_op *op, znode_op operand, uint8_t op_type)
{
    uint8_t base_type = op_type & 0x0F;
    const char *branch_suffix =
        (op_type & IS_SMART_BRANCH_JMPZ)  ? "/jmpz"  :
        (op_type & IS_SMART_BRANCH_JMPNZ) ? "/jmpnz" : "";

    switch (base_type) {
        case IS_UNUSED:
            snprintf(buf, buf_size, "-");
            break;
        case IS_CONST:
            format_zval_str(buf, buf_size, RT_CONSTANT(op, operand));
            break;
        case IS_TMP_VAR:
            snprintf(buf, buf_size, "~%u%s", EX_VAR_TO_NUM(operand.var), branch_suffix);
            break;
        case IS_VAR:
            snprintf(buf, buf_size, "@%u%s", EX_VAR_TO_NUM(operand.var), branch_suffix);
            break;
        case IS_CV: {
            uint32_t var_num = EX_VAR_TO_NUM(operand.var);
            if (var_num < (uint32_t)op_array->last_var) {
                snprintf(buf, buf_size, "$%s%s", ZSTR_VAL(op_array->vars[var_num]), branch_suffix);
            } else {
                snprintf(buf, buf_size, "$cv%u%s", var_num, branch_suffix);
            }
            break;
        }
        default:
            snprintf(buf, buf_size, "?(t=%u)", base_type);
            break;
    }
}

/* ---- opcode dump (traversal order) ---- */
static void dump_op_array(zend_op_array *op_array)
{
    const char *name = op_array->function_name
        ? ZSTR_VAL(op_array->function_name) : "{main}";

#define OPCOL_W 13

    fprintf(stderr,"=== OPCODES %s ===\n", name);
    fprintf(stderr,"%6s %4s  %-26s %-*s %-*s %s\n",
        "line", "num", "opcode",
        OPCOL_W, "op1", OPCOL_W, "op2", "result");
    for (uint32_t i = 0; i < op_array->last; i++) {
        zend_op *op = &op_array->opcodes[i];
        char op1[64], op2[64], result[64];
        format_operand(op1, sizeof(op1), op_array, op, op->op1, op->op1_type);
        format_operand(op2, sizeof(op2), op_array, op, op->op2, op->op2_type);
        format_operand(result, sizeof(result), op_array, op, op->result, op->result_type);
        char *cols[] = { op1, op2, result };
        for (int c = 0; c < 3; c++) {
            if ((int)strlen(cols[c]) > OPCOL_W) {
                cols[c][OPCOL_W - 1] = '>';
                cols[c][OPCOL_W] = '\0';
            }
        }
        fprintf(stderr,"%6u %4u  %-26s %-*s %-*s %s\n",
            op->lineno, i, (zend_get_opcode_name(op->opcode) ?: "?"),
            OPCOL_W, op1, OPCOL_W, op2, result);
    }

#undef OPCOL_W
    fprintf(stderr,"\n");

    for (uint32_t i = 0; i < op_array->num_dynamic_func_defs; i++)
        dump_op_array(op_array->dynamic_func_defs[i]);
}

static void dump_class_methods(zend_class_entry *ce)
{
    zend_string *method_name;
    zend_function *method;
    ZEND_HASH_MAP_FOREACH_STR_KEY_PTR(&ce->function_table, method_name, method) {
        if (method_name && method->type == ZEND_USER_FUNCTION) {
            dump_op_array(&method->op_array);
        }
    } ZEND_HASH_FOREACH_END();
}

static zend_op_array *astop_compile_file(zend_file_handle *fh, int type)
{
    HashTable ft_snapshot, ct_snapshot;
    zend_hash_init(&ft_snapshot, 32, NULL, NULL, 0);
    zend_hash_init(&ct_snapshot, 16, NULL, NULL, 0);
    zend_string *key;
    ZEND_HASH_MAP_FOREACH_STR_KEY(EG(function_table), key) {
        if (key) zend_hash_add_empty_element(&ft_snapshot, key);
    } ZEND_HASH_FOREACH_END();
    ZEND_HASH_MAP_FOREACH_STR_KEY(EG(class_table), key) {
        if (key) zend_hash_add_empty_element(&ct_snapshot, key);
    } ZEND_HASH_FOREACH_END();

    zend_op_array *op_array = original_compile_file(fh, type);
    if (!op_array) {
        zend_hash_destroy(&ft_snapshot);
        zend_hash_destroy(&ct_snapshot);
        return NULL;
    }

    dump_op_array(op_array);

    zend_function *func;
    ZEND_HASH_MAP_FOREACH_STR_KEY_PTR(EG(function_table), key, func) {
        if (key && !zend_hash_exists(&ft_snapshot, key) && func->type == ZEND_USER_FUNCTION) {
            dump_op_array(&func->op_array);
        }
    } ZEND_HASH_FOREACH_END();

    zend_class_entry *ce;
    ZEND_HASH_MAP_FOREACH_STR_KEY_PTR(EG(class_table), key, ce) {
        if (key && !zend_hash_exists(&ct_snapshot, key)) {
            dump_class_methods(ce);
        }
    } ZEND_HASH_FOREACH_END();

    fflush(stdout);
    zend_hash_destroy(&ft_snapshot);
    zend_hash_destroy(&ct_snapshot);
    return op_array;
}

static zend_op_array *astop_compile_string(zend_string *source, const char *filename, zend_compile_position position)
{
    zend_op_array *op_array = original_compile_string(source, filename, position);
    if (op_array) {
        dump_op_array(op_array);
        fflush(stdout);
    }
    return op_array;
}

/* ---- no-exec mode (optional) ---- */
static void astop_execute_ex(zend_execute_data *data)
{
    /* intentionally empty */
}

/* ---- module lifecycle ---- */
PHP_MINIT_FUNCTION(astop)
{
    const char *mode = getenv("ASTOP_MODE");
    astop_no_exec = (mode && strcmp(mode, "no_exec") == 0);

    original_ast_process  = zend_ast_process;
    zend_ast_process      = astop_ast_process;

    original_compile_file = zend_compile_file;
    zend_compile_file     = astop_compile_file;

    original_compile_string = zend_compile_string;
    zend_compile_string     = astop_compile_string;

    if (astop_no_exec) {
        original_execute_ex = zend_execute_ex;
        zend_execute_ex     = astop_execute_ex;
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(astop)
{
    zend_ast_process    = original_ast_process;
    zend_compile_file   = original_compile_file;
    zend_compile_string = original_compile_string;
    if (original_execute_ex) {
        zend_execute_ex = original_execute_ex;
    }
    return SUCCESS;
}

static zend_module_entry astop_module_entry = {
    STANDARD_MODULE_HEADER,
    "astop",
    NULL,
    PHP_MINIT(astop),
    PHP_MSHUTDOWN(astop),
    NULL, NULL, NULL,
    "0.1.0",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_ASTOP
ZEND_GET_MODULE(astop)
#endif
