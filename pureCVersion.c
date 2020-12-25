#include <stdlib.h>
#include "stdio.h"
#include <string.h>
#include "limits.h"

#define MINI_OVER 1

int printMatrix(int size, float **M)
{
    int i,j;
    for(i=0;i<size;i++)
    {
        for(j=0;j<size;j++)
        {
            printf("%.2f ",M[i][j]);
        }
        printf("\n");
    }
    return 0;
}

int AddIn_main(int isAppli, unsigned short OptionNum);

int INIT_ADDIN_APPLICATION(int x, int y)
{
    return 0;
}

int main(int x, int y)
{
    unsigned short t;
    t=1;
    AddIn_main(0,t);
}

int Bdisp_AllClr_DDVRAM()
{
    return 0;
}

int Bdisp_PutDisp_DD()
{
    return 0;
}
int Sleep(int x)
{
    return 0;
}

int PrintMini(int x,int y,  unsigned char * str, int miniover)
{
    printf("%s\n",str);
    return 0;
}

int string_input(int x, int y,char *string)
{
    fgets(string, sizeof(string), stdin);
}


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
    int* idBasicVariables;//1 si esa variable es Basica. 0 sera NoBasica
    int *idByRowOfBasicVarsInBInv;//ejemplo [0]->2. la row 0 tiene asociala la variable 3
    int isUnbounded;//1 en caso de si, 0 en caso de no

    int idVarIn;
    int idVarOut;
    int indexVarOutInBInvMatrix;

    float **Binv;
    int BinvSize;

    float *ct;
    float *xb;
    float *ctBinv;
    float *zj;
    float *cjMinusZj;
    float *yj;
    float *xbDivYj;

    float zSol ;
};

struct problemStatement
{
    int problemType;
    int modelType;
    int is2fasesNeeded;//1 en caso de si. 0 en caso que no
    int is2fasesActive;//1 en caso de si. 0 en caso que no
    
    //1 array de variables por cada constraint
    //Cada constraint tiene primero todas las variables input, luego todas las slope y finalmente las 2fases
    float **constraints;
    float *rightValues;
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
    float *funcObjtValues;//Ultima posicion, termino independiente sin variables
    float *funcObjtValues2Fases;

    float** ajVector; // guarda un vector por cada variable y contiene su valor en todas las constraints.
    
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
    struct node nodes;
    float *bVectorValues;//Right values;
    float *inputCvectorValues;//Values of cost variable
    float *fases2CvectorValues;
    int nNodes;
    int currentFuncObjectivePurpose;
    int nVariables;

    float** ajVector; // guarda un vector por cada variable y contiene su valor en todas las constraints.
};

struct execution ex;

float InputD(int, int);

int InputI(int, int);

float InputD(int x, int y)
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
    }
    return 0;
}

int freeMemoryIteration(struct iteration *it)
{
    //TODO
    free(it->idBasicVariables);
    free(it->idByRowOfBasicVarsInBInv);
    free(it->xb);
    free(it->ctBinv);
    free(it->zj);
    free(it->cjMinusZj);
    free(it->yj);
    free(it->xbDivYj);
    return 0;
}
int freeMemoryNode(struct node *n)
{
    int i;
    for(i=0; i<n->nIterations;i++)
    {
        freeMemoryIteration(n->its[i]);
    }
    free(n->its);
    return 0;
}

int freeMemoryExecution()
{
    int i;
    if(ex.mode == MODE_FULL_EXECUTION)
    {
        freeMemoryStatement(ex.initialProblemStatement);
        freeMemoryStatement(ex.canonicalStatement);
    }
    for(i=0;i<ex.nVariables;i++)
    {
        // free(ex.ajVector[i]);
    }
    // free(ex.ajVector);
    free(ex.bVectorValues);
    free(ex.inputCvectorValues);
    free(ex.fases2CvectorValues);
    return 0;
}

float* calculateVectorMatrixrMul(int size, float **M, float *V)
{
    float *res;
    int i,j;
    res = calloc(size,sizeof(float));
    for(i=0;i<size;i++)
    {
        for(j=0;j<size;j++)
        {
            res[i]+=V[j]*M[j][i];
        }
    }
    return res;
}

