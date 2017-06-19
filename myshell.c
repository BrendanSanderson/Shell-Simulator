#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

char error_message[30] = "An error has occurred\n";

int mainOut;

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void removeLeadingSpaces(char *input)
{
    int i;
    while(input[0] == ' ' ||input[0] == '\t')
    {
        for (i = 0; i<strlen(input)-1; i++)
        {
            input[i] = input[i+1];
        }
        input[strlen(input)-1] = 0;
        if(strlen(input) == 1 && (input[0] == ' '||input[0] == '\t'))
        {
            input[0] = 0;
        }
    }
}

void runExit(char** args, int argNum, int redirected)
{
    if(argNum > 1 || redirected > 0)
    {
        myPrint(error_message);
        return;
    }
    exit(0);
}

void runCd(char** args, int argNum, int redirected)
{
    if (redirected > 0)
    {
        myPrint(error_message);
        return;
    }
    if(argNum == 1)
    {
        if (!!chdir(getenv("HOME")))
        {
            myPrint(error_message);
        }
    }
    else if (argNum == 2)
    {
        if(!!chdir(args[1]))
        {
            myPrint(error_message);
        }
    }
    else
    {
        myPrint(error_message);
    }
    return;
}

void runPwd(char** args, int argNum, int redirected)
{
    if(argNum > 1 || redirected > 0)
    {
        myPrint(error_message);
        return;
    }
    myPrint(getcwd(NULL,0));
    myPrint("\n");
    return;
}

int runRedirect(char* fileName, int redirected)
{
    int openFlags = O_RDWR | O_CREAT | O_TRUNC | O_EXCL;
    int openPermissions = S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH;
    int redirectFile = open(fileName, openFlags, openPermissions);
    if(redirectFile < 0) {
        if (redirected != 2)
        {
            myPrint(error_message);
        }
        return 0;
    }
    dup2(redirectFile, 1);
    return(redirectFile);
    //open(fileName, O_WRONLY | O_CREAT)
}

char* findRedirectedSymbol(char** args, int* argNum, int* redirected)
{
    int symbolIndex = 0;
    *redirected = 1;
    int i;
    for (i = 0; i < *argNum; i++)
    {
        if (!!(strchr(args[i], '>')))
        {
            if (symbolIndex != 0)
            {
                return 0;
            }
            symbolIndex = i;
        }
    }
    char* afterSymbol = strchr(args[symbolIndex], '>');
    afterSymbol[0] = '\0';
    afterSymbol++;
    if (afterSymbol[0] == '+')
    {
        afterSymbol++;
        *redirected = 2;
    }
    if(strlen(afterSymbol) == 0 && symbolIndex == *argNum-1)
    {
        return 0;
    }
    else if(strlen(afterSymbol)!= 0 && symbolIndex != *argNum-1)
    {
        return 0;
    }

    if(strlen(afterSymbol) == 0)
    {
        afterSymbol = args[*argNum-1];
        args[*argNum-1] = NULL;
        argNum--;
    }
    if(strlen(args[symbolIndex]) == 0)
    {
        *argNum -= 1;
    }
    return afterSymbol;

}

