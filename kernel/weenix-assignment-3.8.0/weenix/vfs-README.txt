Documentation for Kernel Assignment 2
=====================================

+------------------------+
| BUILD & RUN (Required) |
+------------------------+

Replace "(Comments?)" with the command the grader should use to compile your kernel (should simply be "make").
    To compile the kernel, the grader should type: (Comments?)
If you have additional instruction for the grader, replace "(Comments?)" with your instruction (or with the word "none" if you don't have additional instructions):
    Additional instructions for building/running this assignment: (Comments?)

+-------------------------+
| SELF-GRADING (Required) |
+-------------------------+

Replace each "(Comments?)" with appropriate information and each stand-alone "?"
with a numeric value:

(A.1) In fs/vnode.c:
    (a) In special_file_read(): ? out of 3 pts
    (b) In special_file_write(): ? out of 3 pts

(A.2) In fs/namev.c:
    (a) In lookup(): ? out of 6 pts
    (b) In dir_namev(): ? out of 10 pts
    (c) In open_namev(): ? out of 2 pts

(A.3) In fs/vfs_syscall.c:
    (a) In do_write(): ? out of 4 pts
    (b) In do_mknod(): ? out of 2 pts
    (c) In do_mkdir(): ? out of 2 pts
    (d) In do_rmdir(): ? out of 2 pts
    (e) In do_unlink(): ? out of 2 pts
    (f) In do_stat(): ? out of 2 pts

(B) vfstest: ? out of 42 pts
    What is your kshell command to invoke vfstest_main(): (Comments?)

(C.1) faber_fs_thread_test (? out of 6 pts)
    What is your kshell command to invoke faber_fs_thread_test(): (Comments?)
(C.2) faber_directory_test (? out of 4 pts)
    What is your kshell command to invoke faber_directory_test(): (Comments?)

(D) Self-checks: (? out of 10 pts)
    Please provide details, add subsections and/or items as needed; or, say that "none is needed".
    Details: (Comments?)

Missing/incomplete required section(s) in README file (vfs-README.txt): (-? pts)
Submitted binary file : (-? pts)
Submitted extra (unmodified) file : (-? pts)
Wrong file location in submission : (-? pts)
Extra printout when running with DBG=error,test in Config.mk : (-? pts)
Incorrectly formatted or mis-labeled "conforming dbg() calls" : (-? pts)
Cannot compile : (-? pts)
Compiler warnings : (-? pts)
"make clean" : (-? pts)
Kernel panic : (-? pts)
Kernel freezes : (-? pts)
Cannot halt kernel cleanly : (-? pts)

+--------------------------------------+
| CONTRIBUTION FROM MEMBERS (Required) |
+--------------------------------------+

1)  Names and USC e-mail addresses of team members: ?
2)  Is the following statement correct about your submission (please replace
        "(Comments?)" with either "yes" or "no", and if the answer is "no",
        please list percentages for each team member)?  "Each team member
        contributed equally in this assignment": (Comments?)

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

Please skip the following tests: (Comments?)

+--------------------------------------------------------------------------------------------+
| ADDITIONAL INFORMATION FOR GRADER (Optional, but the grader should read what you add here) |
+--------------------------------------------------------------------------------------------+

+-----------------------------------------------+
| OTHER (Optional) - Not considered for grading |
+-----------------------------------------------+

Comments on design decisions: (Comments?)