float* calculateMatrixVectorMul(int size, float **M, float *V)
{
    float *res;
    int i,j;
    res = calloc(size,sizeof(float*));
    for(i=0;i<size;i++)
    {
        for(j=0;j<size;j++)
        {
            res[i]+=M[i][j]*V[j];
        }
    }
    return res;
}

float calculateVectorDotProduct(int size, float* A, float *B)
{
    float res;
    int i;
    res=0;
    for(i=0;i<size;i++)
    {
        res+=A[i]*B[i];
    }
    return res;
}

float* getCurrentCvectorValues(struct iteration *it)
{
    float* res;
    res = calloc(ex.nVariables,sizeof(float));
    if(ex.mode==MODE_FULL_EXECUTION && ex.canonicalStatement->is2fasesNeeded && ex.canonicalStatement->is2fasesActive)
    {
        res= ex.fases2CvectorValues;
    }else
    {
        res=ex.inputCvectorValues;
    }
    return res;
}

float* getCurrentCt(struct iteration *it)
{
    float* res;
    int i;
    res = calloc(it->BinvSize,sizeof(float));
    
    for(i=0;i<it->BinvSize;i++)
    {
        if(ex.mode==MODE_FULL_EXECUTION && ex.canonicalStatement->is2fasesNeeded && ex.canonicalStatement->is2fasesActive)
        {
            res[i]= ex.fases2CvectorValues[it->idByRowOfBasicVarsInBInv[i]];
        }else
        {
            res[i]=ex.inputCvectorValues[it->idByRowOfBasicVarsInBInv[i]];
        }
    }
    
    return res;
}

float** calculateNewBinv(struct iteration *oldIt)
{
    float **res;
    int i,j,indexPivot;
    indexPivot=-1;
    res=calloc(oldIt->BinvSize,sizeof(float*));
    for(i=0;i<oldIt->BinvSize;i++)
    {
        res[i]=calloc(oldIt->BinvSize,sizeof(float));
        if(oldIt->idVarOut==oldIt->idByRowOfBasicVarsInBInv[i])
        {
            indexPivot=i;
        }
    }
    //Calculo de la fila pivote
    for(i=0;i<oldIt->BinvSize;i++)
    {
        res[indexPivot][i]=oldIt->Binv[indexPivot][i]/oldIt->yj[indexPivot];
    }

    //Calculo del resto de filas
    for(i=0;i<oldIt->BinvSize;i++)
    {
        for(j=0;j<oldIt->BinvSize && i!=indexPivot;j++)
        {
            res[i][j]=oldIt->Binv[i][j]-(oldIt->yj[i]*res[indexPivot][j]);
        }
    }
    return res;
}

struct iteration* createNewIteration(struct iteration *oldIt)
{
    struct iteration *newIt;
    int i;
    newIt = malloc(sizeof(struct iteration));
    newIt->numIteration=oldIt->numIteration+1;
    newIt->BinvSize=oldIt->BinvSize;
    newIt->Binv=calculateNewBinv(oldIt);
    newIt->idBasicVariables=calloc(ex.nVariables,sizeof(int));
    newIt->idByRowOfBasicVarsInBInv=calloc(newIt->BinvSize,sizeof(int));
    for(i=0;i<ex.nVariables;i++)
    {
        newIt->idBasicVariables[i]=oldIt->idBasicVariables[i];
    }
    for(i=0;i<oldIt->BinvSize;i++)
    {
        if(oldIt->idByRowOfBasicVarsInBInv[i]!=oldIt->idVarOut)
        {
            newIt->idByRowOfBasicVarsInBInv[i]=oldIt->idByRowOfBasicVarsInBInv[i];
        }else
        {
            newIt->idByRowOfBasicVarsInBInv[i]=oldIt->idVarIn;
        }
    }
    newIt->idBasicVariables[oldIt->idVarOut]=0;
    newIt->idBasicVariables[oldIt->idVarIn]=1;
    return newIt;
}

