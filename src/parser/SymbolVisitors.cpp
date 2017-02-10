#include "../precompiled.h" //always first

#include "SymbolVisitors.h"
#include "DataStructs.h"
#include "UtilVisitors.h"
#include "ParseError.h"
#include <assert.h>

////////////////////////////////////////////////////////////////
// BuildScriptSymbols

void BuildScriptSymbols::caseDefault(void *) {}

// Declarations

void BuildScriptSymbols::caseScript(ASTScript &host,void *param)
{
    list<ASTDecl *> ad = host.getScriptBlock()->getDeclarations();

    for(list<ASTDecl *>::iterator it = ad.begin(); it != ad.end(); it++)
    {
        (*it)->execute(*this,param);
    }
}

void BuildScriptSymbols::caseFuncDecl(ASTFuncDecl &host, void *param)
{
    pair<Scope *, SymbolTable *> *p = (pair<Scope *, SymbolTable *> *)param;
    string name = host.getName();
    vector<int> params;
    list<ASTVarDecl *> vds = host.getParams();

    for(list<ASTVarDecl *>::iterator it = vds.begin(); it != vds.end(); it++)
    {
        int type;
        ExtractType temp;
        (*it)->getType()->execute(temp, &type);
        params.push_back(type);

        if(type == ZVARTYPEID_VOID)
        {
            failure=true;
            printErrorMsg(&host, FUNCTIONVOIDPARAM, name);
        }
    }

    int rettype;
    ExtractType temp;
    host.getReturnType()->execute(temp, &rettype);
    int id = p->first->getFuncSymbols().addFunction(name,rettype,params);

    if(id == -1)
    {
        failure = true;
        printErrorMsg(&host, FUNCTIONREDEF, name);
        return;
    }

    p->second->putNodeId(&host, id);
    p->second->putFuncTypeIds(id, rettype, params);
}

void BuildScriptSymbols::caseArrayDecl(ASTArrayDecl &host, void *param)
{
    pair<Scope *, SymbolTable *> *p = (pair<Scope *, SymbolTable *> *)param;
    string name = host.getName();
    int type;
    ExtractType temp;
    host.getType()->execute(temp, &type);

    if(type == ZVARTYPEID_VOID)
    {
        failure = true;
        printErrorMsg(&host, VOIDARR, name);
    }

    if(type == ZVARTYPEID_FFC || type == ZVARTYPEID_ITEM
            || type == ZVARTYPEID_ITEMCLASS || type == ZVARTYPEID_NPC
            || type == ZVARTYPEID_LWPN || type == ZVARTYPEID_EWPN)
    {
        failure = true;
        printErrorMsg(&host, REFARR, name);
    }

    // var is always visible
    int id = p->first->getVarSymbols().addVariable(name, type);

    if(id == -1)
    {
        failure = true;
        printErrorMsg(&host, ARRREDEF, name);
        return;
    }

    p->second->putNodeId(&host, id);
    p->second->putVarTypeId(id, type);

    if(this->deprecateGlobals)
    {
        printErrorMsg(&host, DEPRECATEDGLOBAL, name);
    }

	((ASTExpr *) host.getSize())->execute(*this, param);

    if(host.getList() != NULL)
    {
        for(list<ASTExpr *>::iterator it = host.getList()->getList().begin(); it != host.getList()->getList().end(); it++)
        {
            (*it)->execute(*this,param);
        }
    }
}

void BuildScriptSymbols::caseVarDecl(ASTVarDecl &host, void *param)
{
    pair<Scope *, SymbolTable *> *p = (pair<Scope *, SymbolTable *> *)param;
    string name = host.getName();
    int type;
    ExtractType temp;
    host.getType()->execute(temp, &type);
    
    if(type == ZVARTYPEID_VOID)
    {
        failure = true;
        printErrorMsg(&host, VOIDVAR, name);
    }
    
    if(type == ZVARTYPEID_FFC || type == ZVARTYPEID_ITEM
            || type == ZVARTYPEID_ITEMCLASS || type == ZVARTYPEID_NPC
            || type == ZVARTYPEID_LWPN || type == ZVARTYPEID_EWPN)
    {
        failure = true;
        printErrorMsg(&host, REFVAR, name);
    }
    
    //var is always visible
    int id = p->first->getVarSymbols().addVariable(name, type);
    
    if(id == -1)
    {
        failure = true;
        printErrorMsg(&host, VARREDEF, name);
        return;
    }
    
    p->second->putNodeId(&host, id);
    p->second->putVarTypeId(id, type);
    
    if(this->deprecateGlobals)
    {
        printErrorMsg(&host, DEPRECATEDGLOBAL, name);
    }
}

