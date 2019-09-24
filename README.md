# my_bash

- Use `execvp`, `fork` and `waitpid` to implement command invocation

## Usage:

- compile and run: `./run.py -br`
  - About `run.py`：you can get it from Pypi Library [Qpro](https://pypi.org/project/Qpro/) and search usage at [here](https://github.com/Rhythmicc/ACM-Template).

| command | means |
|:---|:---|
|`osh>exit`| exit |
| `osh>!!` | view all command history |
|`osh>! <indx>`| view the \<indx\>th command in history|

- Not recommended usage:

  - `osh>command &`: this command will make command not be added into history list and break prompt displaying.
   

## Outstanding problems:

- It's can't run `* &` very well.
  
  - I have tried several methods like creating new process to manage background process to solve this problem, but failed.
  