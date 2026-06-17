//
//  precalc.c
//  BigInt
//
//  Created by dlm on 14/02/2026.
//

#include "precalc.h"
#include "ast.h"

#include <stdio.h>
#include <assert.h> // should cause runtime error in target

static bool precalcExpression(ObjExpr **); // returns true if argument has reduced to Literal Int

void precalcStatements(ObjStmt* statements)
{
    ObjStmt *s = statements;
    while (s)
    {
        switch (s->obj.type)
        {
        case OBJ_STMT_RETURN:
        case OBJ_STMT_YIELD:
        case OBJ_STMT_PRINT:
        case OBJ_STMT_EXPRESSION:
            precalcExpression(&((ObjStmtExpression *) s)->expression);
            break;
        case OBJ_STMT_POKE: {
            ObjStmtPoke *poke = (ObjStmtPoke *) s;
            precalcExpression(&poke->location);
            precalcExpression(&poke->offset);
            precalcExpression(&poke->assignment);
            break;
        }
        case OBJ_STMT_VARDECLARATION: {
            ObjStmtVarDeclaration *varDecl = (ObjStmtVarDeclaration *) s;
            if (varDecl->initialiser)
            {
                precalcExpression(&varDecl->initialiser);
            }
            break;
        }
        case OBJ_STMT_PLACEDECLARATION:
            break;
        case OBJ_STMT_BLOCK: {
            ObjStmtBlock *block = (ObjStmtBlock *) s;
            precalcStatements(block->statements);
            break;
        }
        case OBJ_STMT_IF: {
            ObjStmtIf *ifelse = (ObjStmtIf *) s;
            precalcExpression(&ifelse->test);
            precalcStatements(ifelse->ifStmt);
            precalcStatements(ifelse->elseStmt);
            break;
        }
        case OBJ_STMT_FUNDECLARATION: {
            ObjStmtFunDeclaration *fun = (ObjStmtFunDeclaration *) s;
            precalcStatements(fun->body);
            break;
        }
        case OBJ_STMT_WHILE: {
            ObjStmtWhile *whileDone = (ObjStmtWhile *) s;
            precalcExpression(&whileDone->test);
            precalcStatements(whileDone->loop);
            break;
        }
        case OBJ_STMT_FOR: {
            ObjStmtFor *forNext = (ObjStmtFor *) s;
            precalcStatements(forNext->initializer);
            precalcExpression(&forNext->condition);
            precalcExpression(&forNext->loopExpression);
            precalcStatements(forNext->body);
            break;
        }
        case OBJ_STMT_CLASSDECLARATION: {
            ObjStmtClassDeclaration *classDecl = (ObjStmtClassDeclaration *) s;
            for (int i = 0; i < classDecl->methods.objectCount; i++)
            {
                ObjStmtFunDeclaration *fun = (ObjStmtFunDeclaration *) classDecl->methods.objects[i];
                precalcStatements(fun->body);
            }
            break;
        }
        case OBJ_STMT_FIELDDECLARATION: {
            ObjStmtFieldDeclaration *field = (ObjStmtFieldDeclaration *) s;
            precalcExpression(&field->offset); // todo - check if this is needed, I doubt field declarations have expressions‽
            break;
        }
        default:
            assert(!"Statement");
            break;
        }
        s = s->nextStmt;
    }
}

static bool isReducible(ObjExpr *e);
static Int *intExpr(ObjExpr *e);

