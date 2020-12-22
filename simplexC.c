#include "fxlib.h"
#include "revolution.h"
#include "limits.h"
#include <stdlib.h>
#include "stdio.h"
#include "SH4_compatibility.h"

#define NO_EVAL_VALUE INT_MAX

#define TYPE_LP 1
#define TYPE_ILP 2
#define TYPE_CUT 3

#define TYPE_INPUT 1
#define TYPE_SOLVE 2

#define INITIALIZE_VALUE_OPT -1
#define EXIT_OPT 0

#define INE_EQUAL 0
#define INE_LESS_OR_EQUAL_THAN 1
#define INE_GREATER_OR_EQUAL_THAN 2


#define FUNC_MINIMIZE 0
#define FUNC_MAXIMIZE 1


#define MODE_FULL_EXECUTION 0
#define MODE_INPUT_TABLE 1


struct iteration
{
    int numIteration;
    int* idBasicVariables;
    int* idNoBasicVariables;
    int idVarIn;
    int idVarOut;

    double **Binv;
    int BinvSize;

    double *xb;
    double *ct;
    double *ctBinv;
    double *zj;
    double *cjMinusZj;
    double *yj;
    double *xbDivYj;

    double zSol ;
};

struct problemStatement
{
    int problemType;
    int modelType;
    int is2fasesNeeded;//1 en caso de si. 0 en caso que no
    //1 array de variables por cada constraint
    //Cada constraint tiene primero todas las variables input, luego todas las slope y finalmente las 2fases
    double **constraints;
    double *rightValues;
    int *inequalitySigns;
    int nVariables;
    int nVariablesSlope;
    int nVariables2fases;
    int nVariablesInteger;
    int nConstraints;
    int *idIntegerVariables;//1 en caso de que sea. 0 en caso que no
    int *idSlopeVariables;//1 en caso de que sea. 0 en caso que no
    int *id2fasesVariables;//1 en caso de que sea. 0 en caso que no
    int *idConstraintToSlopeVar;//Guarda la id de la variable que es slope perteneciante a esa contraint. -1 en caso de no tener
    int *idConstraintTo2fasesVar;//Guarda la id de la variable que es 2fases perteneciante a esa contraint. -1 en caso de no tener

    int funcObjectivePurpose;
    double *funcObjtValues;//Ultima posicion, termino independiente sin variables
    double *funcObjtValues2Fases;

    double** ajVector; // guarda un vector por cada variable y contiene su valor en todas las constraints.
};

struct node
{
    int id;
    int nIterations;
    struct iteration **its;
};

struct execution
{
    int mode;
    struct problemStatement *initialProblemStatement;
    struct problemStatement *canonicalStatement;
    struct node **nodes;
    int nNodes;
};

double InputD(int, int);

int InputI(int, int);

double InputD(int x, int y)
{
    char *string[128], *stopstring;
    string_input(x, y, string);
    return strtod(string, &stopstring);
}

int InputI(int x, int y)
{
    char *string[128], *stopstring;
    string_input(x, y, string);
    return atoi(&string);
}

int freeMemoryStatement(struct problemStatement *pS)
{
    int i;
    for(i=0;i<pS->nConstraints;i++)
    {
        free(pS->constraints[i]);
    }
    free(pS->constraints);
    free(pS->rightValues);
    free(pS->inequalitySigns);
    free(pS->idIntegerVariables);
    free(pS->idSlopeVariables);
    free(pS->id2fasesVariables);
    free(pS->funcObjtValues);
    if(pS->modelType==TYPE_SOLVE)
    {
        free(pS->idConstraintTo2fasesVar);
        free(pS->idConstraintToSlopeVar);
        for(i=0;pS->nVariables;i++)
        {
            free(pS->ajVector[i]);
        }
        free(pS->ajVector);
    }
    return 0;
}
int freeMemoryIteration(struct iteration *it)
{
    //TODO
    free(it->idBasicVariables);
    return 0;
}
int freeMemoryNode(struct node *n)
{
    int i;
    for(i=0; i<n->nIterations;i++)
    {
        freeMemoryIteration(n->its[i]);
    }
    return 0;
}