int calculateIteration(struct iteration *it)
{
    int i;
    float * cVectorValues,*ct;
    float bestEntryVarValue,bestExitVarValue;
    cVectorValues=getCurrentCvectorValues(it);
    ct=getCurrentCt(it);
    it->ct=ct;
    it->xb=calculateMatrixVectorMul(it->BinvSize,it->Binv,ex.bVectorValues);
    it->ctBinv=calculateVectorMatrixrMul(it->BinvSize,it->Binv,ct);
    it->zSol=calculateVectorDotProduct(it->BinvSize,ct,it->xb);
    //Calculate values for zj and cj-zj and idVarIn
    it->zj=calloc(ex.nVariables,sizeof(float));
    it->cjMinusZj=calloc(ex.nVariables,sizeof(float));
    it->idVarIn=-1;
    if(ex.currentFuncObjectivePurpose==FUNC_MAXIMIZE)
    {
        bestEntryVarValue=INT_MIN;
    }else
    {
        bestEntryVarValue=INT_MAX;
    }
    
    
    for(i=0;i<ex.nVariables;i++)
    {
        if((!it->idBasicVariables[i] && ex.canonicalStatement->is2fasesActive) ||
            (!it->idBasicVariables[i] && !ex.canonicalStatement->is2fasesActive && !ex.canonicalStatement->id2fasesVariables[i]) )
        {
            it->zj[i]=calculateVectorDotProduct(it->BinvSize,it->ctBinv,ex.ajVector[i]);
            it->cjMinusZj[i]=cVectorValues[i]-it->zj[i];

            if(ex.currentFuncObjectivePurpose==FUNC_MINIMIZE && it->cjMinusZj[i]<0 && it->cjMinusZj[i]<bestEntryVarValue )
            {
                bestEntryVarValue=it->cjMinusZj[i];
                it->idVarIn=i;
            }else if(ex.currentFuncObjectivePurpose==FUNC_MAXIMIZE && it->cjMinusZj[i]>0 && it->cjMinusZj[i]>bestEntryVarValue )
            {
                bestEntryVarValue=it->cjMinusZj[i];
                it->idVarIn=i;
            }
        }
    }
    if(it->idVarIn!=-1)//Check optimum criteria
    {
        //Calculate yj
        it->yj=calculateMatrixVectorMul(it->BinvSize,it->Binv,ex.ajVector[it->idVarIn]);
        it->xbDivYj= calloc(it->BinvSize,sizeof(float));
        bestExitVarValue=INT_MAX;
        it->indexVarOutInBInvMatrix=-1;
        it->isUnbounded=1;
        for(i=0;i<it->BinvSize;i++)
        {
            if(it->yj[i]!=0)
            {
                it->xbDivYj[i]=it->xb[i]/it->yj[i];
                if(it->xbDivYj[i]>0 && it->xbDivYj[i]<bestExitVarValue)
                {
                    it->indexVarOutInBInvMatrix=i;
                    bestExitVarValue=it->xbDivYj[i];
                    it->isUnbounded=0;
                }else if(it->xbDivYj[i]<0)
                {
                    it->xbDivYj[i]=NO_EVAL_VALUE;
                }
                
            }else
            {
                it->xbDivYj[i]=NO_EVAL_VALUE;
            }
            
        }
        if(!it->isUnbounded)
        {
            it->idVarOut=it->idByRowOfBasicVarsInBInv[it->indexVarOutInBInvMatrix];
        }
        
    }
    
}

