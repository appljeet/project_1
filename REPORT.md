## Running Simple Commands
Because the provided code used *system()* to execute commands (which is too
simplistic for an actual shell), we had to redesign the provided code to
instead utilize *fork() exec()* and *wait()*. The program is forked, the
parent waits for the child, and retrieves a return value via an argument
passed into wait. This argument is used to print whether or not the
execution of a command was successful.

## Implementing arguments
Arguments were added to the existing functionality by first creating a struct
called currentInput that has variables command and arguments with the first
index of arguments being the command. There is also a function *split* to go
along with struct. The function takes a char array as an input and splits it up
into the command and its arguments. It does by using strtok and a space as a
delimiter. Once the input from the user has been split and stored as a struct,
it is then used to call *execv* which takes the command itself and its
arguments as parameters.

## Builtin Commands: cd and pwd
For the *cd* command, we used the function *chdir* and take the second argument
from user input to go to the target directory. If failed to go to the directory
For the *pwd* command, we used *getcwd* to get the current working directory.

## Output/Input Redirection
After receive user input, we first check if the input contains '>' or '<'. If
true, we only take the input before the symbols for executing. We take the
argument after the symbols to as a file name. The file is open with O_WRONLY, 
O_CREATE, and O\_TRUNC. Then we used the function *dup2* to connect *stdout* 
with the file.

## Piping
Piping was a two step operation. The first was splitting up the input into individual processes and the second was to actually create a pipe between these different processes. The user input was split up using *strtok* with "|" as a delimiter. Then, an array of struct currentInput was created with each index representing a process. The *split* function was called on each of these processes to separate the command from its arguments. Then, two commands at a time are supplied to the *pipeline* function.

## Extra Features: Directory Stack
We build a stack to store the directory that is pushed. 
For *pushd* command, it goes to the target directory by using the function 
*chdir* and gets the current path by using *getcwd* then adds the new current 
path into the stack.
For *popd* command, it takes out the last path that was put into stack, then 
using *chdir("..")* to go to the previous directory.
For *dirs* command, we used a *for* loop to prints out all the path that was
stored in the stack.