int freeMemoryExecution(struct execution *ex)
{
    int i;
    if(ex->mode == MODE_FULL_EXECUTION)
    {
        freeMemoryStatement(ex->initialProblemStatement);
        freeMemoryStatement(ex->canonicalStatement);
    }
    for(i=0;i<ex->nNodes;i++)
    {
        freeMemoryNode(ex->nodes[i]);
    }
    return 0;
}

int lengthConcatenate(int sprintLength)
{
    return (sprintLength > 0) ? sprintLength : 0;
}

char* getInequalityChar(int inequalitySign)
{
    char * res;
    switch (inequalitySign)
    {
        case INE_EQUAL: res="="; break;
        case INE_LESS_OR_EQUAL_THAN: res="<="; break;
        case INE_GREATER_OR_EQUAL_THAN: res=">="; break;
        default: res = "ERROR"; break;
    }
    return res;
}

char * getFuncObjPurposeChar(int purpose)
{
    char * res;
    switch (purpose)
    {
        case FUNC_MINIMIZE: res="MIN"; break;
        case FUNC_MAXIMIZE: res="MAX"; break;
        default: res = "ERROR";break;
    }
    return res;
} 

int printObjFunc(struct problemStatement* pS,int modelType)
{
    char stroutTop[128],stroutBasicInfo[128],stroutBasicInfo2[128],stroutBasicInfo3[128],stroutInfoVar[128];
    char *uselessSring[128];
    int menuChoice;
    menuChoice=INITIALIZE_VALUE_OPT;
    sprintf(stroutTop,"Problem statement %s, OBJ. Exit %d",(pS->modelType==TYPE_INPUT) ? "INPUT" :"SOLVE", EXIT_OPT);
    sprintf(stroutBasicInfo, "NConstraints: %d, NVariables: %d", pS->nConstraints, pS->nVariables);
    if(modelType==TYPE_INPUT)
    {
        sprintf(stroutBasicInfo2, "OBJ %s", getFuncObjPurposeChar(pS->funcObjectivePurpose));
        sprintf(stroutBasicInfo3, "Ind term: %.2f. Sel var", pS->funcObjtValues[pS->nVariables]);
    }else
    {
        sprintf(stroutBasicInfo2, "OBJ MIN");
        sprintf(stroutBasicInfo3, "Ind term: %.2f. Sel var", pS->funcObjtValues2Fases[pS->nVariables]);
    }
    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        PrintMini(0,0,  (unsigned char *)stroutTop, MINI_OVER);
        PrintMini(0,7,  (unsigned char *)stroutBasicInfo, MINI_OVER);
        PrintMini(0, 14, (unsigned char*)stroutBasicInfo2, MINI_OVER);
        PrintMini(0, 21, (unsigned char*)stroutBasicInfo3, MINI_OVER);
        menuChoice = InputI(0, 35);
        Bdisp_PutDisp_DD();
        if(menuChoice!=EXIT_OPT && menuChoice<=pS->nVariables)
        {
            if(modelType==TYPE_INPUT)
            {
                sprintf(stroutInfoVar,"Obj val x%d: %.2f", menuChoice,pS->funcObjtValues[menuChoice-1]);
            }else
            {
                sprintf(stroutInfoVar,"Obj val x%d: %.2f", menuChoice,pS->funcObjtValues2Fases[menuChoice-1]);
            }
            
            PrintMini(0, 42, (unsigned char*)stroutInfoVar, MINI_OVER);
            PrintMini(0, 49, (unsigned char*)"Press any key to continue", MINI_OVER);
            string_input(0, 56, uselessSring);
            Bdisp_PutDisp_DD();
        }
    }
    return 0;
}

