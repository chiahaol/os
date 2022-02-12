Documentation for Warmup Assignment 2
=====================================

+------------------------+
| BUILD & RUN (Required) |
+------------------------+

Replace "(Comments?)" below with the command the grader should use to compile
your program (it should simply be "make" or "make warmup2"; minor variation is
also fine).

    To compile your code, the grader should type: make warmup2

If you have additional instruction for the grader, replace "(Comments?)" with your
instruction (or with the word "none" if you don't have additional instructions):

    Additional instructions for building/running this assignment: none

In the above, it may be a good idea to tell the grader how many virtual CPUs to
use when grading.  The only acceptable number of virtual CPUs are either 1 or 2.

+-------------------------+
| SELF-GRADING (Required) |
+-------------------------+

Replace each "?" below with a numeric value:

(A) Basic running of the code : 90 out of 90 pts

(B) Basic handling of <Cntrl+C> : 10 out of 10 pts
    (Please note that if you entered 0 for (B) above, it means that you did not
    implement <Cntrl+C>-handling and the grader will simply deduct 12 points for
    not handling <Cntrl+C>.  But if you enter anything other than 0 above, it
    would mean that you have handled <Cntrl+C>.  In this case, the grader will
    grade accordingly and it is possible that you may end up losing more than
    12 points for not handling <Cntrl+C> correctly!)

Missing required section(s) in README file : -0 pts
Submission in wrong file format : -0 pts
Submitted binary file : -0 pts
Cannot compile : -0 pts
Compiler warnings : -0 pts
"make clean" : -0 pts
Segmentation faults : -0 pts
Separate compilation : -0 pts

Delay trace printing : -0 pts
Using busy-wait : -0 pts
Trace output :
    1) regular packets: -0 pts
    2) dropped packets: -0 pts
    3) removed packets: -0 pts
    4) token arrival (dropped or not dropped): -0 pts
    5) monotonically non-decreasing timestamps: -0 pts
Remove packets : -0 pts
Statistics output :
    1) inter-arrival time : -0 pts
    2) service time : -0 pts
    3) number of packets in Q1 : -0 pts
    4) number of packets in Q2 : -0 pts
    5) number of packets at a server : -0 pts
    6) time in system : -0 pts
    7) standard deviation for time in system : -0 pts
    8) drop probability : -0 pts
Output bad format : -0 pts
Output wrong precision for statistics (must be 6 or more significant digits) : -0 pts
Statistics in wrong units (time related statistics must be in seconds) : -0 pts
Large total number of packets test : -0 pts
Large total number of packets with high arrival rate test : -0 pts
Dropped tokens and packets test : -0 pts
<Ctrl+C> is handled but statistics are way off : -0 pts
Cannot stop (or take too long to stop) packet arrival thread when required : -0 pts
Cannot stop (or take too long to stop) token depositing thread when required : -0 pts
Cannot stop (or takes too long to stop) a server thread when required : -0 pts
Not using condition variables : -0 pts
Synchronization check : -0 pts
Deadlocks/freezes : -0 pts
Bad commandline or command : -0 pts

+---------------------------------+
| BUGS / TESTS TO SKIP (Required) |
+---------------------------------+

Are there are any tests mentioned in the grading guidelines test suite that you
know that it's not working and you don't want the grader to run it at all so you
won't get extra deductions, please replace "(Comments?)" below with your list.
(Of course, if the grader won't run such tests in the plus points section, you
will not get plus points for them; if the garder won't run such tests in the
minus points section, you will lose all the points there.)  If there's nothing
the grader should skip, please replace "(Comments?)" with "none".

Please skip the following tests: none

+--------------------------------------------------------------------------------------------+
| ADDITIONAL INFORMATION FOR GRADER (Optional, but the grader should read what you add here) |
+--------------------------------------------------------------------------------------------+

+-----------------------------------------------+
| OTHER (Optional) - Not considered for grading |
+-----------------------------------------------+

Comments on design decisions: 
(1) While packet thread is done, if Q1 is empty, cancel token thread and then call broadcast to wake up server threads.
(2) Before generating a token, check if there are still packets waiting for tokens, or if ctrl+c is pressed,
    because it might be waiting for the mutex, and can't be cancelled immediately. If so, give up the mutex and exit the thread.
(3) Always broadcast server threads whenever a packet is sent to Q2.
(4) When server thread wakes up, it will check if there are no packets left to serve or if ctrl+c is pressed.
    If so, exit the thread, else serve a packet. 
(5) When Ctrl + C is pressed, signal thread captures a SIGINT signal, it will cancel both packet and token threads, and then wake up server threads.
    Server thread will check the terminate condition immediately or after it finish serving the packet, and terminate itself.
(6) sigwait() requires the signals that it is waiting to be blocked 
(7) Check if ctrl+c is pressed right after packet/token thread attains mutex_lock, because it might happen a race condition such that
    ctrl+c happened to be pressed when packet/token thread is waiting in the mutex_lock function. In this case, when the thread got out of 
    the mutex_lock, it will still run until it runs to usleep() function.
    However, when the thread don't need to sleep at all (sleep 0 usec), it will not meet any cancellation point, so it will keep running.
    Though printf() is supposed to be a cancellation point, however seems like it didn't act like it from my observation.
(8) Enable cancellation before usleep() and disable it after usleep returns, because it's safer to make sure that we can't cancel the thread
    during the thread is in a mutex lock, otherwise, it might exit without unlocking the mutex.
