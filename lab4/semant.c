#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "helper.h"
#include "env.h"
#include "semant.h"

/*Lab4: Your implementation of lab4*/
/* Func prototype */
bool isSameType(Ty_ty left, Ty_ty right);
Ty_ty actual_ty(Ty_ty ty);
Ty_fieldList getTyFieldList(A_pos pos, S_table tenv, A_fieldList fields);
Ty_fieldList actual_tys(Ty_fieldList fields);

typedef void* Tr_exp;
struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty)
{
	struct expty e;

	e.exp = exp;
	e.ty = ty;

	return e;
};

/* transProg */
void SEM_transProg(A_exp exp) {
	transExp(E_base_venv(), E_base_tenv(), exp);
}

/* transExp, all Exp kind can be found in absyn.h */
struct expty transExp(S_table venv, S_table tenv, A_exp a) {
	switch(a->kind) {
		case A_varExp:
			return transVar(venv, tenv, a->u.var);

		case A_nilExp:
			return expTy((Tr_exp)NULL, Ty_Nil());

		case A_intExp:
			return expTy((Tr_exp)NULL, Ty_Int());

		case A_stringExp:
			return expTy((Tr_exp)NULL, Ty_String());

		case A_callExp: {
			/* Check func id */
			E_enventry x = S_look(venv, get_callexp_func(a));
			if (x && x->kind == E_funEntry) {
				/* Check args */
				A_expList args;
				Ty_tyList argTypes;
				struct expty exp;
				for (args = get_callexp_args(a), argTypes = get_func_tylist(x); 
					args && argTypes; args = args->tail, argTypes = argTypes->tail) {
					exp = transExp(venv, tenv, args->head);
					if(!isSameType(argTypes->head, exp.ty)) {
						EM_error(args->head->pos, "para type mismatch");
					}
				}

				if (args) {
					EM_error(a->pos, "too many params in function %s", S_name(get_callexp_func(a)));
				}

				if (argTypes) {
					EM_error(a->pos, "too few params in function %s", S_name(get_callexp_func(a)));
				}

				return expTy((Tr_exp)NULL, get_func_res(x));
			} else {
				EM_error(a->pos, "undefined function %s", S_name(get_callexp_func(a)));
				return expTy((Tr_exp)NULL, Ty_Int());
			}
		}

		case A_opExp: {
			/* left op right */
			struct expty left = transExp(venv, tenv, get_opexp_left(a));
			struct expty right = transExp(venv, tenv, get_opexp_right(a));
			A_oper oper = get_opexp_oper(a);
			if (oper == A_plusOp || oper == A_minusOp || 
				oper == A_timesOp || oper == A_divideOp) {
				if (get_expty_kind(left) != Ty_int) {
					EM_error(get_opexp_leftpos(a), "integer required");
				}

				if (get_expty_kind(right) != Ty_int) {
					EM_error(get_opexp_rightpos(a), "integer required");
				}
			} else if (oper == A_eqOp || oper == A_neqOp) {
				if (!isSameType(left.ty, right.ty)) {
					EM_error(a->pos, "same type required");
				}
			} else {
				if (!((get_expty_kind(left) == Ty_int && get_expty_kind(right) == Ty_int) ||
                     (get_expty_kind(left) == Ty_string && get_expty_kind(right) == Ty_string))) {
            		EM_error(a->pos, "same type required");
				}
			}

			return expTy((Tr_exp)NULL, Ty_Int());
		}

		case A_recordExp: {
			/* Check whether type exists */
			S_symbol typ = get_recordexp_typ(a);
			Ty_ty ty = S_look(tenv, typ);
			if (!ty) {
				EM_error(a->pos, "undefined type %s", S_name(typ));
				return expTy((Tr_exp)NULL, Ty_Int());
			}

			/* Check whether type is a record */
			ty = actual_ty(ty);
			if (ty->kind != Ty_record) {
                EM_error(a->pos, "not a record type");
                return expTy((Tr_exp)NULL, Ty_Int());
            }

            /* Check whether the fields in efieldList match the record type */
            A_efieldList efields;
            Ty_fieldList fields;
            struct expty exp;

            for (efields = get_recordexp_fields(a), fields = ty->u.record; efields && fields; 
            	efields = efields->tail, fields = fields->tail) {
                exp = transExp(venv, tenv, efields->head->exp);

                if (efields->head->name != fields->head->name || 
                	!isSameType(exp.ty, fields->head->ty)) {
                    EM_error(efields->head->exp->pos, "type mismatch");
                }
            }

            if (efields || fields) {
                EM_error(a->pos, "type mismatch");
            }

			return expTy((Tr_exp)NULL, ty);
		}

		case A_seqExp: {
			/* The result is the type of last exp, if the exp has return */
			struct expty last = expTy((Tr_exp)NULL, Ty_Void());
			A_expList exps;
			for (exps = get_seqexp_seq(a); exps; exps = exps->tail) {
				last = transExp(venv, tenv, exps->head);
			}

			return last;
		}

		case A_assignExp: {
			/* return void */
			Ty_ty ty = Ty_Void();

			/* var := exp */
			A_var var = get_assexp_var(a);
			A_exp exp = get_assexp_exp(a);
			struct expty left = transVar(venv, tenv, var);
			struct expty right = transExp(venv, tenv, exp);

			/* Loop var cannot be assigned */
			if (var->kind == A_simpleVar) {
				E_enventry x = S_look(venv, get_simplevar_sym(var));
				if (x->readonly == 1) {
                	EM_error(a->pos, "loop variable can't be assigned");
                }
            }

            /* Check var type and exp type */
            if (!isSameType(left.ty, right.ty)) {
                EM_error(a->pos, "unmatched assign exp");
			}

			return expTy((Tr_exp)NULL, ty);
		}

		case A_ifExp: {
			/* if exp1 then exp2 (else exp3) */
			A_exp test = get_ifexp_test(a);
			A_exp then = get_ifexp_then(a);
			A_exp elsee = get_ifexp_else(a);

			/* exp1 should be int */
			struct expty exp1 = transExp(venv, tenv, test);
           	if (get_expty_kind(exp1) != Ty_int) {
            	EM_error(test->pos, "if required integer");
            }

            struct expty exp2 = transExp(venv, tenv, then);
            /* exp3 exists, check the type of exp2 and exp3 */
            if (elsee) {
                struct expty exp3 = transExp(venv, tenv, elsee);
                if (!isSameType(exp2.ty, exp3.ty)) {
                    EM_error(a->pos, "then exp and else exp type mismatch");
                }

                return expTy((Tr_exp)NULL, exp2.ty);
            } else {
                if (get_expty_kind(exp2) != Ty_void) {
                    EM_error(a->pos, "if-then exp's body must produce no value");
                }

                return expTy((Tr_exp)NULL, Ty_Void());
			}
		}

		case A_whileExp: {
			/* while exp1 do exp2 */
			A_exp test = get_whileexp_test(a);
			A_exp body = get_whileexp_body(a);

			/* exp1 should be int */
			struct expty exp1 = transExp(venv, tenv, test);
			if (get_expty_kind(exp1) != Ty_int) {
            	EM_error(test->pos, "while test required int");
            }

            /* exp2 should be void */
            struct expty exp2 = transExp(venv, tenv, body);
            if (get_expty_kind(exp2) != Ty_void) {
            	EM_error(body->pos, "while body must produce no value");
            }

            return expTy((Tr_exp)NULL, Ty_Void());
		}

		case A_forExp: {
			/* for var := exp1 to exp2 do exp3 */
			S_symbol var = get_forexp_var(a);
			A_exp lo = get_forexp_lo(a);
			A_exp hi = get_forexp_hi(a);
			A_exp body = get_forexp_body(a);

			/* exp1, exp2 should be int */
			struct expty exp1 = transExp(venv, tenv, lo);
			if (get_expty_kind(exp1) != Ty_int) {
            	EM_error(lo->pos, "for lo required int");
            }

            struct expty exp2 = transExp(venv, tenv, hi);
			if (get_expty_kind(exp2) != Ty_int) {
            	EM_error(hi->pos, "for exp's range type is not integer");
            }

            /* Enter var into env */
            S_beginScope(venv);
            S_enter(venv, var, E_ROVarEntry(Ty_Int()));
            /* exp3 should be void */
            struct expty exp3 = transExp(venv, tenv, body);
            if (get_expty_kind(exp3) != Ty_void) {
                EM_error(body->pos, "for body required void");
            }
            S_endScope(venv);

			return expTy((Tr_exp)NULL, Ty_Void());
		}

		case A_breakExp:
			return expTy((Tr_exp)NULL, Ty_Void());

		case A_letExp: {
			/* let decs in expseq end */
            A_decList decs;
            A_exp body = get_letexp_body(a);

            /* Check dec */
            S_beginScope(venv);
            S_beginScope(tenv);
            for (decs = get_letexp_decs(a); decs; decs = decs->tail) {
                transDec(venv, tenv, decs->head);
            }
            struct expty exp = transExp(venv, tenv, body);
            S_endScope(tenv);
            S_endScope(venv);

			/* The result is the type of last exp, if the exp has return */
			return expTy((Tr_exp)NULL, exp.ty);
		}

		case A_arrayExp: {
			/* var exp1 of exp2 */
			S_symbol typ = get_arrayexp_typ(a);
			A_exp size = get_arrayexp_size(a);
			A_exp init = get_arrayexp_init(a);

			/* Check whether type exists */
			Ty_ty ty = S_look(tenv, typ);
			if (!ty) {
				EM_error(a->pos, "undefined type %s", typ);
				return expTy((Tr_exp)NULL, Ty_Int());
			}

			/* Check whether type is a array */
			ty = actual_ty(ty);
			if (ty->kind != Ty_array) {
                EM_error(a->pos, "array type required");
                return expTy((Tr_exp)NULL, Ty_Int());
            }

            /* exp1 should be int */
            struct expty exp1 = transExp(venv, tenv, size);
            if (get_expty_kind(exp1) != Ty_int) {
                EM_error(size->pos, "array size required int");
            }

            /* exp2 should be the type of array */
            struct expty exp2 = transExp(venv, tenv, init);
            if (!isSameType(exp2.ty, get_ty_array(ty))) {
                EM_error(size->pos, "array init type mismatch");
            }

            return expTy((Tr_exp)NULL, ty);
		}
	}
}