int printAjVal(struct problemStatement* pS, int varId)
{
    char stroutBasicInfo[128],stroutVariable[128];
    char *uselessSring[128];
    int menuChoice;
    menuChoice=INITIALIZE_VALUE_OPT;
    sprintf(stroutBasicInfo, "Ax%d. nConstraints: %d, Exit %d",varId,pS->nConstraints,EXIT_OPT);
    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        PrintMini(0, 0, (unsigned char*)stroutBasicInfo, MINI_OVER);
        menuChoice=InputI(0,7);
        Bdisp_PutDisp_DD();
        if(menuChoice!=EXIT_OPT && menuChoice<=pS->nConstraints)
        {
            Bdisp_AllClr_DDVRAM();
            sprintf(stroutVariable, "Ax%d. %.2f Pos %d",varId,pS->ajVector[varId-1][menuChoice-1],menuChoice);
            PrintMini(0,0,(unsigned char*) stroutVariable,MINI_OVER);
            PrintMini(0, 7, (unsigned char*)"Press any key to continue", MINI_OVER);
            string_input(0, 14, uselessSring);
            Bdisp_PutDisp_DD();
            memset(stroutVariable,0,128);
        }
    }
    return 0;
}

int printAjVec(struct problemStatement* pS)
{
    char stroutBasicInfo[128];
    int menuChoice;
    menuChoice=INITIALIZE_VALUE_OPT;
    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        sprintf(stroutBasicInfo, "Axj pMenu Vars %d.  %d exit",pS->nVariables,EXIT_OPT);
        PrintMini(0, 0, (unsigned char*)stroutBasicInfo, MINI_OVER);
        menuChoice = InputI(0, 7);
        Bdisp_PutDisp_DD();
        if(menuChoice!=EXIT_OPT && menuChoice<=pS->nVariables)
        {
            printAjVal(pS,menuChoice);
        }
    }
    return 0;
}

int printStatementMenu(struct problemStatement* pS)
{
    char stroutTop[128],stroutBasicInfo[128],stroutObjInfo[128];
    int menuChoice;
    menuChoice=INITIALIZE_VALUE_OPT;
    sprintf(stroutTop,"Problem statement %s, printMenu. Exit %d",(pS->modelType==TYPE_INPUT) ? "INPUT" :"SOLVE", EXIT_OPT);
    sprintf(stroutBasicInfo, "NConstraints: %d, NVariables: %d", pS->nConstraints, pS->nVariables);

    if(pS->modelType==TYPE_SOLVE && pS->is2fasesNeeded)
    {
        sprintf(stroutObjInfo,"2 Pinfo obj func. 3 Pinfo 2fases");
    }else
    {
        sprintf(stroutObjInfo,"2 Pinfo obj func.");
    }
    
    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        
        PrintMini(0,0,  (unsigned char *)stroutTop, MINI_OVER);
        PrintMini(0,7,  (unsigned char *)stroutBasicInfo, MINI_OVER);
        PrintMini(0, 14, (unsigned char*)"1 Pinfo constraint", MINI_OVER);
        PrintMini(0, 21, (unsigned char*)stroutObjInfo, MINI_OVER);
        if(pS->modelType==TYPE_SOLVE)
        {
            PrintMini(0, 28, (unsigned char*)"4 Pinfo aj vec", MINI_OVER);
        }
        menuChoice = InputI(0, 35);
        Bdisp_PutDisp_DD();
        switch (menuChoice)
        {
        case 1: printAllConstraintsMenu(pS);break;
        case 2: printObjFunc(pS,TYPE_INPUT);break;
        case 3: if(pS->modelType==TYPE_SOLVE){printObjFunc(pS,TYPE_SOLVE);};break;
        case 4: if(pS->modelType==TYPE_SOLVE){printAjVec(pS);};break;
        default: break;
        }
    }
    return 0;
}

