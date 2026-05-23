//
//  precalc.h
//  BigInt
//
//  Created by dlm on 14/02/2026.
//
#ifndef cyarg_bigint_precalc_h
#define cyarg_bigint_precalc_h

// Try to reduce literal int expressions as follows
// (5) => 5
// 6+7 => 13
// 7+(2*3) => 13
// Currently the parse tree makes some optimisations difficult e.g:
// x+5+2 as the parse tree loses the priority information i.e. x*5+2 results in a similar structure
// a workaround is the developer writes x+(5+2) which is optimised
// todo - the expressions x+0 and x*1 could be optimised

struct ObjStmt;
void precalcStatements(struct ObjStmt* stmt);

#endif