struct expty transVar(S_table venv, S_table tenv, A_var v) {
	switch (v->kind) {
		case A_simpleVar: {
			/* id */
			/* Check id */
			S_symbol id = get_simplevar_sym(v);
			E_enventry x = S_look(venv, id);
			if (x && x->kind == E_varEntry) {
				return expTy((Tr_exp)NULL, get_varentry_type(x));
			} else {
				EM_error(v->pos, "undefined variable %s", S_name(id));
				return expTy((Tr_exp)NULL, Ty_Int());
			}
		}

		case A_fieldVar: {
			A_var lvalue = get_fieldvar_var(v);
			S_symbol id = get_fieldvar_sym(v);

			/* lvalue should be record */
			struct expty var = transVar(venv, tenv, lvalue);
			if (get_expty_kind(var) != Ty_record) {
				EM_error(lvalue->pos, "not a record type");
				return expTy((Tr_exp)NULL, Ty_Int());
			} 

			/* Check whether id in fields */
			Ty_fieldList fields;
			for (fields = get_record_fieldlist(var); fields; fields = fields->tail) {
				if (fields->head->name == id) {
					return expTy((Tr_exp)NULL, fields->head->ty);
				}
			}

			EM_error(lvalue->pos, "field %s doesn't exist", S_name(id));
			return expTy((Tr_exp)NULL, Ty_Int());
		}

		case A_subscriptVar: {
			/* var[exp] */
			A_var lvalue = get_subvar_var(v);
			A_exp index = get_subvar_exp(v);

			/* lvalue should be array */
			struct expty var = transVar(venv, tenv, lvalue);
			if (get_expty_kind(var) != Ty_array) {
				EM_error(lvalue->pos, "array type required");
				return expTy((Tr_exp)NULL, Ty_Int());
			} 

			/* exp should be int */
			struct expty exp = transExp(venv, tenv, index);
			if (get_expty_kind(exp) != Ty_int) {
				EM_error(index->pos, "int type required");
			} 

			return expTy((Tr_exp)NULL, get_array(var));
		}
	}
}