int printVariablesProblemStatement(int contsId, struct problemStatement* pS)
{
    char stroutBasicInfo[128],stroutVariable[128];
    char *uselessSring[128];
    int menuChoice;
    int lengthString;
    lengthString=0;
    menuChoice=INITIALIZE_VALUE_OPT;
    
    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        sprintf(stroutBasicInfo, "Constraint %d. Vars: %d, Exit %d",contsId,pS->nVariables,EXIT_OPT);
        PrintMini(0, 0, (unsigned char*)stroutBasicInfo, MINI_OVER);
        menuChoice=InputI(0,7);
        Bdisp_PutDisp_DD();
        if(menuChoice!=EXIT_OPT && menuChoice<=pS->nVariables)
        {
            Bdisp_AllClr_DDVRAM();
            lengthString += lengthConcatenate(sprintf(stroutVariable+lengthString, "Const %d. %.2f x%d",contsId,pS->constraints[contsId-1][menuChoice-1],menuChoice));

            if(pS->idSlopeVariables[menuChoice-1])
            {
                lengthString += lengthConcatenate(sprintf(stroutVariable+lengthString, " Slope"));
            }
            if(pS->id2fasesVariables[menuChoice-1])
            {
                lengthString += lengthConcatenate(sprintf(stroutVariable+lengthString, " Dual"));
            }
            if(pS->idIntegerVariables[menuChoice-1])
            {
                lengthString += lengthConcatenate(sprintf(stroutVariable+lengthString, " Int"));
            }
            PrintMini(0,0,(unsigned char*) stroutVariable,MINI_OVER);
            PrintMini(0, 7, (unsigned char*)"Press any key to continue", MINI_OVER);
            string_input(0, 14, uselessSring);
            Bdisp_PutDisp_DD();
            memset(stroutVariable,0,128);
            lengthString=0;
        }
    }
    return 0;
}

int printIne(int contsId, struct problemStatement* pS)
{
    char stroutBasicInfo[128];
    char *uselessSring[128];
    Bdisp_AllClr_DDVRAM();
    sprintf(stroutBasicInfo, "Constraint %d. Ine: %s",contsId,getInequalityChar( pS->inequalitySigns[contsId-1]));
    PrintMini(0, 0, (unsigned char*)stroutBasicInfo, MINI_OVER);
    PrintMini(0, 7, (unsigned char*)"Press any key to continue", MINI_OVER);
    string_input(0, 14, uselessSring);
    Bdisp_PutDisp_DD();
    return 0;
}

int printRVal(int contsId, struct problemStatement* pS)
{
    char stroutBasicInfo[128];
    char *uselessSring[128];
    Bdisp_AllClr_DDVRAM();
    sprintf(stroutBasicInfo, "Constraint %d. Rval: %.2f",contsId,pS->rightValues[contsId-1]);
    PrintMini(0, 0, (unsigned char*)stroutBasicInfo, MINI_OVER);
    PrintMini(0, 7, (unsigned char*)"Press any key to continue", MINI_OVER);
    string_input(0, 14, uselessSring);
    Bdisp_PutDisp_DD();
    return 0;
}


int printInfoConstraint(int contsId, struct problemStatement* pS)
{
    char stroutBasicInfo[128];
    int menuChoice;
    menuChoice=INITIALIZE_VALUE_OPT;
    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        sprintf(stroutBasicInfo, "Constraint %d printMenu. %d exit",contsId,EXIT_OPT);
        PrintMini(0, 0, (unsigned char*)stroutBasicInfo, MINI_OVER);
        PrintMini(0, 7, (unsigned char*)"1 Pinfo var", MINI_OVER);
        PrintMini(0, 14, (unsigned char*)"2 Pinfo ine", MINI_OVER);
        PrintMini(0, 21, (unsigned char*)"3 Pinfo RVal", MINI_OVER);
        menuChoice = InputI(0, 28);
        Bdisp_PutDisp_DD();
        if(menuChoice!=EXIT_OPT && menuChoice<=3)
        {
            switch (menuChoice)
            {
            case 1: printVariablesProblemStatement(contsId,pS);break;
            case 2: printIne(contsId,pS);break;
            case 3: printRVal(contsId,pS);break;
            default: break;
            }
        }
    }
    return 0;
}