struct iteration* modelToIteration(struct problemStatement *pS)
{
    struct iteration *it;
    int i;
    it = malloc(sizeof(struct iteration));
    it->numIteration=0;
    //Matriz B-1
    it->BinvSize=pS->nConstraints;
    it->Binv=calloc(it->BinvSize,sizeof(float));
    for(i=0;i<it->BinvSize;i++)
    {
        it->Binv[i]=calloc(it->BinvSize,sizeof(float));
        it->Binv[i][i]=1;
    }
    it->idBasicVariables=calloc(pS->nVariables,sizeof(int));
    it->idByRowOfBasicVarsInBInv=calloc(it->BinvSize,sizeof(int));
    for(i=0;i<it->BinvSize;i++)
    {
        if(pS->idConstraintTo2fasesVar[i]>0)
        {
            it->idByRowOfBasicVarsInBInv[i]=pS->idConstraintTo2fasesVar[i];
            it->idBasicVariables[pS->idConstraintTo2fasesVar[i]]=1;
        }else if(pS->idConstraintToSlopeVar[i]>0)
        {
            it->idByRowOfBasicVarsInBInv[i]=pS->idConstraintToSlopeVar[i];
            it->idBasicVariables[pS->idConstraintToSlopeVar[i]]=1;
        }
    }
    return it;
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

int printResVariables()
{
    char stroutTop[128],stroutBasicInfo[128],stroutBasicInfo2[128],stroutVariable[128];
    char *uselessSring[128];
    int menuChoice,i,idBasicVar;
    menuChoice=INITIALIZE_VALUE_OPT;
    sprintf(stroutTop,"Var res, printMenu. Exit %d", EXIT_OPT);
    sprintf(stroutBasicInfo, "Nvars: %d, NVB: %d",ex.nVariables,ex.nodes.its[ex.nodes.nIterations]->BinvSize);
    sprintf(stroutBasicInfo2, "Select a var:");

    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        
        PrintMini(0,0,  (unsigned char *)stroutTop, MINI_OVER);
        PrintMini(0,7,  (unsigned char *)stroutBasicInfo, MINI_OVER);
        PrintMini(0,14,  (unsigned char *)stroutBasicInfo2, MINI_OVER);
        menuChoice = InputI(0, 21);
        Bdisp_PutDisp_DD();
        if(menuChoice<=ex.nVariables)
        {
            if(ex.nodes.its[ex.nodes.nIterations]->idBasicVariables[menuChoice-1])
            {
                for(i=0;i<ex.nodes.its[ex.nodes.nIterations]->BinvSize;i++)
                {
                    if(ex.nodes.its[ex.nodes.nIterations]->idByRowOfBasicVarsInBInv[i]==menuChoice-1)
                    {
                        idBasicVar=i;
                    }
                }
                sprintf(stroutVariable, "x%d: %.2f",menuChoice,ex.nodes.its[ex.nodes.nIterations]->xb[idBasicVar]);
            }else
            {
                sprintf(stroutVariable, "x%d: %.2f",menuChoice,0);
            }
            PrintMini(0,28,(unsigned char*) stroutVariable,MINI_OVER);
        }else
        {
            PrintMini(0,28,  (unsigned char *)"No var found", MINI_OVER);
        }
        PrintMini(0, 35, (unsigned char*)"Press any key to continue", MINI_OVER);
        string_input(0, 42, uselessSring);
        Bdisp_PutDisp_DD();
        memset(stroutVariable,0,128);
        
    }
    return 0;
}

int printIts()
{
    return 0;
}