void BuildScriptSymbols::caseVarDeclInitializer(ASTVarDeclInitializer &host, void *param)
{
    host.getInitializer()->execute(*this,param);
    caseVarDecl(host, param);
}

// Expressions

void BuildScriptSymbols::caseStringConstant(ASTStringConstant& host, void* param)
{
	pair<Scope*, SymbolTable*> *p = (pair<Scope*, SymbolTable*>*)param;
	BuildFunctionSymbols bfs;
	BFSParam newp = {p->first, p->second, SCRIPTTYPE_VOID};
	host.execute(bfs, &newp);

	if (!bfs.isOK())
		failure = true;
}

void BuildScriptSymbols::caseExprDot(ASTExprDot &host, void *param)
{
    pair<Scope *, SymbolTable *> *p = (pair<Scope *, SymbolTable *> *)param;
    BuildFunctionSymbols bfs;
    BFSParam newp = {p->first, p->second, SCRIPTTYPE_VOID};
    host.execute(bfs, &newp);
    
    if(!bfs.isOK())
    {
        failure=true;
    }
}

void BuildScriptSymbols::caseExprArray(ASTExprArray &host, void *param)
{
    pair<Scope *, SymbolTable *> *p = (pair<Scope *, SymbolTable *> *)param;
    BuildFunctionSymbols bfs;
    BFSParam newp = {p->first, p->second, SCRIPTTYPE_VOID};
    host.execute(bfs, &newp);
    
    if(!bfs.isOK())
    {
        failure=true;
    }
    
    host.getIndex()->execute(bfs, &newp);
}

void BuildScriptSymbols::caseExprArrow(ASTExprArrow &host, void *)
{
    printErrorMsg(&host, BADGLOBALINIT, "");
    failure=true;
}

void BuildScriptSymbols::caseFuncCall(ASTFuncCall &host, void *)
{
    printErrorMsg(&host, BADGLOBALINIT, "");
    failure=true;
}

//////////////////////////////////////////////////////////////
// BuildFunctionSymbols

// Statements

void BuildFunctionSymbols::caseBlock(ASTBlock &host, void *param)
{
    //push in  a new scope
    BFSParam *p = (BFSParam *)param;
    Scope *newscope = new Scope(p->scope);
    BFSParam newparam = {newscope, p->table,p->type};
    list<ASTStmt *> stmts = host.getStatements();
    
    for(list<ASTStmt *>::iterator it = stmts.begin(); it != stmts.end(); it++)
    {
        (*it)->execute(*this, &newparam);
    }
    
    delete newscope;
}

void BuildFunctionSymbols::caseStmtFor(ASTStmtFor &host, void *param)
{
    //push in new scope
    //in accordance with C++ scoping
    BFSParam *p = (BFSParam *)param;
    Scope *newscope = new Scope(p->scope);
    BFSParam newparam = {newscope, p->table, p->type};
    host.getPrecondition()->execute(*this, &newparam);
    host.getTerminationCondition()->execute(*this, &newparam);
    host.getIncrement()->execute(*this, &newparam);
    ASTStmt * stmt = host.getStmt();
    
    stmt->execute(*this,&newparam);
    delete newscope;
}

// Declarations

void BuildFunctionSymbols::caseFuncDecl(ASTFuncDecl &host, void *param)
{
    BFSParam *p = (BFSParam *)param;
    //push in the scope
    Scope *subscope = new Scope(p->scope);
    BFSParam newparam = {subscope,p->table,p->type};
    //if it's a run method, add this
    ExtractType et;
    int rtype;
    host.getReturnType()->execute(et, &rtype);

    if(host.getName() == "run" && rtype == ZVARTYPEID_VOID)
    {
        int vid = subscope->getVarSymbols().addVariable("this", p->type);
        newparam.table->putVarTypeId(vid, ScriptParser::getThisType(p->type));
        thisvid=vid;
    }

    //add the params
    list<ASTVarDecl *> vars = host.getParams();

    for(list<ASTVarDecl *>::iterator it = vars.begin(); it != vars.end(); it++)
    {
        string name = (*it)->getName();
        int type;
        ExtractType temp;
        (*it)->getType()->execute(temp, &type);
        int id = subscope->getVarSymbols().addVariable(name,type);

        if(id == -1)
        {
            printErrorMsg(*it, VARREDEF, name);
            failure = true;
            delete subscope;
            return;
        }

        p->table->putVarTypeId(id, type);
        p->table->putNodeId(*it, id);
    }

    //shortcut the block
    list<ASTStmt *> stmts = host.getBlock()->getStatements();

    for(list<ASTStmt *>::iterator it = stmts.begin(); it != stmts.end(); it++)
    {
        (*it)->execute(*this, &newparam);
    }

    delete subscope;
}