void transDec(S_table venv, S_table tenv, A_dec d) {
	switch (d->kind) {
		case A_functionDec: {
			/* funcList: name-sym (params-fieldList) : result-sym = body-exp......  */
			/* Put all func heads into venv */ 
			A_fundecList functions, afterFuncList;
			A_fundec func;
			S_symbol name, typ, result;
			A_fieldList params;
			A_field field;
			Ty_tyList formals, formalsTail;
			Ty_ty formal, resultTy;
			E_enventry funEntry;
			struct expty exp;
			for (functions = get_funcdec_list(d); functions; functions = functions->tail) {
				func = functions->head;
				name = func->name;
				/* Check name duplicated in funcList */
				for (afterFuncList = functions->tail; afterFuncList; 
					afterFuncList = afterFuncList->tail) {
					if (name == afterFuncList->head->name) {
						EM_error(d->pos, "two functions have the same name");
						break;
					}
				}

				/* Check fieldList */
				formals = NULL;
				formalsTail = formals;
				int count = 0;
				for (params = func->params; params; params = params->tail) {
					count++;
					field = params->head;
					typ = field->typ;
					formal = S_look(tenv, typ);
					if (!formal) {
						EM_error(d->pos, "undefined type %s", S_name(typ));
						formal = Ty_Int();
					}

					formal = actual_ty(formal);

					if (count == 1) {
						formals = Ty_TyList(NULL, NULL);
						formalsTail = formals;
					} else {
						formalsTail->tail = Ty_TyList(NULL, NULL);
						formalsTail = formalsTail->tail;
					}
					formalsTail->head = formal;
				}

				/* Check result */
				result = func->result;
				if (strcmp(S_name(result), "")) {
					resultTy = S_look(tenv, result);
					if (!resultTy) {
						EM_error(d->pos, "undefined type %s", S_name(result));
						resultTy = Ty_Int();
					}

                } else {
                    resultTy = Ty_Void();
				}

				S_enter(venv, name, E_FunEntry(formals, resultTy));
			}

			/* Check all the bodies */ 
			for (functions = get_funcdec_list(d); functions; functions = functions->tail) {
				func = functions->head;
				name = func->name;
				S_beginScope(venv);
                funEntry = S_look(venv, name);
                formals = get_func_tylist(funEntry);
                resultTy = get_func_res(funEntry);
                /* Put the params into venv */
                for (params = func->params; params && formals; params = params->tail, formals = formals->tail) {
                    field = params->head;
                    S_enter(venv, field->name, E_VarEntry(formals->head));
                }
                /* Check body */
                exp = transExp(venv, tenv, func->body);

                /* body type should be the type of result */
                if (!isSameType(exp.ty, resultTy)) {
              		if (resultTy->kind == Ty_void) {
                   	    EM_error(func->body->pos, "procedure returns value");
                	} else {
                    	EM_error(func->body->pos, "type mismatch");
					}
                }

				S_endScope(venv);
			}


			return;
		}

		case A_varDec: {
			/* sym : (type) = init */
			S_symbol var = get_vardec_var(d);
			S_symbol typ =  get_vardec_typ(d);
			A_exp init = get_vardec_init(d);

			struct expty exp = transExp(venv, tenv, init);
			/* long dec */
			if (strcmp(S_name(typ), "")) {
				/* Check whether type exists */
				Ty_ty ty = S_look(tenv, typ);
				if (!ty) {
					EM_error(d->pos, "undefined type %s", S_name(typ));
					ty = Ty_Int();
				}

				/* init type = typ */
				ty = actual_ty(ty);
				if (!isSameType(ty, exp.ty)) {
					EM_error(d->pos, "vardec type mismatch");
				}

				S_enter(venv, var, E_VarEntry(ty));
			} else {
				/* short dex */
				/* init should not be nil */
				if (get_expty_kind(exp) == Ty_nil) {
					EM_error(d->pos, "init should not be nil without type specified");
				}

				S_enter(venv, var, E_VarEntry(exp.ty));
			}

			return;
		}

		case A_typeDec: {
			/* name-sym = type-ty */
			A_nametyList nametys, afterNametys;
            int maxCircle, nowCircle;
            A_namety namety;
            S_symbol name;
            Ty_ty idTy, typeTy;

            /* Put the ty_Name into tenv */
            for (nametys = get_typedec_list(d); nametys; nametys = nametys->tail) {
            	namety = nametys->head;
            	name = namety->name;
            	/* Check name duplicated */
            	for (afterNametys = nametys->tail; afterNametys; 
            		afterNametys = afterNametys->tail) {
            		if (name == afterNametys->head->name) {
            			EM_error(d->pos, "two types have the same name");
            			break;
            		}
            	}

                S_enter(tenv, name, Ty_Name(name, NULL));
            }

            /* Check wrong cycle and put the real type into tenv */
            maxCircle = 0;
            for (nametys = get_typedec_list(d); nametys; nametys = nametys->tail) {
            	namety = nametys->head;
            	name = namety->name;
                switch(namety->ty->kind) {
                    case A_nameTy: {
                    	idTy = S_look(tenv, name);
                    	if (!idTy) {
							EM_error(d->pos, "undefined type %s", S_name(name));
							idTy = Ty_Int();
						}
                        idTy->u.name.sym = namety->ty->u.name;
                        maxCircle++;
                        break;
                    }

                    case A_recordTy: {
                        idTy = S_look(tenv, name);
                        if (!idTy) {
							EM_error(d->pos, "undefined type %s", S_name(name));
							idTy = Ty_Int();
						}
                        idTy->kind = Ty_record;
                        idTy->u.record = getTyFieldList(d->pos, tenv, namety->ty->u.record);
                        break;
                    }

                    case A_arrayTy: {
                        idTy = S_look(tenv, name);
                        if (!idTy) {
							EM_error(d->pos, "undefined type %s", S_name(name));
							idTy = Ty_Int();
						}
                        idTy->kind = Ty_array;
                        idTy->u.array = S_look(tenv, namety->ty->u.array);
                        if (!idTy->u.array) {
							EM_error(d->pos, "undefined type %s", S_name(namety->ty->u.array));
							idTy->u.array = Ty_Int();
						}
                        break;
                    }
                }
            }

            while (maxCircle > 0) {
                nowCircle = 0;
                for (nametys = get_typedec_list(d); nametys; nametys = nametys->tail) {
                	namety = nametys->head;
            		name = namety->name;
                    if (namety->ty->kind == A_nameTy) {
                        idTy = S_look(tenv, name);
                        if (!idTy) {
							EM_error(d->pos, "undefined type %s", S_name(name));
							idTy = Ty_Int();
						}
                        if (!idTy->u.name.ty) {
                            typeTy = S_look(tenv ,idTy->u.name.sym);
                            if (!typeTy) {
								EM_error(d->pos, "undefined type %s", S_name(idTy->u.name.sym));
								typeTy = Ty_Int();
							}

                            if (typeTy->kind == Ty_name) {
                                if (typeTy->u.name.ty) {
                                    idTy->u.name.ty = typeTy->u.name.ty;
                                } else {
                                    nowCircle++;
                                }
                            } else {
                                idTy->u.name.ty = typeTy;
                            }
                        }
                    }
                }

                if (nowCircle == maxCircle) {
                    EM_error(d->pos, "illegal type cycle");
                    break;
                }

                maxCircle = nowCircle;
            }

            /* Check record type and array type */
            for (nametys = get_typedec_list(d); nametys; nametys = nametys->tail) {
            	namety = nametys->head;
            	name = namety->name;
                switch(namety->ty->kind) {
                	case A_nameTy:
                    	break;
                	
                	case A_recordTy: {
                        idTy = S_look(tenv, name);
                        if (!idTy) {
							EM_error(d->pos, "undefined type %s", S_name(name));
							idTy = Ty_Int();
						}
                        idTy->u.record = actual_tys(idTy->u.record);
                        break;
                    }

                	case A_arrayTy: {
                        idTy = S_look(tenv, name);
                        if (!idTy) {
							EM_error(d->pos, "undefined type %s", S_name(name));
							idTy = Ty_Int();
						}
                        idTy->u.array = actual_ty(idTy->u.array);
                        break;
                    }
                }
			}
			return;
		}
	}
}