int printAllConstraintsMenu(struct problemStatement* pS)
{
    char stroutBasicInfo[128];
    int menuChoice;
    menuChoice=INITIALIZE_VALUE_OPT;
    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        sprintf(stroutBasicInfo, "pMenu Consts %d.  %d exit",pS->nConstraints,EXIT_OPT);
        PrintMini(0, 0, (unsigned char*)stroutBasicInfo, MINI_OVER);
        menuChoice = InputI(0, 7);
        Bdisp_PutDisp_DD();
        if(menuChoice!=EXIT_OPT && menuChoice<=pS->nConstraints)
        {
            printInfoConstraint(menuChoice,pS);
        }
    }
    return 0;
}

int getInequalitySign(int nConstraint, int *nVariablesSlope, int * nVariables2fases)
{   
    char strout[128];
    int res;
    Bdisp_AllClr_DDVRAM();
    sprintf(strout, "Constraint: %d, Choose Inequality", nConstraint);
    PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
    sprintf(strout, "Press E: %d, LET %d,  GET %d", 
        INE_EQUAL ,INE_LESS_OR_EQUAL_THAN,INE_GREATER_OR_EQUAL_THAN);
    PrintMini(0, 7, (unsigned char *)strout, MINI_OVER);
    res = InputI(0,14);
    Bdisp_PutDisp_DD();
    switch (res)
    {
        case INE_EQUAL: *nVariables2fases+=1; break;
        case INE_LESS_OR_EQUAL_THAN: *nVariablesSlope+=1; break;
        case INE_GREATER_OR_EQUAL_THAN: *nVariablesSlope+=1;*nVariables2fases+=1; break;
    }

    return res;
}

double getRightValue(int nConstraint)
{   
    char strout[128];
    double res;
    Bdisp_AllClr_DDVRAM();
    sprintf(strout, "Constraint: %d, Right Value", nConstraint);
    PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
    Bdisp_PutDisp_DD();
    res = InputD(0,7);
    return res;
}

double *getInequation(unsigned int nVariables, int nConstraint, int *inequalitySign, double *rightValue, int *nVariablesSlope, int * nVariables2fases)
{
    char strout[128];
    unsigned int i;
    double *constraintValues;
    constraintValues = (double *)malloc(sizeof(double) * nVariables);
    for (i = 0; i < nVariables; i++)
    {
        Bdisp_AllClr_DDVRAM();
        sprintf(strout, "Constraint: %d, variable: %d", nConstraint, (i + 1));
        PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
        Bdisp_PutDisp_DD();
        constraintValues[i] = InputD(0, 7);
    }
    *inequalitySign = getInequalitySign(nConstraint, nVariablesSlope,nVariables2fases);
    *rightValue = getRightValue(nConstraint);
    return constraintValues;
}