bool precalcExpression(ObjExpr **ep)
{
    ObjExpr **h = ep;
    ObjExpr *e;
    bool r = true;

    while (ep != 0 && (e = *ep) != 0)
    {
        ObjType t = e->obj.type;
        switch (t)
        {
        case OBJ_EXPR_NUMBER:
        case OBJ_EXPR_STRING:
        case OBJ_EXPR_LITERAL:
            assert(e == *h);
            r = isReducible(e);
            if (e->nextExpr == 0)
            {
                return r;
            }
            break;
        case OBJ_EXPR_NAMEDVARIABLE: {
            ObjExprNamedVariable *var = (ObjExprNamedVariable *) e;
            if (var->assignment == 0)
            {
                r = false;
            }
            else
            {
                r = precalcExpression(&var->assignment);
            }
            break;
        }
        case OBJ_EXPR_OPERATION: {
            ObjExprOperation *o = (ObjExprOperation *) e;
            bool rhsIsReducecible = precalcExpression(&o->rhs);
            r = r & rhsIsReducecible;
            if (r)
            {
                IntConcrete254 result;
                int_init_concrete254(&result);
                bool reduceToResult = true;
                switch (o->operation)
                {
                    // todo - could all be done if precalc handled machine type literals
                    // EXPR_OP_EQUAL, EXPR_OP_GREATER, EXPR_OP_RIGHT_SHIFT, EXPR_OP_LESS,  EXPR_OP_LEFT_SHIFT,
                    // EXPR_OP_BIT_OR, EXPR_OP_BIT_AND, EXPR_OP_BIT_XOR,
                    // EXPR_OP_NOT_EQUAL, EXPR_OP_GREATER_EQUAL, EXPR_OP_LESS_EQUAL, EXPR_OP_NOT,
                    // EXPR_OP_LOGICAL_AND, EXPR_OP_LOGICAL_OR, EXPR_OP_DEREF_PTR
                case EXPR_OP_ADD:
                    int_add(intExpr(*h), intExpr(o->rhs), (Int *) &result);
                    break;
                case EXPR_OP_SUBTRACT:
                    int_sub(intExpr(*h), intExpr(o->rhs), (Int *) &result);
                    break;
                case EXPR_OP_MULTIPLY:
                    int_mul(intExpr(*h), intExpr(o->rhs), (Int *) &result);
                    break;
                case EXPR_OP_DIVIDE:
                    int_div(intExpr(*h), intExpr(o->rhs), (Int *) &result, 0);
                    break;
                case EXPR_OP_MODULO: {
                    IntConcrete254 q;
                    int_init_concrete254(&q);
                    int_div(intExpr(*h), intExpr(o->rhs), (Int *) &q, (Int *) &result);
                    break;
                }
                case EXPR_OP_NEGATE:
                    reduceToResult = false;
                    int_neg(intExpr(o->rhs));
                    o->rhs->nextExpr = e->nextExpr;
                    *ep = o->rhs;
                    break;
                default:
                    reduceToResult = false;
                    r = false;
                    break;
                }
                if (reduceToResult)
                {
                    ObjExprNumber *reducedResult = (ObjExprNumber *) allocateObject(sizeof (ObjExprNumber) + sizeof (uint16_t) * result.d_, OBJ_EXPR_NUMBER);
                    reducedResult->bigInt.m_ = result.d_ + result.d_ % 2;
                    reducedResult->type = NUMBER_INT;
                    int_set_t((Int *) &result, &reducedResult->bigInt);

                    reducedResult->expr.nextExpr = e->nextExpr;
                    *h = &reducedResult->expr;
                    *ep = *h;
                }
            }
            else
            {
                r = false;
            }
            break;
        }
        case OBJ_EXPR_GROUPING: {
            ObjExprGrouping *g = (ObjExprGrouping *) e;
            bool expressionIsReducecible = precalcExpression(&g->expression);
            r = r & expressionIsReducecible;
            if (expressionIsReducecible)
            {
                g->expression->nextExpr = e->nextExpr;
                *ep = g->expression; // no need to have a group of a number
            }
            break;
        }
        case OBJ_EXPR_CALL: {
            ObjExprCall *call = (ObjExprCall *) e;
            DynamicObjArray *a = &call->arguments;

            for (int i = 0; i < a->objectCount; i++)
            {
                precalcExpression((ObjExpr **) &a->objects[i]);
            }
            break;
        }
        case OBJ_EXPR_ARRAYINIT: {
            ObjExprArrayInit *array = (ObjExprArrayInit *) e;
            DynamicObjArray *a = &array->initializers;
            precalcExpression(&array->cardinality);

            for (int i = 0; i < a->objectCount; i++)
            {
                precalcExpression((ObjExpr **) &a->objects[i]);
            }
            break;
        }
        case OBJ_EXPR_BUILTIN:
            r = false;
            break;
        case OBJ_EXPR_DOT: {
            ObjExprDot *dot = (ObjExprDot *) e;
            if (dot->offset != 0)
            {
                precalcExpression(&dot->offset);
            }
            if (dot->assignment != 0)
            {
                precalcExpression(&dot->assignment);
            }
            break;
        }
        case OBJ_EXPR_ARRAYELEMENT: {
            ObjExprArrayElement* array = (ObjExprArrayElement*)e;
            precalcExpression(&array->element);
            if (array->assignment != 0)
            {
                precalcExpression(&array->assignment);
            }
            break;
        }
        default:
            //printf("%d\n", t);
            break;
        }
        ep = &(*ep)->nextExpr;
    }
    return r;
}


bool isReducible(ObjExpr *e)
{
    return (e != 0 && e->obj.type == OBJ_EXPR_NUMBER && ((ObjExprNumber *) e)->type == NUMBER_INT);
}

Int *intExpr(ObjExpr *e)
{
    assert(isReducible(e));
    return &((ObjExprNumber *) e)->bigInt;
}