int printSolMenu()
{
    char stroutTop[128],stroutBasicInfo[128],stroutBasicInfo2[128];
    int menuChoice;
    menuChoice=INITIALIZE_VALUE_OPT;
    sprintf(stroutTop,"Problem solution, printMenu. Exit %d", EXIT_OPT);
    sprintf(stroutBasicInfo, "Nits: %d, ZSol: %.2f",ex.nodes.nIterations,ex.nodes.its[ex.nodes.nIterations]->zSol);

    while(menuChoice!=EXIT_OPT)
    {
        Bdisp_AllClr_DDVRAM();
        
        PrintMini(0,0,  (unsigned char *)stroutTop, MINI_OVER);
        PrintMini(0,7,  (unsigned char *)stroutBasicInfo, MINI_OVER);
        PrintMini(0, 14, (unsigned char*)"1 Pinfo res vars", MINI_OVER);
        PrintMini(0, 21, (unsigned char*)"2 Pinfo its", MINI_OVER);
        menuChoice = InputI(0, 28);
        Bdisp_PutDisp_DD();
        switch (menuChoice)
        {
            case 1: printResVariables();break;
            case 2: printIts();break;
            default: break;
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

float getRightValue(int nConstraint)
{   
    char strout[128];
    float res;
    Bdisp_AllClr_DDVRAM();
    sprintf(strout, "Constraint: %d, Right Value", nConstraint);
    PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
    Bdisp_PutDisp_DD();
    res = InputD(0,7);
    return res;
}

float *getInequation(unsigned int nVariables, int nConstraint, int *inequalitySign, float *rightValue, int *nVariablesSlope, int * nVariables2fases)
{
    char strout[128];
    unsigned int i;
    float *constraintValues;
    constraintValues = (float *)malloc(sizeof(float) * nVariables);
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
    float **constraints;
    float *rightValues;
    float *funcObjtValues;
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
    constraints = (float **)malloc(sizeof(float) * nConstraints);
    rightValues=calloc(nConstraints,sizeof(float));
    inequalitiesSigns = (int *) malloc(sizeof(int)*nConstraints);
    funcObjtValues = calloc(nVariables+1,sizeof(float));
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

int selectExecutionMode()
{
    char strout[128];
    Bdisp_AllClr_DDVRAM();
    sprintf(strout,"Mode. Full %d, Table %d",MODE_FULL_EXECUTION,MODE_INPUT_TABLE);
    PrintMini(0, 0, (unsigned char *)strout, MINI_OVER);
    ex.mode = InputI(0, 7);
    Bdisp_PutDisp_DD();
    ex.nNodes=0;
    return 0;
}

int convertModel()
{
    int i,j,nextConstraintSlopeSet,nextConstraint2fasesSet;
    struct problemStatement *modelToSolve;
    nextConstraint2fasesSet=0;nextConstraintSlopeSet=0;
    modelToSolve = malloc(sizeof(struct problemStatement));
    modelToSolve->problemType=ex.initialProblemStatement->problemType;
    modelToSolve->nVariablesSlope=ex.initialProblemStatement->nVariablesSlope;
    modelToSolve->nVariables2fases=ex.initialProblemStatement->nVariables2fases;
    modelToSolve->nVariablesInteger=ex.initialProblemStatement->nVariablesInteger;
    modelToSolve->nConstraints=ex.initialProblemStatement->nConstraints;
    modelToSolve->funcObjectivePurpose=ex.initialProblemStatement->funcObjectivePurpose;
    modelToSolve->is2fasesNeeded=ex.initialProblemStatement->is2fasesNeeded;
    modelToSolve->nVariables=ex.initialProblemStatement->nVariables+ex.initialProblemStatement->nVariablesSlope+ex.initialProblemStatement->nVariables2fases;
    modelToSolve->funcObjtValues=calloc(modelToSolve->nVariables+1,sizeof(float));
    if(modelToSolve->is2fasesNeeded)
    {
        modelToSolve->funcObjtValues2Fases=calloc(modelToSolve->nVariables+1,sizeof(float));
    }
    modelToSolve->idSlopeVariables=calloc(modelToSolve->nVariables,sizeof(int));
    modelToSolve->id2fasesVariables=calloc(modelToSolve->nVariables,sizeof(int));
    modelToSolve->idConstraintToSlopeVar=calloc(modelToSolve->nConstraints,sizeof(int));
    modelToSolve->idConstraintTo2fasesVar=calloc(modelToSolve->nConstraints,sizeof(int));
    memset(modelToSolve->idConstraintTo2fasesVar,-1,sizeof(modelToSolve->idConstraintTo2fasesVar));
    memset(modelToSolve->idConstraintToSlopeVar,-1,sizeof(modelToSolve->idConstraintToSlopeVar));
    //SET ID TYPES OF VARIABLES
    for(i=0;i<modelToSolve->nVariables;i++)
    {
        if(i>=ex.initialProblemStatement->nVariables && i<ex.initialProblemStatement->nVariables+ex.initialProblemStatement->nVariablesSlope)
        {
            modelToSolve->idSlopeVariables[i]=1;
        }else if(i>=ex.initialProblemStatement->nVariables+ex.initialProblemStatement->nVariablesSlope)
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
                if(ex.initialProblemStatement->inequalitySigns[j]==INE_LESS_OR_EQUAL_THAN ||ex.initialProblemStatement->inequalitySigns[j]==INE_GREATER_OR_EQUAL_THAN)
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
                if(ex.initialProblemStatement->inequalitySigns[j]==INE_EQUAL ||ex.initialProblemStatement->inequalitySigns[j]==INE_GREATER_OR_EQUAL_THAN)
                {
                    modelToSolve->idConstraintTo2fasesVar[j]=i;
                    nextConstraint2fasesSet=j+1;
                    break;
                }
            }
        }
    }
    //MODIFY THE CONSTRAINTS and copy right values
    modelToSolve->constraints=(float **)malloc(sizeof(float) * modelToSolve->nConstraints);
    modelToSolve->rightValues=calloc(modelToSolve->nConstraints,sizeof(float));
    modelToSolve->inequalitySigns=calloc(modelToSolve->nConstraints,sizeof(int));
    modelToSolve->idIntegerVariables=calloc(modelToSolve->nVariables,sizeof(int));

    for(i=0;i<modelToSolve->nConstraints;i++)
    {
        modelToSolve->constraints[i]=calloc(modelToSolve->nVariables,sizeof(float));
        for(j=0;j<modelToSolve->nVariables;j++)
        {
            if(j<ex.initialProblemStatement->nVariables)//Copiar variables de entrada
            {
                modelToSolve->constraints[i][j]=ex.initialProblemStatement->constraints[i][j];
                modelToSolve->idIntegerVariables[i]=ex.initialProblemStatement->idIntegerVariables[i];
            }else if(ex.initialProblemStatement->inequalitySigns[i]==INE_LESS_OR_EQUAL_THAN && j==modelToSolve->idConstraintToSlopeVar[i])
            {
                modelToSolve->constraints[i][j]=1;
            }else if(ex.initialProblemStatement->inequalitySigns[i]==INE_GREATER_OR_EQUAL_THAN && j==modelToSolve->idConstraintToSlopeVar[i])
            {
                modelToSolve->constraints[i][j]=-1;
            }else if((ex.initialProblemStatement->inequalitySigns[i]==INE_GREATER_OR_EQUAL_THAN || ex.initialProblemStatement->inequalitySigns[i]==INE_EQUAL)&& j==modelToSolve->idConstraintTo2fasesVar[i])
            {
                modelToSolve->constraints[i][j]=1;
            }
            //Cualquier otro caso tiene un 0 por calloc
        }
        modelToSolve->rightValues[i]=ex.initialProblemStatement->rightValues[i];
    }

    //Create new obj funct
    for(i=0;i<modelToSolve->nVariables;i++)
    {
        if(i<ex.initialProblemStatement->nVariables)
        {
            modelToSolve->funcObjtValues[i]=ex.initialProblemStatement->funcObjtValues[i];
        }else if(i>=(ex.initialProblemStatement->nVariables+ex.initialProblemStatement->nVariablesSlope))
        {
            modelToSolve->funcObjtValues2Fases[i]=1;
        }
    }

    //Create aj vectors
    ex.ajVector=calloc(modelToSolve->nVariables,sizeof(float*));
    for(i=0;i<modelToSolve->nVariables;i++)
    {
        ex.ajVector[i]=calloc(modelToSolve->nConstraints,sizeof(float));
        for(j=0;j<modelToSolve->nConstraints;j++)
        {
            ex.ajVector[i][j]=modelToSolve->constraints[j][i];
        }
    }
    modelToSolve->ajVector=ex.ajVector;
    modelToSolve->funcObjtValues[modelToSolve->nVariables]=ex.initialProblemStatement->funcObjtValues[ex.initialProblemStatement->nVariables];
    modelToSolve->modelType=TYPE_SOLVE;
    ex.canonicalStatement=modelToSolve;
    ex.nVariables=modelToSolve->nVariables;
    return 0;
}

struct problemStatement* createProblemStatementToDebug2Fases()
{
    struct problemStatement *res;
    int nConstraints;
    int nVariables;
    nConstraints=3;
    nVariables=2;
    res= malloc(sizeof(struct problemStatement));
    res->modelType=TYPE_INPUT;
    res->nConstraints=nConstraints;
    res->nVariables=nVariables;
    res->nVariables2fases=1;
    res ->nVariablesSlope=2;
    res->problemType=TYPE_LP;
    res -> funcObjtValues = calloc(nVariables+1,sizeof(float));
    res->idIntegerVariables=calloc(nVariables,sizeof(int));
    res->idSlopeVariables= calloc(nVariables,sizeof(int));
    res->id2fasesVariables=calloc(nVariables,sizeof(int));
    res->inequalitySigns=calloc(nConstraints,sizeof(int));
    res->inequalitySigns[0]=INE_LESS_OR_EQUAL_THAN;
    res->inequalitySigns[1]=INE_LESS_OR_EQUAL_THAN;
    res->inequalitySigns[2]=INE_EQUAL;
    res->funcObjectivePurpose=FUNC_MAXIMIZE;
    res->funcObjtValues[0]=3;
    res->funcObjtValues[1]=5;
    res->funcObjtValues[2]=0;
    res->constraints=calloc(nConstraints,sizeof(float));
    res->constraints[0]=calloc(nVariables,sizeof(float));
    res->constraints[1]=calloc(nVariables,sizeof(float));
    res->constraints[2]=calloc(nVariables,sizeof(float));
    res->constraints[0][0]=1;
    res->constraints[0][1]=0;
    res->constraints[1][0]=0;
    res->constraints[1][1]=2;
    res->constraints[2][0]=3;
    res->constraints[2][1]=2;
    res->rightValues=calloc(nConstraints,sizeof(float));
    res->rightValues[0]=4;
    res->rightValues[1]=12;
    res->rightValues[2]=18;
    res->nVariables2fases=1;
    res->is2fasesNeeded=1;

    return res;
}

struct problemStatement* createProblemStatementToDebug()
{
    struct problemStatement *res;
    int nConstraints;
    int nVariables;
    nConstraints=3;
    nVariables=2;
    res= malloc(sizeof(struct problemStatement));
    res->modelType=TYPE_INPUT;
    res->nConstraints=nConstraints;
    res->nVariables=nVariables;
    res->nVariables2fases=0;
    res ->nVariablesSlope=3;
    res->problemType=TYPE_LP;
    res -> funcObjtValues = calloc(nVariables+1,sizeof(float));
    res->idIntegerVariables=calloc(nVariables,sizeof(int));
    res->idSlopeVariables= calloc(nVariables,sizeof(int));
    res->id2fasesVariables=calloc(nVariables,sizeof(int));
    res->inequalitySigns=calloc(nConstraints,sizeof(int));
    res->funcObjectivePurpose=FUNC_MINIMIZE;
    res->funcObjtValues[0]=-4;
    res->funcObjtValues[1]=-6;
    res->funcObjtValues[2]=0;
    res->constraints=calloc(nConstraints,sizeof(float));
    res->constraints[0]=calloc(nVariables,sizeof(float));
    res->constraints[1]=calloc(nVariables,sizeof(float));
    res->constraints[2]=calloc(nVariables,sizeof(float));
    res->constraints[0][0]=-1;
    res->constraints[0][1]=1;
    res->constraints[1][0]=1;
    res->constraints[1][1]=1;
    res->constraints[2][0]=2;
    res->constraints[2][1]=5;
    res->inequalitySigns[0]=INE_LESS_OR_EQUAL_THAN;
    res->inequalitySigns[1]=INE_LESS_OR_EQUAL_THAN;
    res->inequalitySigns[2]=INE_LESS_OR_EQUAL_THAN;
    res->rightValues=calloc(nConstraints,sizeof(float));
    res->rightValues[0]=11;
    res->rightValues[1]=27;
    res->rightValues[2]=90;
    res->nVariables2fases=0;
    res->is2fasesNeeded=0;

    return res;
}

struct iteration *convertToSecondPhase(int nodeId, int firsItId)
{
    int i,j;
    struct iteration *it,*originalIt;
    originalIt=ex.nodes.its[firsItId];
    it = malloc(sizeof(struct iteration));
    it->BinvSize=originalIt->BinvSize;
    it->Binv=calloc(it->BinvSize,sizeof(float));
    for(i=0;i<it->BinvSize;i++)
    {
        it->Binv[i]=calloc(it->BinvSize,sizeof(float));
        for(j=0;j<it->BinvSize;j++)
        {
            it->Binv[i][j]=originalIt->Binv[i][j];
        }
        
    }
    it->idBasicVariables=calloc(ex.nVariables,sizeof(int));
    it->idByRowOfBasicVarsInBInv=calloc(it->BinvSize,sizeof(int));
    for(i=0;i<it->BinvSize;i++)
    {
        it->idByRowOfBasicVarsInBInv[i]=originalIt->idByRowOfBasicVarsInBInv[i];
    }
    for(i=0;i<ex.nVariables;i++)
    {
        it->idBasicVariables[i]=originalIt->idBasicVariables[i];
    }
    return it;
}

int solveSimplexLPOneFase(int nodeId, int firsItId)
{
    int itId, nId;
    float zSol;
    itId=firsItId;
    ex.nodes.its[itId]->numIteration=itId;
    while(ex.nodes.its[itId]->idVarIn!=-1 && ex.nodes.its[itId]->idVarOut!=-1)
    {
        calculateIteration(ex.nodes.its[itId]);
        if(ex.nodes.its[itId]->idVarIn!=-1 && ex.nodes.its[itId]->idVarOut!=-1){
            ex.nodes.its[itId+1]=createNewIteration(ex.nodes.its[itId]);
            zSol= ex.nodes.its[itId]->zSol;
	        itId++;
        }
            
    }
    return itId;
}

int solveSimplexLP(int nodeId)
{
    int itId,lastItId;
    char strSol[128];
    itId=0;
    ex.nodes.id=nodeId;
    ex.nodes.its=malloc(10*sizeof(struct iteration*));
    ex.nodes.its[0]=modelToIteration(ex.canonicalStatement);
    if(ex.canonicalStatement->is2fasesActive)
    {
        lastItId=solveSimplexLPOneFase(nodeId, 0);
        ex.currentFuncObjectivePurpose=ex.canonicalStatement->funcObjectivePurpose;//CAMBIAR
        //NO SE SI HACE FALTA MAS
        ex.canonicalStatement->is2fasesActive=0;
        ex.nodes.its[lastItId+1]=convertToSecondPhase(nodeId, lastItId);
        lastItId=solveSimplexLPOneFase(nodeId, lastItId+1);
    }else
    {
        lastItId=solveSimplexLPOneFase(nodeId, 0);
    }
    ex.nodes.nIterations=lastItId;

    return 0;
}


int initializeExecution()
{
    ex.bVectorValues=ex.canonicalStatement->rightValues;
    if(ex.canonicalStatement->is2fasesNeeded)
    {
        ex.canonicalStatement->is2fasesActive=1;
        ex.currentFuncObjectivePurpose=FUNC_MINIMIZE;
    }else
    {
        ex.currentFuncObjectivePurpose=ex.canonicalStatement->funcObjectivePurpose;
    }
    ex.inputCvectorValues=ex.canonicalStatement->funcObjtValues;
    ex.fases2CvectorValues=ex.canonicalStatement->funcObjtValues2Fases;
    return 0;
}

int AddIn_main(int isAppli, unsigned short OptionNum)
{
    char str[128];
    // selectExecutionMode();
    if(ex.mode==MODE_FULL_EXECUTION)
    {
        ex.initialProblemStatement=createProblemStatementToDebug2Fases();
        // ex.initialProblemStatement=getProblemInputs();
        // Bdisp_AllClr_DDVRAM();
        // sprintf(str,"nSlope %d, N2F %d",ex.initialProblemStatement->nVariablesSlope,ex.initialProblemStatement->nVariables2fases);
        // PrintMini(0, 0, (unsigned char *)str, MINI_OVER);    
        // Bdisp_PutDisp_DD();
        // Sleep(3000);
        // printStatementMenu(ex.initialProblemStatement);
        convertModel(ex);
        printStatementMenu(ex.canonicalStatement);
        initializeExecution();
        solveSimplexLP(0);
        printSolMenu();

    }else if(ex.mode==MODE_INPUT_TABLE)
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