struct problemStatement* getProblemInputs()
{
    char strout[128];
    unsigned int nConstraints;
    unsigned int nVariables;
    unsigned int i;
    int nVariablesInteger;
    int problemType;
    double **constraints;
    double *rightValues;
    double *funcObjtValues;
    int *inequalitiesSigns;
    int *idIntegerVariables;
    int *idSlopeVariables;
    int *id2fasesVariables;
    struct problemStatement *pInput;
    nVariablesInteger=0;
    pInput = malloc(sizeof(struct problemStatement));
    Bdisp_AllClr_DDVRAM();
    PrintMini(0, 0, (unsigned char *)"NConstraints?", MINI_OVER);
    Bdisp_PutDisp_DD();
    nConstraints = InputI(0, 7);
    PrintMini(0, 14, (unsigned char *)"NVariables?", MINI_OVER);
    Bdisp_PutDisp_DD();
    nVariables = InputI(0, 21);
    PrintMini(0, 28, (unsigned char *)"Press. 1-LP, 2-ILP, 3-Cuts", MINI_OVER);
    Bdisp_PutDisp_DD();
    problemType = InputI(0, 35);
    constraints = (double **)malloc(sizeof(double) * nConstraints);
    rightValues = (double *) malloc(sizeof(double)*nConstraints);
    inequalitiesSigns = (int *) malloc(sizeof(int)*nConstraints);
    funcObjtValues = calloc(nVariables+1,sizeof(double));
    idIntegerVariables = calloc(nVariables,sizeof(int));
    idSlopeVariables = calloc(nVariables,sizeof(int));
    id2fasesVariables = calloc(nVariables,sizeof(int));
    pInput->nVariablesSlope=0;
    pInput->nVariables2fases=0;
    for(i=0;i<nVariables;i++)
    {
        if(problemType!=TYPE_LP)
        {
            Bdisp_AllClr_DDVRAM();
            sprintf(strout,"Variable x%d",i+1);
            PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
            PrintMini(0, 7, (unsigned char *)"Press 1 if Int var, 0 no", MINI_OVER);
            idIntegerVariables[i] = InputI(0, 14);
            Bdisp_PutDisp_DD();
            memset(strout,0,128);
            if(idIntegerVariables[i]==1)
            {
                nVariablesInteger+=1;
            }
        }
    }
    for (i = 0; i < nConstraints; i++)
    {
        constraints[i] = getInequation(nVariables, i + 1,&inequalitiesSigns[i],&rightValues[i],&pInput->nVariablesSlope,&pInput->nVariables2fases);
    }
    
    Bdisp_AllClr_DDVRAM();
    sprintf(strout,"OBJ. Min: %d, Max %d",FUNC_MINIMIZE,FUNC_MAXIMIZE);
    PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
    pInput->funcObjectivePurpose = InputI(0, 7);
    Bdisp_PutDisp_DD();
    memset(strout,0,128);
    for(i=0;i<nVariables;i++)
    {
        Bdisp_AllClr_DDVRAM();
        sprintf(strout,"Obj value var x%d",i+1);
        PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
        funcObjtValues[i] = InputD(0, 7);
        Bdisp_PutDisp_DD();
        memset(strout,0,128);
    }
    Bdisp_AllClr_DDVRAM();
    sprintf(strout,"Obj value Independent");
    PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
    funcObjtValues[nVariables] = InputD(0, 7);
    Bdisp_PutDisp_DD();
    pInput->constraints=constraints;
    pInput->rightValues=rightValues;
    pInput->inequalitySigns=inequalitiesSigns;
    pInput->nVariables=nVariables;
    pInput->nConstraints=nConstraints;
    pInput->idIntegerVariables=idIntegerVariables;
    pInput->idSlopeVariables=idSlopeVariables;
    pInput->id2fasesVariables=id2fasesVariables;
    pInput->funcObjtValues=funcObjtValues;
    pInput->nVariablesInteger=nVariablesInteger;
    pInput->modelType=TYPE_INPUT;
    pInput->problemType=problemType;
    pInput->is2fasesNeeded=(pInput->nVariables2fases>0)?1:0;
    return pInput;
}

struct execution* selectExecutionMode()
{
    char strout[128];
    struct execution *ex;
    ex = malloc(sizeof(struct execution));
    Bdisp_AllClr_DDVRAM();
    sprintf(strout,"Mode. Full %d, Table %d",MODE_FULL_EXECUTION,MODE_INPUT_TABLE);
    PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
    ex->mode = InputI(0, 7);
    Bdisp_PutDisp_DD();
    ex->nNodes=0;
    return ex;
}