void runCommand(char *i)
{
//    char *input = realloc(input,sizeof(i));
//    strcpy(input, i);
//    printf("%s\n", i);
    removeLeadingSpaces(i);
    if(strlen(i) == 0) 
    {
        return;
    }
    //printf("%s, %lu\n",i, strlen(i));
    int redirected = 0;
    char* redirectedFile = 0;
    int hasRedirectSymbol = 0;
    char *args[514];
    int redFileInd = 0;
    int argNum = 0;
    char *arg;
    char* redTest = i;
    char* oldFileContents = NULL;
    if (strchr(redTest, '>'))
    {
        hasRedirectSymbol = 1;
    }
    arg = strtok(i," \t");
    while(arg != NULL) {
        if (strcmp(arg,">") == 0)
        {
            redirected = 1;
            if (redFileInd != 0)
            {
                myPrint(error_message);
                return;
            }
            redFileInd = argNum + 1;
        }
        else if (strcmp(arg,">+") == 0)
        {
            redirected = 2;
            if (redFileInd != 0)
            {
                myPrint(error_message);
                return;
            }
            redFileInd = argNum + 1;
        }
        args[argNum] = arg;
        arg = strtok(NULL," \t");
        argNum++;
    }
    if(hasRedirectSymbol == 1 && redirected == 0)
    {
        redirectedFile = findRedirectedSymbol(args, &argNum, &redirected);
        if (redirectedFile == 0)
        {
            myPrint(error_message);
            return;
        }
    }
    else if ((redirected && strcmp(args[argNum-1],">") == 0) 
    || (redirected == 2 && strcmp(args[argNum-1],">+") == 0))
    {
        myPrint(error_message);
        return;
    }
    else if (hasRedirectSymbol && (argNum < 2 || ((strcmp(args[argNum-2],">") != 0) && (strcmp(args[argNum-2],">+") != 0))))
    {
        myPrint(error_message);
        return;
    }
    args[argNum] = NULL;
    char* command = args[0];
    if(strcmp(command,"exit") == 0)
    {
        runExit(args, argNum, redirected);
    }
    else if (strcmp(command,"cd") == 0)
    {
        runCd(args, argNum, redirected);
    }
    else if (strcmp(command,"pwd") == 0)
    {
        runPwd(args, argNum, redirected);
    }
    else
    {
        int redirectFile = 0;
        if(redirected >= 1)
        {
            if (redirectedFile == 0)
            {
                redirectedFile = args[redFileInd];
                args[argNum-2] = NULL;
                argNum-= 2;
            }
            redirectFile = runRedirect(redirectedFile, redirected);
            if (redirectFile<=0)
            {
                if (redirected == 1)
                {
                    return;
                }
                else if (redirected == 2)
                {
                    long int oldFileLength;
                    FILE *inputFile = fopen(redirectedFile, "rb");
                    if(!inputFile)
                    {
                        myPrint(error_message);
                        return;
                    }
                    fseek(inputFile, 0, SEEK_END);
                    oldFileLength = ftell(inputFile);
                    rewind(inputFile);
                    oldFileContents = malloc(sizeof(char)*oldFileLength);
                    fread(oldFileContents, sizeof(char), oldFileLength, inputFile);
                    fclose(inputFile);
                    remove(args[redFileInd]);
                    redirected = 3;
                    redirectFile = runRedirect(redirectedFile, 1);
                    if(redirectFile<=0)
                    {
                        return;
                    }          
                }
            }
        }
        pid_t id = fork();
        int waitVal;
        if(id == 0)
        {
            execvp(command, args);
            myPrint(error_message);
            exit(0);
        }
        else
        {
            while(id != wait(&waitVal))
            {
            }
            if (redirectFile)
            {
                if (redirected == 3)
                {
                    myPrint(oldFileContents);
                }
                close(redirectFile);
            }
        }
    }
    return;
}

void parseCommands(char *input, int batch)
{
    char inputCpy[strlen(input)];
    strcpy(inputCpy, input);
    removeLeadingSpaces(input);
    if(strlen(input) == 0) 
    {
        return;
    }
    if (batch)
    {
        myPrint(inputCpy);
        myPrint("\n");
    }
    char *command;
    while((command = strtok(input, ";")) != NULL)
    {
        input += strlen(command)+1;
        runCommand(command);
        while(input[0] == ';')
        {
            input++;
        }
        //removeLeadingSpaces(input);
    }
    return;
}


int main(int argc, char *argv[]) 
{
    char input[514];
    int mainOut = dup(1);
    FILE* batchFile;
    int batch = argc-1;;
    if (argc>2)
    {
        myPrint(error_message);
        exit(0);
    }
    if (batch)
    {
        if(!(batchFile = fopen(argv[1], "r")))
        {
            myPrint(error_message);
            exit(0);
        }
    }
    while (1) {
        dup2(mainOut, 1);
        if (!batch)
        {
            myPrint("myshell> ");
            fgets(input, 514, stdin);
        }
        else
        {
            if(!(fgets(input, 514, batchFile)))
            {
                exit(0);
            }
        }
        //end of file has been read
        //string line is too long
        if (strlen(input)>=513 && input[512] != '\n')
        {
            if(batch)
            {
                myPrint(input);
            }
            while(strlen(input)>=513 && input[512] != '\n')
            {
                if(batch)
                {
                    fgets(input, 514, batchFile);
                    myPrint(input);
                }
                else
                {
                    fgets(input, 514, stdin);
                }
            }
            myPrint(error_message);
        }
        else
        {
            input[strlen(input)-1] = 0;
            parseCommands(input, batch);
        }
    }
}
