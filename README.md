---
title: Writing Your Own Unix Shell
date: 2023-07-09 16:26:25
tags: [csapp,lab]
categories: [csapp]
description: 

---

## Introduce



这个任务的目的是让你更加熟悉进程控制和信号传递的概念。你将通过编写一个简单的Unix shell程序来实现作业控制。



<!--more-->



## 使用说明



1. 输入命令 `tar xvf shlab-handout.tar` 解压 tarfile文件。

   ```shell
   tar xvf shlab-handout.tar
   ```

2. 键入 make 命令以编译和链接一些测试例程。

3. 查看 tsh.c 文件，它包含一个简单 Unix shell 的函数框架。为了帮助你入门，我们已经实现了一些不那么有趣的函数。你的任务就是完成下列函数。为了让你进行一次合理性检查，我们列出了我们参考解决方案中每个函数的大致代码行数（包括大量注释）。

   1. eval: 解析和解释命令行的 Main 例程. [70 行]
   2. builtin cmd: 识别和解释内置命令: quit，fg，bg，and jobs [25 行]
   3. do_bgfg: Implements the bg and fg built-in commands. [50 lines]
   4. waitfg: 等待前台作业完成。[20 行]
   5. sigchld handler: 捕捉 SIGCHILD 信号。[80 行]
   6. sigint handler: 捕获SIGINT（ctrl-c）信号。[15行]
   7. sigtstp handler: 捕捉 SIGTSTP (ctrl-z)信号。[15 行]

4. 每次修改 tsh.c 文件时，输入 make 重新编译它。要运行 shell，在命令行中输入 `./tsh`:

   ```shell
   unix> ./tsh
   ```

   

## tsh 规范

您的 tsh shell 应该具有以下特性:

- 提示符应该是字符串“ tsh >”。
- 用户输入的命令行应该包含一个名称，零个或多个参数，所有参数都由一个或多个空格分隔。 如果 name 是一个内置命令，那么 tsh 应该立即处理它并等待下一个命令行。否则，tsh 应该 假定 name 是一个可执行文件的路径，它在一个初始子进程的上下文中加载并运行该文件(在这个上下文中，术语 job 指的是这个初始子进程)。
- tsh 不需要支持管道(|)或 i/o 重定向(< 和 >)。
- 键入 ctrl-c (ctrl-z)应该导致将 SIGINT (SIGTSTP)信号发送到前台作业，以及该作业的任何后代(例如，它分叉的任何子进程)。如果没有前台作业，那么该信号应该没有作用。
- 如果命令行以 & 结尾，那么 tsh 应该在后台运行作业。否则，它应该在前台运行作业。
- 每个作业可以由进程 ID (PID)或作业 ID (JID)标识，后者是由 tsh 分配的一个正整数。Jid 应该 在命令行中用前缀“%”表示。例如，“% 5”表示 JID 5，“5”表示 PID 5。(我们已经为您提供了 操纵工作列表所需的所有例程）
- tsh 应该支持以下内置命令:
  - Quit 命令终止 shell。
  - Jobs 命令列出了所有的后台工作。
  - Bg < job > 命令通过发送 SIGCONT 信号重启 < job > ，然后在后台运行。参数 < job > 可以是 PID 或 JID。
  - fg < job > 命令通过发送 SIGCONT 信号重新启动 < job > ，然后在前台运行它。参数 < job > 可以是 PID 或 JID。
  - tsh 应该回收所有的僵尸子进程。如果有任何作业因接收到未捕获的信号而终止，那么 tsh 应该识别此事件，并打印一条包含作业的 PID 和触发信号描述的消息。



## 检查你的程序



我们提供了一些工具来帮助你检查你的程序。



**参考解决方案。**

Linux 可执行文件 tshref 是该 Shell 的参考解决方案。运行此程序以解答您对您的 Shell 应如何行为的任何疑问。您的 Shell 应生成与参考解决方案完全相同的输出（当然，除了进程ID，因为它们在每次运行时会变化）。



**Shell 驱动程序**

sdriver.pl程序将shell作为子进程执行，根据跟踪文件的指示发送命令和信号，并捕获并显示shell的输出。



**使用 -h 参数查找 sdriver.pl 的用法:**

```shell
-> ./sdriver.pl -h

Usage: ./sdriver.pl [-hv] -t <trace> -s <shellprog> -a <args>
Options:
  -h            Print this message
  -v            Be more verbose
  -t <trace>    Trace file
  -s <shell>    Shell program to test
  -a <args>     Shell arguments
  -g            Generate output for autograder
```



我们还提供了16个跟踪文件（trace{01-16}.txt），您将与Shell驱动程序一起使用这些文件来测试您的Shell的正确性。较低编号的跟踪文件执行非常简单的测试，而较高编号的测试执行更复杂的测试。



您可以通过输入以下命令，在您的Shell上使用trace文件trace01.txt来运行Shell驱动程序：

\- a“-p”参数告诉你的 shell 忽略提示信息

```shell
unix> ./sdriver.pl -t trace01.txt -s ./tsh -a "-p" 
```

或者

```shell
unix> make test01
```

类似地，您可以通过输入以下命令来运行跟踪驱动程序，并将其应用于参考Shell，以将结果与参考Shell进行比较：

```shell
./sdriver.pl -t trace01.txt -s ./tshref -a "-p"
```

或者

```shell
make rtest01
```

tshref.out提供了参考解决方案在所有跟踪文件上的输出。这可能比您手动在所有跟踪文件上运行Shell驱动程序更方便。

跟踪文件的好处在于，它们生成的输出与您以交互方式运行Shell时获得的输出相同（除了一个标识跟踪的初始注释）。

例如：

```shell
bass> make test15
./sdriver.pl -t trace15.txt -s ./tsh -a "-p" #
# trace15.txt - Putting it all together
#
tsh> ./bogus
./bogus: Command not found.
tsh> ./myspin 10
Job (9721) terminated by signal 2
tsh> ./myspin 3 &
[1] (9723) ./myspin 3 &
tsh> ./myspin 4 &
5
[2] (9725) ./myspin 4 &
tsh> jobs
[1] (9723) Running ./myspin 3 & [2] (9725) Running ./myspin 4 & tsh> fg %1
Job [1] (9723) stopped by signal 20 tsh> jobs
[1] (9723) Stopped
[2] (9725) Running
tsh> bg %3
%3: No such job
tsh> bg %1
[1] (9723) ./myspin 3 &
tsh> jobs
[1] (9723)
[2] (9725)
tsh> fg %1
tsh> quit
bass>
```

## 提示：



- 仔细阅读教材第8章（异常控制流）的每一个词。

- 使用跟踪文件指导您的Shell的开发。从trace01.txt开始，确保您的Shell产生与参考Shell完全相同的输出。然后继续处理trace02.txt，以此类推。

- `waitpid`、`kill`、`fork`、`execve`、`setpgid`和`sigprocmask`函数将非常有用。`waitpid`的`WUNTRACED`和`WNOHANG`选项也很有用。

- 在实现信号处理程序时，请确保使用`kill`函数的参数中的`"-pid"`而不是`"pid"`，向整个前台进程组发送`SIGINT`和`SIGTSTP`信号。`sdriver.pl`程序会检查此错误。

- 任务的一个棘手部分是决定`waitfg`和`sigchld`处理函数之间的工作分配。我们推荐以下方法：

  - 在`waitfg`中，使用`sleep`函数周围的忙等待循环。

  - 在`sigchld`处理程序中，只调用一次`waitpid`。

  虽然还有其他可能的解决方案，比如在waitfg和sigchld处理程序中都调用waitpid，但这可能非常令人困惑。在处理程序中完成所有的收集工作更简单。

- 在`eval`中，父进程在`fork`子进程之前必须使用`sigprocmask`阻塞`SIGCHLD`信号，然后在将子进程添加到作业列表中时，通过调用`addjob`后再解除阻塞。由于子进程继承了其父进程的阻塞向量，因此子进程在执行新程序之前必须确保解除阻塞`SIGCHLD`信号。

  

  父进程以这种方式阻塞`SIGCHLD`信号，以避免在父进程调用`addjob`之前，子进程被`sigchld`处理程序收回（从而从作业列表中移除）的竞争条件。

- 诸如`more`、`less`、`vi`和`emacs`之类的程序会对终端设置进行奇怪的操作。不要从您的Shell中运行这些程序。坚持使用简单的基于文本的程序，例如`/bin/ls`、`/bin/ps`和`/bin/echo`。

- 当您从标准Unix Shell运行您的Shell时，您的Shell正在运行在前台进程组中。如果您的Shell创建了一个子进程，默认情况下，该子进程也将成为前台进程组的成员。由于键入ctrl-c会向前台组中的每个进程发送SIGINT信号，因此键入ctrl-c将向您的Shell发送SIGINT信号，以及您的Shell创建的每个进程，这显然是不正确的。

  

  以下是解决方法：

  在fork之后但在execve之前，子进程应调用setpgid(0, 0)，这将子进程放入一个新的进程组，其组ID与子进程的PID相同。这确保前台进程组中只有一个进程，即您的Shell。当您键入ctrl-c时，Shell应捕获生成的SIGINT信号，然后将其转发给相应的前台作业（更确切地说，包含前台作业的进程组）。

  

  ## 评估

  根据以下分配，您的得分将计为最高90分中的一部分：

  1. 80分 正确性：

  2. 16个跟踪文件，每个文件5分。
  3. 10分 风格分。

  我们期望您有良好的注释（5分），并检查每个系统调用的返回值（5分）。

  

  您的解决方案将在Linux机器上进行正确性测试，使用与实验目录中包含的Shell驱动程序和跟踪文件相同的工具。您的Shell应在这些跟踪文件上产生与参考Shell完全相同的输出，只有两个例外：
  • PID可能会不同（会变化）。
  • trace11.txt、trace12.txt和trace13.txt中/bin/ps命令的输出会因每次运行而有所不同。然而，在/bin/ps命令的输出中，mysplit进程的运行状态应保持一致。