struct problemStatement* convertModel(struct problemStatement* pInput)
{
    int i,j,nextConstraintSlopeSet,nextConstraint2fasesSet;
    struct problemStatement *modelToSolve;
    nextConstraint2fasesSet=0;nextConstraintSlopeSet=0;
    modelToSolve = malloc(sizeof(struct problemStatement));
    modelToSolve->problemType=pInput->problemType;
    modelToSolve->nVariablesSlope=pInput->nVariablesSlope;
    modelToSolve->nVariables2fases=pInput->nVariables2fases;
    modelToSolve->nVariablesInteger=pInput->nVariablesInteger;
    modelToSolve->nConstraints=pInput->nConstraints;
    modelToSolve->funcObjectivePurpose=pInput->funcObjectivePurpose;
    modelToSolve->is2fasesNeeded=pInput->is2fasesNeeded;
    modelToSolve->nVariables=pInput->nVariables+pInput->nVariablesSlope+pInput->nVariables2fases;
    modelToSolve->funcObjtValues=calloc(modelToSolve->nVariables+1,sizeof(double));
    if(modelToSolve->is2fasesNeeded)
    {
        modelToSolve->funcObjtValues2Fases=calloc(modelToSolve->nVariables+1,sizeof(double));
    }
    modelToSolve->idSlopeVariables=calloc(modelToSolve->nVariables,sizeof(int));
    modelToSolve->id2fasesVariables=calloc(modelToSolve->nVariables,sizeof(int));
    modelToSolve->idConstraintToSlopeVar=calloc(modelToSolve->nConstraints,sizeof(int));
    modelToSolve->idConstraintTo2fasesVar=calloc(modelToSolve->nConstraints,sizeof(int));
    memset(modelToSolve->idConstraintTo2fasesVar,-1,modelToSolve->nConstraints);
    memset(modelToSolve->idConstraintToSlopeVar,-1,modelToSolve->nConstraints);
    //SET ID TYPES OF VARIABLES
    for(i=0;i<modelToSolve->nVariables;i++)
    {
        if(i>=pInput->nVariables && i<pInput->nVariables+pInput->nVariablesSlope)
        {
            modelToSolve->idSlopeVariables[i]=1;
        }else if(i>=pInput->nVariables+pInput->nVariablesSlope)
        {
            modelToSolve->id2fasesVariables[i]=1;
        }
        
    }
    for(i=0;i<modelToSolve->nVariables;i++)
    {
        if(modelToSolve->idSlopeVariables[i])
        {
            for(j=nextConstraintSlopeSet;j<modelToSolve->nConstraints;j++)
            {
                if(pInput->inequalitySigns[j]==INE_LESS_OR_EQUAL_THAN ||pInput->inequalitySigns[j]==INE_GREATER_OR_EQUAL_THAN)
                {
                    modelToSolve->idConstraintToSlopeVar[j]=i;
                    nextConstraintSlopeSet=j+1;
                    break;
                }
            }
        }
        if(modelToSolve->id2fasesVariables[i])
        {
            for(j=nextConstraint2fasesSet;j<modelToSolve->nConstraints;j++)
            {
                if(pInput->inequalitySigns[j]==INE_EQUAL ||pInput->inequalitySigns[j]==INE_GREATER_OR_EQUAL_THAN)
                {
                    modelToSolve->idConstraintTo2fasesVar[j]=i;
                    nextConstraint2fasesSet=j+1;
                    break;
                }
            }
        }
    }
    //MODIFY THE CONSTRAINTS and copy right values
    modelToSolve->constraints=(double **)malloc(sizeof(double) * modelToSolve->nConstraints);
    modelToSolve->rightValues=calloc(modelToSolve->nConstraints,sizeof(double));
    modelToSolve->inequalitySigns=calloc(modelToSolve->nConstraints,sizeof(int));
    modelToSolve->idIntegerVariables=calloc(modelToSolve->nVariables,sizeof(int));

