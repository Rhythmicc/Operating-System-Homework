# my_bash

- Use `execvp`, `fork` and `waitpid` to implement command invocation

## Outstanding problems:

- It's can't run `* &` very well.
  
  - I have tried several methods like creating new process to manage background process to solve this problem, but failed.