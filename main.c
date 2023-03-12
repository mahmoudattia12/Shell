#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define max_command_length 100
#define max_arguments 100

FILE *logFile;

char *remove_quotes(char str[])
{
    if (str != NULL)
    {
        int i, j = 0;
        int len = strlen(str);
        for (i = 0; i < len; i++)
        {
            if (str[i] != '"')
            {
                str[j++] = str[i];
            }
        }
        str[j] = '\0'; // null-terminate the new string
    }
    return str;
}

// function to replace the substring in the string by the replacement
void replaceSubstring(char *string, char *substring, char *replacement)
{
    char buffer[100];
    char *ptr;

    if ((ptr = strstr(string, substring)) != NULL)
    {
        strcpy(buffer, string);
        buffer[ptr - string] = '\0';
        strcat(buffer, replacement);
        strcat(buffer, ptr + strlen(substring));
        strcpy(string, buffer);
    }
}
char *parse_input(char str[])
{
    if (str != NULL)
    {
        str = remove_quotes(str);
        for (int i = 0; i < strlen(str); i++)
        {
            char *ptr = strchr(str, '$');
            if (ptr != NULL)
            {
                // use temp to determine the end of the substring
                char temp[255];
                strcpy(temp, ptr);
                char *p1, *p2, *p;
                p1 = strchr(temp + 1, '$');
                p2 = strchr(temp + 1, ' ');
                if ((p1 == NULL) || (p2 != NULL && (p2 < p1)))
                    p = p2;
                else if (p2 == NULL || (p1 != NULL && (p1 < p2)))
                    p = p1;
                // end of the substring
                if (p != NULL)
                    *p = '\0';
                // get the value of the variable from environment
                char *r = getenv(temp + 1);
                if (r == NULL)
                    replaceSubstring(str, temp, "");
                else
                    replaceSubstring(str, temp, r);
            }
        }
    }
    return str;
}


void execute_cd(char *path)
{
    if (path == NULL)
    {
        // cd
        path = getenv("HOME");
        chdir(path);
    }
    else if (path[0] == '~')
    {
        // cd ~
        char *home_dir = getenv("HOME");
        path++;
        strcat(home_dir, path);
        chdir(home_dir);
    }
    else
    {
        chdir(path);
    }
}
void execute_export(char *arg)
{
    // separate the argument to variable and value
    char *var = strtok(arg, "=");
    char *val = remove_quotes(strtok(NULL, "="));
    setenv(var, val, 1);
}

void execute_shell_builtin(char *command, char *arguments[])
{
    if (!strcmp(command, "cd"))
    {
        execute_cd(arguments[1]);
    }
    else if (!strcmp(command, "echo"))
    {
        printf("%s\n", remove_quotes(arguments[1]));
    }
    else if (!strcmp(command, "export"))
    {
        execute_export(arguments[1]);
    }
}

void execute_command(char *command, char *arguments[], int backGround)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // child processe executes the command
        if (execvp(command, arguments) < 0)
        {
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // parent process waits the child either from background or foreground
        int status;
        if (backGround)
        {
            waitpid(pid, &status, WNOHANG);
        }
        else if (waitpid(pid, &status, 0) < 0)
        {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }
    }
}

void setup_environment()
{
    // setup my default directory to home
    char *default_directory = getenv("HOME");
    chdir(default_directory);
}

void on_child_exit()
{
    int wstat;
    pid_t pid;

    // function to reap zombie to clear process table from zombies
    while (1)
    {
        pid = wait3(&wstat, WNOHANG, (struct rusage *)NULL);
        if (pid == 0 || pid == -1)
            break;
    }
    // log here
    logFile = fopen("logFile.txt", "a");
    fprintf(logFile, "Child process was terminated\n");
    fclose(logFile);
}

void shell()
{
    char input[max_command_length];
    char *arguments[max_arguments];
    while (1)
    {
        char cwd[1024]; // buffer to hold the current working directory
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("\033[1;31m%s:~$ \033[0m", cwd);
        }
        else
        {
            printf("\033[1;31m:~$ \033[0m");
        }
        fgets(input, max_command_length, stdin);
        // check background or foreground
        int backGround = 0, num_arguments = 0;
        if (input[strlen(input) - 2] == '&')
        {
            backGround = 1;
            input[strlen(input) - 2] = '\0';
        }
        // parse input and prepare input
        arguments[num_arguments] = parse_input(strtok(input, " \n"));
        if (strcmp(arguments[num_arguments], "echo") == 0 || strcmp(arguments[num_arguments], "export") == 0)
        {
            char *temp = remove_quotes(strtok(NULL, "\0\n"));
            temp[strlen(temp) - 1] = '\0';
            arguments[++num_arguments] = parse_input(temp);
            num_arguments++;
        }
        else
        {
            while (arguments[num_arguments] != NULL && num_arguments < max_arguments - 1)
            {
                num_arguments++;
                arguments[num_arguments] = parse_input(strtok(NULL, " \n"));
            }
            if (arguments[1] != NULL)
            {
                char *ch = strchr(arguments[1], ' ');
                if (ch != NULL)
                    *ch = '\0';
            }
        }
        // to identify the end of the array
        arguments[num_arguments] = NULL;
        if (num_arguments == 0)
        {
            continue;
        }
        if (strcmp(arguments[0], "exit") == 0)
        {
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(arguments[0], "cd") == 0 || strcmp(arguments[0], "echo") == 0 || strcmp(arguments[0], "export") == 0)
        {
            execute_shell_builtin(arguments[0], arguments);
        }

        else
        {
            execute_command(arguments[0], arguments, backGround);
        }
    }
}

int main()
{
    fclose(fopen(logFile, "w"));
    signal(SIGCHLD, on_child_exit); // register child signal
    setup_environment();
    shell();
    return 0;
}