/* Help func */

bool isSameType(Ty_ty left, Ty_ty right)
{
	/* Warning: type a = {int,int} type b = {int,int} */
    /* They are not the same type */
	/* array and record should check its address */
	/* A none-nil record equals ty_nil */
    if (left->kind == Ty_array) {
        if (left == right) {
        	return TRUE;
        }
    } else if (left->kind == Ty_record) {
        if (left == right || right->kind == Ty_nil) {
        	return TRUE;
        }
    } else if (right->kind == Ty_record) {
        if (left == right || left->kind == Ty_nil) {
        	return TRUE;
        }
    } else {
        if (left->kind == right->kind) {
        	return TRUE;
        }
    }

    return FALSE;
}

Ty_ty actual_ty(Ty_ty ty) {
    if (ty->kind == Ty_name) {
        return ty->u.name.ty;
    } else {
        return ty;
    }
}

Ty_fieldList getTyFieldList(A_pos pos, S_table tenv, A_fieldList fields) {
    if (fields) {
    	Ty_ty ty = S_look(tenv, fields->head->typ);
		if (!ty) {
			EM_error(pos, "undefined type treelist %s", S_name(fields->head->typ));
			ty = Ty_Int();
		}
        return Ty_FieldList(Ty_Field(fields->head->name, ty), getTyFieldList(pos, tenv, fields->tail));
    } else {
        return NULL;
    }
}

Ty_fieldList actual_tys(Ty_fieldList fields) {
    if (fields) {
        return Ty_FieldList(Ty_Field(fields->head->name, actual_ty(fields->head->ty)), actual_tys(fields->tail));
    } else {
        return NULL;
    }
}