void BuildFunctionSymbols::caseArrayDecl(ASTArrayDecl &host, void *param)
{
    BFSParam *p = (BFSParam *)param;
    string name = host.getName();
    int type;
    ExtractType temp;
    host.getType()->execute(temp, &type);
    int id = p->scope->getVarSymbols().addVariable(name, type);

    if(id == -1)
    {
        printErrorMsg(&host, ARRREDEF, name);
        failure = true;
        return;
    }

    p->table->putNodeId(&host, id);
    p->table->putVarTypeId(id, type);

	((ASTExpr *) host.getSize())->execute(*this, param);

    if(host.getList() != NULL)
    {
        for(list<ASTExpr *>::iterator it = host.getList()->getList().begin(); it != host.getList()->getList().end(); it++)
        {
            (*it)->execute(*this,param);
        }
    }
}

void BuildFunctionSymbols::caseVarDecl(ASTVarDecl &host, void *param)
{
    BFSParam *p = (BFSParam *)param;
    string name = host.getName();
    int type;
    ExtractType temp;
    host.getType()->execute(temp, &type);
    int id = p->scope->getVarSymbols().addVariable(name, type);
    
    if(id == -1)
    {
        printErrorMsg(&host, VARREDEF, name);
        failure = true;
        return;
    }
    
    p->table->putNodeId(&host, id);
    p->table->putVarTypeId(id, type);
}

void BuildFunctionSymbols::caseVarDeclInitializer(ASTVarDeclInitializer &host, void *param)
{
	host.getInitializer()->execute(*this,param);
	caseVarDecl(host, param);
}

// Expressions

void BuildFunctionSymbols::caseStringConstant(ASTStringConstant& host, void* param)
{
	BFSParam *p = (BFSParam*)param;
	int id = ScriptParser::getUniqueVarID();
	p->table->putNodeId(&host, id);
}

void BuildFunctionSymbols::caseFuncCall(ASTFuncCall &host, void *param)
{
    BFSParam *p = (BFSParam *)param;
    ASTExpr *ident = host.getName();
    //ident could be a dotexpr, or an arrow exp.
    //if an arrow exp, it is not my problem right now
    bool isdot = false;
    IsDotExpr temp;
    ident->execute(temp, &isdot);
    
    if(!isdot)
    {
        //recur to get any dotexprs inside
        ident->execute(*this,param);
    }
    else
    {
        //resolve the possible functions
        ASTExprDot *dotname = (ASTExprDot *)host.getName();
        string name = dotname->getName();
        string nspace = dotname->getNamespace();
        vector<int> possibleFuncs = p->scope->getFuncsInScope(nspace, name);
        
        if(possibleFuncs.size() == 0)
        {
            string fullname;
            
            if(nspace == "")
                fullname=name;
            else
                fullname = nspace + "." + name;
                
            printErrorMsg(&host, FUNCUNDECLARED, fullname);
            failure = true;
            return;
        }
        
        p->table->putPossibleNodeFuncIds(&host, possibleFuncs);
    }
    
    for(list<ASTExpr *>::iterator it = host.getParams().begin(); it != host.getParams().end(); it++)
    {
        (*it)->execute(*this,param);
    }
}

void BuildFunctionSymbols::caseExprDot(ASTExprDot &host, void *param)
{
    BFSParam *p = (BFSParam *)param;
    string name = host.getName();
    string nspace = host.getNamespace();
    int id = p->scope->getVarInScope(nspace, name);
    
    if(id == -1 && !(nspace == "" && p->table->isConstant(name)))
    {
        string fullname;
        
        if(nspace == "")
            fullname=name;
        else
            fullname = nspace + "." + name;
            
        printErrorMsg(&host, VARUNDECLARED, fullname);
        failure = true;
        return;
    }
    
    p->table->putNodeId(&host, id);
}

void BuildFunctionSymbols::caseExprArrow(ASTExprArrow &host, void *param)
{
    //recur on the name
    host.getLVal()->execute(*this,param);
    
    //recur on the index, if it exists
    if(host.getIndex())
        host.getIndex()->execute(*this,param);
        
    //wait for type-checking to do rest of work
}

void BuildFunctionSymbols::caseExprArray(ASTExprArray &host, void *param)
{
    BFSParam *p = (BFSParam *)param;
    string name = host.getName();
    string nspace = host.getNamespace();
    int id = p->scope->getVarInScope(nspace, name);
    
    if(id == -1 && !(nspace == "" && p->table->isConstant(name)))
    {
        string fullname;
        
        if(nspace == "")
            fullname=name;
        else
            fullname = nspace + "." + name;
            
        printErrorMsg(&host, VARUNDECLARED, fullname);
        failure = true;
        return;
    }
    
    p->table->putNodeId(&host, id);
    
    if(host.getIndex())
        host.getIndex()->execute(*this,param);
}