    for(i=0;i<modelToSolve->nConstraints;i++)
    {
        modelToSolve->constraints[i]=calloc(modelToSolve->nVariables,sizeof(double));
        for(j=0;j<modelToSolve->nVariables;j++)
        {
            if(j<pInput->nVariables)//Copiar variables de entrada
            {
                modelToSolve->constraints[i][j]=pInput->constraints[i][j];
                modelToSolve->idIntegerVariables[i]=pInput->idIntegerVariables[i];
            }else if(pInput->inequalitySigns[i]==INE_LESS_OR_EQUAL_THAN && j==modelToSolve->idConstraintToSlopeVar[i])
            {
                modelToSolve->constraints[i][j]=1;
            }else if(pInput->inequalitySigns[i]==INE_GREATER_OR_EQUAL_THAN && j==modelToSolve->idConstraintToSlopeVar[i])
            {
                modelToSolve->constraints[i][j]=-1;
            }else if((pInput->inequalitySigns[i]==INE_GREATER_OR_EQUAL_THAN || pInput->inequalitySigns[i]==INE_EQUAL)&& j==modelToSolve->idConstraintTo2fasesVar[i])
            {
                modelToSolve->constraints[i][j]=1;
            }
            //Cualquier otro caso tiene un 0 por calloc
        }
        modelToSolve->rightValues[i]=pInput->rightValues[i];
    }

    //Create new obj funct
    for(i=0;i<modelToSolve->nVariables;i++)
    {
        if(i<pInput->nVariables)
        {
            modelToSolve->funcObjtValues[i]=pInput->funcObjtValues[i];
        }else if(i>=(pInput->nVariables+pInput->nVariablesSlope))
        {
            modelToSolve->funcObjtValues2Fases[i]=1;
        }
    }

    //Create aj vectors
    modelToSolve->ajVector=calloc(modelToSolve->nVariables,sizeof(double));
    for(i=0;i<modelToSolve->nVariables;i++)
    {
        modelToSolve->ajVector[i]=calloc(modelToSolve->nConstraints,sizeof(double));
        for(j=0;j<modelToSolve->nConstraints;j++)
        {
            modelToSolve->ajVector[i][j]=modelToSolve->constraints[j][i];
        }
    }

    modelToSolve->funcObjtValues[modelToSolve->nVariables]=pInput->funcObjtValues[pInput->nVariables];
    modelToSolve->modelType=TYPE_SOLVE;
    return modelToSolve;
}

int AddIn_main(int isAppli, unsigned short OptionNum)
{
    char str[128];
    struct execution *ex;
    ex = selectExecutionMode();
    if(ex->mode==MODE_FULL_EXECUTION)
    {
        ex->initialProblemStatement=getProblemInputs();
        Bdisp_AllClr_DDVRAM();
        sprintf(str,"nSlope %d, N2F %d",ex->initialProblemStatement->nVariablesSlope,ex->initialProblemStatement->nVariables2fases);
        PrintMini(0, 0, (unsigned char *)str, MINI_OVER);    
        Bdisp_PutDisp_DD();
        Sleep(3000);
        printStatementMenu(ex->initialProblemStatement);
        ex->canonicalStatement=convertModel(ex->initialProblemStatement);
        printStatementMenu(ex->canonicalStatement);
    }else if(ex->mode==MODE_INPUT_TABLE)
    {
        //TODO
    }else
    {
        Bdisp_AllClr_DDVRAM();
        PrintMini(0, 0, (unsigned char *)"ERROR", MINI_OVER);
        Bdisp_PutDisp_DD();
        Sleep(3000);
    }
    freeMemoryExecution(ex);
    
}

#pragma section _BR_Size
unsigned long BR_Size;
#pragma section

#pragma section _TOP

int InitializeSystem(int isAppli, unsigned short OptionNum)
{
    return INIT_ADDIN_APPLICATION(isAppli, OptionNum);
}

#pragma section